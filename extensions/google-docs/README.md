# ImeAudio Google 文档输出扩展

扩展随程序分发，不单独打包：本目录的文件经 `src/extension.qrc` 嵌进程序，新增文件要同时加进去。
程序侧代码在 `src/services/docs/`（引导窗口在 `src/ui/controls/DocsSetupDialog`）。

1. ImeAudio 横条右键菜单点"Google 文档..."，弹出引导窗口，同时把扩展导出到数据目录的 `extensions/google-docs`。
   令牌写进导出目录的 `pairing.json`，程序配置里只存它的 SHA-256；已导出的令牌仍有效时沿用。
   本目录没有 `pairing.json`，开发时也从导出目录加载。
2. 按窗口里的步骤在 `chrome://extensions` 加载导出目录。扩展连上后窗口自动打勾。
3. 在窗口里点"新建文档"或"用 Chrome 当前标签页"，程序命令扩展绑定；也可以在 Chrome 里点扩展图标绑定。
   只能绑定 `https://docs.google.com/document/` 下的页面。
4. 绑定后听写的文字写进该文档，不再注入前台窗口。绑定期间连接意外断开也不注入前台，
   直到重新绑定或在窗口里点"停止输出"。

绑定期间 Chrome 会显示"正在调试此浏览器"横幅，这是调试器协议的固有提示，不要点"取消"。
扩展 ID 由 manifest 的 `key` 固定，程序只接受这个 ID，并校验令牌。
