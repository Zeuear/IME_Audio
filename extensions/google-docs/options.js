const input = document.getElementById('token');
chrome.storage.local.get('token').then((v) => { input.value = v.token || ''; });
document.getElementById('save').onclick = async () => {
  await chrome.storage.local.set({ token: input.value.trim() });
  document.getElementById('msg').textContent = '已保存';
};
