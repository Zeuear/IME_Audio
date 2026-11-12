// 弹窗：显示连接与绑定状态并切换绑定；真正的绑定逻辑在 background.js
const $ = (id) => document.getElementById(id);
const ask = (msg) => chrome.runtime.sendMessage(msg);

async function currentTab() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  return tab;
}

const LINK = {
  nokey: ['● 缺少令牌文件', '#c80'],
  off: ['● 未连接 ImeAudio', '#c00'],
  wait: ['● 正在配对…', '#c80'],
  ready: ['● 已连接 ImeAudio', '#080'],
  denied: ['● 令牌已失效', '#c00'],
};

async function render() {
  const st = await ask({ type: 'status' });
  const tab = await currentTab();
  const here = st.boundTabId === tab.id;
  $('status').textContent = here ? '当前标签页已绑定，听写会写入这个文档。'
    : st.boundTabId !== null ? '另一个标签页已绑定。' : '当前标签页未绑定。';
  const [linkText, linkColor] = LINK[st.link] || LINK.off;
  $('link').textContent = linkText;
  $('link').style.color = linkColor;
  $('recv').textContent = st.recvCount
    ? `已收到 ${st.recvCount} 段文字，最近一段 ${Math.round((Date.now() - st.lastRecvAt) / 1000)} 秒前（${st.lastRecvLen} 字）`
    : '还没有收到过文字';
  $('err').textContent = st.lastError;
  $('err').hidden = !st.lastError || st.link === 'ready';
  $('toggle').textContent = here ? '解除绑定' : '绑定当前标签页';
}

$('toggle').onclick = async () => {
  await ask({ type: 'toggle', tab: await currentTab() });
  window.close();
};

render();
