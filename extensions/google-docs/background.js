// 弹窗里绑定 / 解除当前标签页；绑定后收到 text 就用调试器协议写进该标签页。
// 开启焦点模拟，文档窗口不在前台也能收字。连上后须先发配对码，程序才接受其余消息。

const URL = 'ws://127.0.0.1:38617';
const DEBUGGER_VERSION = '1.3';

let ws = null;
let boundTabId = null;
let titleCache = '';

const log = (...a) => console.log('[ImeAudio]', ...a);
const post = (obj) => { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj)); };
const cmd = (tabId, method, params) => chrome.debugger.sendCommand({ tabId }, method, params || {});

function setBadge(text, color) {
  chrome.action.setBadgeText({ text });
  if (color) chrome.action.setBadgeBackgroundColor({ color });
}

async function connect() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;
  const { token } = await chrome.storage.local.get('token');
  if (!token) { setBadge('KEY', '#c80'); return; }
  try { ws = new WebSocket(URL); } catch (_) { return; }
  ws.onopen = () => {
    post({ type: 'hello', token });
    if (boundTabId !== null) post({ type: 'bound', title: titleCache });
  };
  ws.onmessage = async (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch (_) { return; }
    if (msg.type === 'text' && boundTabId !== null) {
      try {
        await cmd(boundTabId, 'Input.insertText', { text: msg.text });
      } catch (e) {
        log('insertText failed', e.message);
        await unbind();
      }
    }
  };
  ws.onclose = () => { ws = null; };
  ws.onerror = () => {};
}

async function bind(tab) {
  if (boundTabId !== null) await unbind();
  try {
    await chrome.debugger.attach({ tabId: tab.id }, DEBUGGER_VERSION);
    await cmd(tab.id, 'Emulation.setFocusEmulationEnabled', { enabled: true });
  } catch (e) {
    log('attach failed', e.message);
    setBadge('ERR', '#c00');
    return;
  }
  boundTabId = tab.id;
  titleCache = (tab.title || '').replace(/ - Google (文档|Docs)$/i, '');
  setBadge('ON', '#080');
  await connect();
  post({ type: 'bound', title: titleCache });
}

async function unbind() {
  const id = boundTabId;
  boundTabId = null;
  setBadge('');
  if (id !== null) {
    try { await cmd(id, 'Emulation.setFocusEmulationEnabled', { enabled: false }); } catch (_) {}
    try { await chrome.debugger.detach({ tabId: id }); } catch (_) {}
  }
  post({ type: 'unbound' });
}

// 点图标弹出 popup.html，由它发来消息；不再有 onClicked
chrome.runtime.onMessage.addListener((msg, _sender, reply) => {
  (async () => {
    if (msg.type === 'status') {
      const { token } = await chrome.storage.local.get('token');
      reply({ hasToken: !!token, boundTabId });
    } else if (msg.type === 'setToken') {
      await chrome.storage.local.set({ token: msg.token });
      reply({});
    } else if (msg.type === 'toggle') {
      if (boundTabId === msg.tab.id && !msg.forceBind) await unbind(); else await bind(msg.tab);
      reply({});
    }
  })();
  return true;   // 异步回复
});

chrome.tabs.onRemoved.addListener((id) => { if (id === boundTabId) unbind(); });
chrome.debugger.onDetach.addListener((src) => {
  if (src.tabId === boundTabId) { boundTabId = null; setBadge(''); post({ type: 'unbound' }); }
});

// 配对码改了就断开重连
// 仍有绑定时保留 ON 角标，不能一律清空
chrome.storage.onChanged.addListener(() => {
  if (ws) ws.close();
  ws = null;
  setBadge(boundTabId !== null ? 'ON' : '', '#080');
  connect();
});

chrome.alarms.create('reconnect', { periodInMinutes: 0.5 });
chrome.alarms.onAlarm.addListener(connect);
chrome.runtime.onStartup.addListener(connect);
connect();
