# AGENTS.md — ImeAudio 编码约定

Qt6 桌面语音输入法（Windows / macOS / Linux）。领域术语见 [CONTEXT.md](CONTEXT.md)，设计决策见 [docs/adr/](docs/adr/)。
新增代码必须贴合现有风格；下面每条都来自项目里已有的做法。

## 界面（UI）

- **标签页/页面写在 [src/MainWin.ui](src/MainWin.ui) 里**，不要在 C++ 里 `addTab` 或手工拼主窗口页面。
- 自定义控件在 .ui 中**提升（`<customwidget>`）**，参照 `TermWidget`、`NavListWidget`：
  - 控件提供 `explicit X(QWidget* parent = nullptr)` 默认构造，内部 `setupUi()` 用代码搭内部布局。
  - 依赖（服务、配置）由 `MainWin` 通过 setter 注入，例如 `ui->terms_widget->setTermsManager(...)`、
    `ui->file_transcribe_widget->setServices(...)`，写在 `MainWin::setupUiConnections()` 里。
- 控件对象名用 `snake_case`（`terms_widget`、`identification_log_edit`）。
- 自定义控件放 [src/widgets/](src/widgets/)。
- 列表类界面用 **Model + Delegate**：状态放 `QAbstractListModel`（如 `FileTaskModel`），
  卡片绘制放 `QStyledItemDelegate`（如 `FileTaskDelegate`），控件只负责编排。
  颜色取 `palette`，保证 light / gray / dark 主题都可读；只有成功/失败使用固定语义色。

## 错误通知

- 用户可见的错误统一走 `errorOccurred(title, cause)` 信号 → `MainWin::notify(...)` 弹窗，
  **title/cause 用中文**。不要在子模块里直接 `QMessageBox`。
- 详见 [docs/adr/global-error-notify-unified.md](docs/adr/global-error-notify-unified.md)。
- 排查用的信息写日志：`LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR`（[src/utils/Logger.h](src/utils/Logger.h)），
  日志用英文。

## 多语言

- 界面文字用 `tr("English source")`，**源串用英文**；中文翻译写进
  [resources/translations/zh_CN.ts](resources/translations/zh_CN.ts)，英文对照写进 `qt_EN.ts`。
- `.ui` 里的标题同样是英文（如 `Theaurus`、`File Transcription`），翻译放在 `MainWin` 上下文里。
- 更新翻译用 [updateTS.bat](updateTS.bat)（`lupdate ... -no-obsolete`）。
- 例外：传给 `notify` 的 title/cause 和抛给用户的错误原因是中文字面量。

## 架构分层

- 依赖方向：`MainWin` → `WorkflowManager` → `IRecorder` / `ITranscription` / `ISherpaModel`
  （[src/interfaces/workflow_interfaces.h](src/interfaces/workflow_interfaces.h)）。
  工作流只依赖接口，便于测试注入假实现。
- 音频内部统一 **16-bit / 16 kHz / mono PCM**，云端后端才封装 WAV（`buildWavBytes`）。
- `TranscriptionService` 是统一转录入口，按 `AsrBackendKind` 分发（Sherpa / Groq / Gladia / Gemini）。
  后处理（术语替换、标点）经 `postProcess`，新功能复用它，不要另起一套。
- **听写结果会被注入前台窗口**。任何非听写来源（如文件转录）必须走独立通道
  （`TranscriptionService::setFileMode` + `fileSegmentFinished`），否则结果会被打进别的软件。
- 模型加载/转录任务在 `SherpaManager` 的 worker 线程队列里**串行**执行；不要在界面线程直接调识别器。

## 跨平台

- 平台相关代码放 `src/platform/{win,mac,linux}/`，每个平台一份实现，
  由 [src/CMakeLists.txt](src/CMakeLists.txt) 按平台挑选（递归 glob 会排除 `/platform/`）。
  新增平台接口：在公共头声明，三个目录各写一份（mac 用 `.mm`）。
- `SystemAudioEndpointController` 的 COM hack 是为 VoiceMeeter 虚拟声卡存在，**Windows 专属，不要移植**。
- **sherpa-onnx**：官方预编译库在 Linux 上与 C++ API 有 ABI 不匹配（`AddPunctuation` 链接失败）。
  标点模型走 `c-api.h` 的纯 C 接口，不要改回 C++ 封装，也不要靠切 `_GLIBCXX_USE_CXX11_ABI` 解决。
  其余识别/VAD 用 `cxx-api.h` 即可（其它符号已在 CI 通过）。
- 新增源文件不需要改 CMake：`src/` 下 `*.cpp/*.h/*.ui` 都是 `GLOB_RECURSE CONFIGURE_DEPENDS`，重新配置即可。

## 代码风格

- 注释用中文，说明"为什么"；类头用 `// @brief` 简述职责。
- 命名：类 `PascalCase`，方法 `camelCase`，成员 `m_camelCase`，常量 `kPascalCase`。
- 优先使用信号槽与 `Qt::QueuedConnection` 跨线程通信；后台计算用 `QtConcurrent::run` + `QFutureWatcher`。
- 不引入新的第三方依赖前先确认；Qt 模块限于 CMake 已列出的（含 Multimedia、Concurrent）。

## 构建与验证

- 生成器为 Visual Studio（Windows）：`cmake --build build --config Release --target ImeAudio`，
  产物在 `app/Release/`。
- 改完必须编译通过再提交；涉及 UI 的改动需实际运行确认，编译通过不代表界面正常。
- 测试相关决策见 [docs/adr/0001-test-stack-selection.md](docs/adr/0001-test-stack-selection.md)。

## Git

- **直接提交到 `main`**，不要开 feature 分支（其它分支只用于打版本号编译）。
- 提交信息：`type(scope): 中文说明`，如 `fix(linux): 用纯 C 接口绕开标点模型 AddPunctuation 的 ABI 不匹配`。
- 提交由用户明确要求后才执行。
