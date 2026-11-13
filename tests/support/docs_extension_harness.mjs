// 在 Node 里运行真实的 extensions/google-docs/background.js，Chrome 接口全部换成假的，
// 连真实的 DocsBridge，并随机注入故障：断开 WebSocket、重启后台脚本、写入文档失败。
// 每写进"文档"一句就以 JSON 行追加到 --out 文件，供 C++ 测试核对。stdin 收到 quit 或关闭时退出。
//
// 用法：node docs_extension_harness.mjs --script <background.js> --port <n> --token <t> --out <file>
//       [--seed n] [--drop r] [--restart r] [--fail r]   r 为每 25ms 一次检查的触发概率 / 每句写入失败概率

import fs from 'node:fs';
import vm from 'node:vm';

const args = Object.fromEntries(process.argv.slice(2).reduce((acc, v, i, a) => {
  if (v.startsWith('--')) acc.push([v.slice(2), a[i + 1]]);
  return acc;
}, []));
const ORIGIN = 'chrome-extension://hhpgdlkkbdnlilodnjipmnfbmcpbfeak';
const DOC_URL = 'https://docs.google.com/document/d/soak/edit';
const dropRate = Number(args.drop ?? 0.02);
const restartRate = Number(args.restart ?? 0.01);
const failRate = Number(args.fail ?? 0.005);
const debug = !!process.env.HARNESS_DEBUG;

// 固定种子的伪随机数，失败时可按种子复现
let seed = Number(args.seed ?? 1) >>> 0 || 1;
const rand = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 0x100000000);

const source = fs.readFileSync(args.script, 'utf8').replace('ws://127.0.0.1:38617', `ws://127.0.0.1:${args.port}`);
fs.writeFileSync(args.out, '');

// 跨后台脚本重启保留的浏览器状态
const sessionStore = {};
const docTab = { id: 7, windowId: 1, title: 'Soak - Google Docs' };
let attached = false;
let insertsInFlight = 0;
const stats = { written: 0, drops: 0, restarts: 0, insertFailures: 0, workers: 0 };

const clone = (v) => (v === undefined ? undefined : JSON.parse(JSON.stringify(v)));

function makeWorker() {
  stats.workers++;
  const timers = new Set();
  const sockets = new Set();
  const listeners = { message: [], alarm: [], startup: [], removed: [], detach: [] };
  const on = (name) => ({ addListener: (fn) => listeners[name].push(fn) });

  class MockWebSocket extends WebSocket {
    constructor(url) {
      super(url, { headers: { origin: ORIGIN } });
      sockets.add(this);
      this.addEventListener('close', () => sockets.delete(this));
      if (debug) {
        this.addEventListener('message', (ev) => {
          const m = JSON.parse(ev.data);
          console.error(`[w${stats.workers}] recv ${m.type}${m.id ? ' id=' + m.id : ''}`);
        });
        const send = this.send.bind(this);
        this.send = (d) => { const m = JSON.parse(d); if (m.type !== 'ping') console.error(`[w${stats.workers}] send ${m.type}${m.id ? ' id=' + m.id : ''}`); send(d); };
      }
    }
  }

  const chrome = {
    runtime: {
      getURL: (p) => `mock://${p}`,
      onMessage: on('message'),
      onStartup: on('startup'),
    },
    storage: {
      session: {
        get: async (keys) => Object.fromEntries([].concat(keys).filter((k) => k in sessionStore).map((k) => [k, clone(sessionStore[k])])),
        set: async (obj) => { for (const [k, v] of Object.entries(obj)) sessionStore[k] = clone(v); },
        remove: async (keys) => { for (const k of [].concat(keys)) delete sessionStore[k]; },
      },
      local: { remove: async () => {} },
    },
    tabs: {
      get: async (id) => { if (id !== docTab.id) throw new Error('No tab with id: ' + id); return docTab; },
      query: async () => [docTab],
      create: async () => docTab,
      onRemoved: on('removed'),
    },
    windows: { update: async () => {} },
    action: { setBadgeText: () => {}, setBadgeBackgroundColor: () => {} },
    alarms: { create: () => {}, onAlarm: on('alarm') },
    debugger: {
      attach: async ({ tabId }) => {
        if (tabId !== docTab.id) throw new Error('Cannot access a chrome:// URL');
        if (attached) throw new Error('Another debugger is already attached to the tab with id: ' + tabId);
        attached = true;
      },
      detach: async () => {
        if (!attached) throw new Error('Debugger is not attached to the tab');
        attached = false;
      },
      sendCommand: async ({ tabId }, method, params) => {
        if (!attached || tabId !== docTab.id) throw new Error('Debugger is not attached to the tab with id: ' + tabId);
        if (method === 'Runtime.evaluate') return { result: { value: [DOC_URL, docTab.title] } };
        if (method !== 'Input.insertText') return {};
        insertsInFlight++;
        try {
          // 偶尔让出事件循环，让写入和断线、重启交错；不用定时器，Windows 上定时器至少 1ms
          if (rand() < 0.2) await new Promise((r) => setImmediate(r));
          if (rand() < failRate) {
            stats.insertFailures++;
            throw new Error('Mock insert failure');
          }
          stats.written++;
          fs.appendFileSync(args.out, JSON.stringify(params.text) + '\n');
          return {};
        } finally {
          insertsInFlight--;
        }
      },
      onDetach: on('detach'),
    },
  };

  const track = (fn, clear) => (...a) => { const t = fn(...a); timers.add([t, clear]); return t; };
  const sandbox = {
    chrome,
    WebSocket: MockWebSocket,
    fetch: async () => ({ json: async () => ({ token: args.token }) }),
    console: debug ? { log: (...a) => console.error(`[w${stats.workers}]`, ...a), warn: console.error, error: console.error } : { log: () => {}, warn: () => {}, error: () => {} },
    setTimeout: track(setTimeout, clearTimeout),
    setInterval: track(setInterval, clearInterval),
    clearTimeout, clearInterval, Promise, JSON, Date, Math, Error,
  };
  vm.createContext(sandbox);
  vm.runInContext(source, sandbox, { filename: 'background.js' });

  return {
    fireAlarm: () => listeners.alarm.forEach((fn) => fn({ name: 'reconnect' })),
    dropSocket: () => { for (const s of sockets) s.close(); },
    kill: () => {
      for (const [t, clear] of timers) clear(t);
      for (const s of sockets) s.close();
    },
  };
}

let worker = makeWorker();
let lastRestartAt = 0;

// Chrome 只在后台脚本空闲时停掉它，所以只在没有写入进行中时模拟重启
setInterval(() => {
  if (rand() < dropRate) { stats.drops++; worker.dropSocket(); }
  if (insertsInFlight === 0 && Date.now() - lastRestartAt > 200 && rand() < restartRate) {
    stats.restarts++;
    lastRestartAt = Date.now();
    worker.kill();
    worker = makeWorker();
  }
}, 25);
// 真实的 alarm 每 30 秒一次，这里加快
setInterval(() => worker.fireAlarm(), 100);

function quit() {
  process.stderr.write(`harness stats ${JSON.stringify(stats)}\n`);
  process.exit(0);
}
process.stdin.on('data', (d) => { if (String(d).includes('quit')) quit(); });
process.stdin.on('end', quit);
