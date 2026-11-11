// 弹窗：没有配对码时让用户粘贴，有了就显示绑定状态并切换；真正的绑定逻辑在 background.js
const $ = (id) => document.getElementById(id);
const ask = (msg) => chrome.runtime.sendMessage(msg);

async function currentTab() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  return tab;
}

async function render(forcePair) {
  const st = await ask({ type: 'status' });
  const tab = await currentTab();
  const needPair = forcePair || !st.hasToken;
  $('pair').hidden = !needPair;
  $('main').hidden = needPair;
  if (needPair) return;
  const here = st.boundTabId === tab.id;
  $('status').textContent = here ? '当前标签页已绑定，听写会写入这个文档。'
    : st.boundTabId !== null ? '另一个标签页已绑定。' : '当前标签页未绑定。';
  $('toggle').textContent = here ? '解除绑定' : '绑定当前标签页';
}

$('toggle').onclick = async () => {
  await ask({ type: 'toggle', tab: await currentTab() });
  window.close();
};

$('save').onclick = async () => {
  const token = $('token').value.trim();
  if (!token) return;
  await ask({ type: 'setToken', token });
  await ask({ type: 'toggle', tab: await currentTab(), forceBind: true });
  window.close();
};

$('repair').onclick = () => render(true);
render(false);
