// 双光标验证扩展（仅测试）。
// 用法：点击工具栏图标 → 对"当前标签页"挂上调试器 → 开启焦点模拟 → 依次用两种方式写入几句话：
//   A 段 Input.insertText          （像输入法提交一整段文字）
//   B 段 Input.dispatchKeyEvent    （逐字符发 char 事件，像键盘输入）
// 写入期间你可以切到别的窗口打字。结果以角标和 Service Worker 控制台日志显示。
// 不开任何本地端口；调试器只挂在被点击的这一个标签页上，跑完自动卸下。

const TEXT = '语音测试 voice test, 标点。';
const ROUNDS = 3;
const GAP_MS = 2500;
// 点击图标后先等这么久再挂调试器，留时间把窗口最小化/切走，模拟"语音窗口已在后台"的情形
const START_DELAY_S = 10;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const log = (...a) => console.log('[probe]', ...a);

function send(target, method, params) {
  return chrome.debugger.sendCommand(target, method, params || {});
}

async function typeByKeyEvents(target, text) {
  for (const ch of text) {
    await send(target, 'Input.dispatchKeyEvent', {
      type: 'keyDown', key: ch, text: ch, unmodifiedText: ch,
    });
    await send(target, 'Input.dispatchKeyEvent', { type: 'keyUp', key: ch });
  }
}

async function run(tab) {
  const target = { tabId: tab.id };
  const badge = (t, c) => {
    chrome.action.setBadgeText({ tabId: tab.id, text: t });
    if (c) chrome.action.setBadgeBackgroundColor({ tabId: tab.id, color: c });
  };

  for (let s = START_DELAY_S; s > 0; s--) {
    badge(String(s), '#888');
    await sleep(1000);
  }

  try {
    await chrome.debugger.attach(target, '1.3');
  } catch (e) {
    log('attach 失败', e.message);
    badge('ERR', '#c00');
    return;
  }

  try {
    // 让页面以为自己有焦点，这样窗口不在前台也能收到输入
    await send(target, 'Emulation.setFocusEmulationEnabled', { enabled: true });
    log('焦点模拟已开启，开始写入。现在可以切到别的窗口打字。');

    for (let i = 1; i <= ROUNDS; i++) {
      badge('A' + i, '#06c');
      await send(target, 'Input.insertText', { text: `【A${i}】${TEXT}` });
      log(`A${i} insertText 已发送`);
      await sleep(GAP_MS);
    }
    for (let i = 1; i <= ROUNDS; i++) {
      badge('B' + i, '#609');
      await typeByKeyEvents(target, `【B${i}】${TEXT}`);
      log(`B${i} keyEvents 已发送`);
      await sleep(GAP_MS);
    }
    badge('OK', '#080');
  } catch (e) {
    log('写入出错', e.message);
    badge('ERR', '#c00');
  } finally {
    try { await send(target, 'Emulation.setFocusEmulationEnabled', { enabled: false }); } catch (_) {}
    try { await chrome.debugger.detach(target); } catch (_) {}
    log('已卸下调试器');
  }
}

chrome.action.onClicked.addListener((tab) => { run(tab); });
