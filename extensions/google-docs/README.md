# ImeAudio Google 文档输出扩展

扩展随程序分发，不单独打包：本目录的文件经 `src/extension.qrc` 嵌进程序，用户在横条右键菜单点
"安装 Google 文档扩展"，程序把它导出到数据目录的 `extensions/google-docs` 并打开 [guide.html](guide.html)（给用户看的安装说明）。
新增文件要同时加进 `src/extension.qrc`。下面是开发时直接从源码目录加载的步骤。

1. ImeAudio 横条右键菜单点"安装 Google 文档扩展"。导出时生成新令牌写进导出目录的 `pairing.json`，
   程序配置里只存令牌的 SHA-256。本目录没有 `pairing.json`，开发时也从导出目录加载。
2. Chrome 打开 `chrome://extensions`，开启"开发者模式"，"加载已解压的扩展程序"选导出目录。
3. 打开文档标签页，点扩展图标，点"绑定当前标签页"（角标 ON 即已绑定）。
4. 绑定后听写的文字写进该文档，不再注入当前前台窗口。绑定期间连接意外断开也不注入前台，
   直到重新绑定或在横条菜单点"停止 Google 文档输出"。

绑定期间 Chrome 会显示"正在调试此浏览器"横幅，这是调试器协议的固有提示，不要点"取消"。
扩展 ID 由 manifest 的 `key` 固定，程序只接受这个 ID，并校验令牌。
