// 弹窗里绑定 / 解除当前标签页；绑定后收到 text 就用调试器协议写进该标签页。
// 开启焦点模拟，文档窗口不在前台也能收字。连上后须先发令牌，程序才接受其余消息。
// 令牌由 ImeAudio 导出扩展时写进 pairing.json，每次导出都会换新，所以每次连接前重新读。

const URL = 'ws://127.0.0.1:38617';
const DEBUGGER_VERSION = '1.3';
// 后台脚本空闲约 30 秒会被 Chrome 停掉，WebSocket 上有收发才算活动
const PING_MS = 20000;

let ws = null;
let boundTabId = null;
let titleCache = '';
// 连接状态：nokey 没有令牌文件 / off 未连 / wait 已连上等配对结果 / ready 配对通过 / denied 令牌不对
let link = 'off';
let recvCount = 0;
let lastRecvAt = 0;
let lastRecvLen = 0;
let lastError = '';

const log = (...a) => console.log('[ImeAudio]', ...a);
const post = (obj) => { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj)); };
const cmd = (tabId, method, params) => chrome.debugger.sendCommand({ tabId }, method, params || {});
const DOCS_PREFIX = 'https://docs.google.com/document/';
const cleanTitle = (t) => (t || '').replace(/ - Google (文档|Docs)$/i, '');

// 没有 tabs 权限读不到标签页的地址和标题，调试器附加后从页面里取
async function pageInfo(tabId) {
  const r = await cmd(tabId, 'Runtime.evaluate', { expression: '[location.href, document.title]', returnByValue: true });
  const [url, title] = (r && r.result && r.result.value) || ['', ''];
  return { url, title };
}

function setBadge(text, color) {
  chrome.action.setBadgeText({ text });
  if (color) chrome.action.setBadgeBackgroundColor({ color });
}

// 角标一眼看状态：KEY 令牌缺失或失效，OFF 连不上程序，LINK 已连接未绑定，ON 已连接且已绑定
function refreshBadge() {
  if (link === 'nokey') return setBadge('KEY', '#c80');
  if (link === 'denied') return setBadge('KEY', '#c00');
  if (link !== 'ready') return setBadge('OFF', '#888');
  if (boundTabId !== null) return setBadge('ON', '#080');
  setBadge('LINK', '#06c');
}

async function readToken() {
  try {
    const r = await fetch(chrome.runtime.getURL('pairing.json'), { cache: 'no-store' });
    return (await r.json()).token || '';
  } catch (_) {
    return '';
  }
}

// 后台脚本被停掉再启动时，内存里的绑定没了，从会话存储找回
function saveBinding() {
  return chrome.storage.session.set({ boundTabId, titleCache });
}

async function restoreBinding() {
  const s = await chrome.storage.session.get(['boundTabId', 'titleCache']);
  if (s.boundTabId == null || boundTabId !== null) return;
  const id = s.boundTabId;
  try {
    await chrome.tabs.get(id);
    // 调试器可能还挂着，attach 报"已附加"不算失败，以 sendCommand 能否成功为准
    try { await chrome.debugger.attach({ tabId: id }, DEBUGGER_VERSION); } catch (_) {}
    await cmd(id, 'Emulation.setFocusEmulationEnabled', { enabled: true });
  } catch (e) {
    log('restore binding failed', e.message);
    await chrome.storage.session.remove(['boundTabId', 'titleCache']);
    return;
  }
  boundTabId = id;
  titleCache = s.titleCache || '';
  log('binding restored');
}

async function connect() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;
  const token = await readToken();
  if (!token) {
    link = 'nokey';
    lastError = '没有找到令牌文件：请在 ImeAudio 横条右键菜单点“安装 Google 文档扩展”，并从它打开的文件夹加载扩展';
    refreshBadge();
    log('pairing.json missing, skip connect');
    return;
  }
  if (ws) return;   // 读令牌期间别的调用已经开始连了
  try { ws = new WebSocket(URL); } catch (e) { lastError = e.message; log('WebSocket create failed', e.message); return; }
  ws.onopen = () => {
    link = 'wait';
    lastError = '';
    log('ws open, sending hello');
    post({ type: 'hello', token });
    if (boundTabId !== null) post({ type: 'bound', title: titleCache });
  };
  ws.onmessage = async (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch (_) { return; }
    if (msg.type === 'ready') {
      link = 'ready';
      log('pairing ok');
      refreshBadge();
    } else if (msg.type === 'denied') {
      link = 'denied';
      lastError = '令牌已失效：请在 ImeAudio 里重新点“安装 Google 文档扩展”，再到 chrome://extensions 点本扩展的刷新按钮';
      log('pairing token rejected by ImeAudio');
      refreshBadge();
    } else if (msg.type === 'release') {
      log('ImeAudio asked to release the tab');
      await unbind();
    } else if (msg.type === 'newDoc') {
      await bindNewDoc();
    } else if (msg.type === 'bindActive') {
      const [tab] = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
      const err = tab ? await bind(tab.id, true) : 'notDocs';
      if (err) post({ type: 'bindFailed', reason: err });
    } else if (msg.type === 'text' && boundTabId === null) {
      log('text received but no tab bound, dropped');
    } else if (msg.type === 'text') {
      // 只记次数和长度，不记内容
      recvCount++; lastRecvAt = Date.now(); lastRecvLen = (msg.text || '').length;
      log('text received, length', lastRecvLen);
      try {
        await cmd(boundTabId, 'Input.insertText', { text: msg.text });
      } catch (e) {
        lastError = '写入文档失败：' + e.message;
        log('insertText failed', e.message);
        await unbind();
      }
    }
  };
  // 连不上时 onerror 不带原因，只能提示常见原因；不要打印令牌
  ws.onclose = (ev) => {
    log('ws closed, code', ev.code, 'wasClean', ev.wasClean);
    ws = null;
    if (link !== 'denied') link = 'off';
    refreshBadge();
  };
  ws.onerror = () => {
    lastError = '连不上 ImeAudio：请确认程序正在运行';
    log('ws error: cannot reach', URL, '- is ImeAudio running?');
  };
}

// 返回空串表示成功，否则是失败原因：notDocs 或调试器的错误信息
async function bind(tabId, requireDocs) {
  if (boundTabId === tabId) return '';
  if (boundTabId !== null) await unbind();
  try {
    await chrome.debugger.attach({ tabId }, DEBUGGER_VERSION);
  } catch (e) {
    log('attach failed', e.message);
    return e.message;
  }
  try {
    titleCache = '';
    if (requireDocs) {
      const info = await pageInfo(tabId);
      if (!info.url.startsWith(DOCS_PREFIX)) {
        try { await chrome.debugger.detach({ tabId }); } catch (_) {}
        log('tab is not a Google Doc, not bound');
        return 'notDocs';
      }
      titleCache = cleanTitle(info.title);
    }
    await cmd(tabId, 'Emulation.setFocusEmulationEnabled', { enabled: true });
  } catch (e) {
    try { await chrome.debugger.detach({ tabId }); } catch (_) {}
    log('bind failed', e.message);
    return e.message;
  }
  boundTabId = tabId;
  await saveBinding();
  refreshBadge();
  await connect();
  post({ type: 'bound', title: titleCache });
  return '';
}

// 新文档还在加载，标题要等一会儿才有
async function bindNewDoc() {
  const tab = await chrome.tabs.create({ url: 'https://docs.new', active: true });
  try { await chrome.windows.update(tab.windowId, { focused: true }); } catch (_) {}
  const err = await bind(tab.id, false);
  if (err) { post({ type: 'bindFailed', reason: err }); return; }
  setTimeout(async () => {
    if (boundTabId !== tab.id) return;
    try {
      titleCache = cleanTitle((await pageInfo(tab.id)).title);
      await saveBinding();
      post({ type: 'bound', title: titleCache });
    } catch (_) {}
  }, 4000);
}

async function unbind() {
  const id = boundTabId;
  boundTabId = null;
  await chrome.storage.session.remove(['boundTabId', 'titleCache']);
  refreshBadge();
  if (id !== null) {
    try { await cmd(id, 'Emulation.setFocusEmulationEnabled', { enabled: false }); } catch (_) {}
    try { await chrome.debugger.detach({ tabId: id }); } catch (_) {}
  }
  post({ type: 'unbound' });
}

// 启动时先找回绑定再连接，否则连上后报的绑定状态不对
const ready = restoreBinding().catch(() => {}).then(() => { refreshBadge(); return connect(); });

// 点图标弹出 popup.html，由它发来消息；不再有 onClicked
chrome.runtime.onMessage.addListener((msg, _sender, reply) => {
  (async () => {
    await ready;
    if (msg.type === 'status') {
      reply({ boundTabId, link, recvCount, lastRecvAt, lastRecvLen, lastError });
    } else if (msg.type === 'toggle') {
      if (boundTabId === msg.tab.id) { await unbind(); reply({}); }
      else reply({ error: await bind(msg.tab.id, true) });
    }
  })();
  return true;   // 异步回复
});

chrome.tabs.onRemoved.addListener((id) => { if (id === boundTabId) unbind(); });
chrome.debugger.onDetach.addListener((src) => {
  if (src.tabId !== boundTabId) return;
  boundTabId = null;
  chrome.storage.session.remove(['boundTabId', 'titleCache']);
  refreshBadge();
  post({ type: 'unbound' });
});

setInterval(() => post({ type: 'ping' }), PING_MS);
chrome.alarms.create('reconnect', { periodInMinutes: 0.5 });
chrome.alarms.onAlarm.addListener(() => ready.then(connect));
chrome.runtime.onStartup.addListener(() => ready.then(connect));
// 旧版本把手动粘贴的配对码存在这里，已不再使用
chrome.storage.local.remove('token');
