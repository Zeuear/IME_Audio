# CONTEXT.md — ImeAudio 领域术语表

> 纯 glossaries，不含实现细节。更新时机：每当一个术语在 grilling 会话中被确认。

## 录音 (AudioCapture)

- **AudioRecorderService** — 管理麦克风捕获、VAD 分段、频谱可视化的 Qt 服务类。
  不是录音文件（AudioFile），而是实时流式采集。
- **VAD (Voice Activity Detection)** — 语音活动检测。本项目使用 sherpa-onnx
  的 Silero VAD。用于判断"当前有人在说话"，驱动录音段的开始/结束。
  VAD ≠ ASR：VAD 仅检测有无语音，ASR 负责识别内容。
- **SpeechSegment** — VAD 检测到语音段后，由 `VadWorker` 输出的、裁剪好的 PCM
  音频块。每个 SpeechSegment 代表一句话(utterance)。
- **PCM (Pulse-Code Modulation)** — 未压缩音频格式。本项目内部统一使用
  16-bit little-endian, 16kHz, mono。外部 API(Groq/Gladia) 需要 WAV 封装。

## 转录 (ASR / SpeechRecognition)

- **ASR (Automatic Speech Recognition)** — 自动语音识别。本项目支持多种后端，
  通过 `AsrBackendKind` 枚举切换。
- **ASR 后端 (Backend)** — 识别引擎的具体实现：
  - `Sherpa` — 本地 onnxruntime (离线或在线流式)
  - `Gemini` — Google Gemini API (音频+文本一体)
  - `Groq` — Groq Whisper API (仅音频)
  - `Gladia` — Gladia API (仅音频)
- **TranscriptionResult** — 一次 ASR 调用的完整结果，包含 `rawText` (原始识别)、
  `finalText` (后处理后)、`errorMsg`。
- **postProcess** — 将 rawText 转为 finalText 的步骤：应用术语替换 + 自动加标点。

## 润色 (Polish / LLMRefinement)

- **Polish** — LLM (Gemini 或 OpenAI-compatible) 对转录文本的优化：去除口语词、
  修正自我修正、结构化整理、按样式重写。
- **AI 引擎 (aiEngine)** — `0 = Gemini`, `1 = OpenAI-compatible (本地/自訂)`, `2 = Ollama (本地)`。
  影响 API 端点和请求体构造。不是 ASR 后端，是后处理 LLM。`2 = Ollama` 复用 OpenAI-compatible
  的 `/v1/chat/completions` 路径（`isOllama` 标志），仅 base URL 指向本地 Ollama（默认
  `http://localhost:11434`），无需 API key。
- **TextPolishProvider** — 润色后端抽象接口（`src/textpolish/`）：`GeminiProvider` 走 Gemini
  `generateContent`，`OpenAiProvider` 走 OpenAI 兼容（含本地 Ollama）。`TextPolishService` 通过
  `createTextPolishProvider(params)` 工厂按 `aiEngine`/`isOllama` 选择实现，共享 prompt 逻辑在
  `TextPolishPrompts`。
- **Style** — 润色的语气风格：`商務正式`, `日常口語`, `簡潔扼要`, `自訂 Prompt`。
  存储为 prompts/*.txt，由 `styleInstruction` 加载。
- **Replace Rules (replaceRules)** — 用户定义的词语替换规则，格式为
  `wrong=>correct` 用分号分隔。用于 postProcess 和 AI prompt 的字典指令。
  ≠ TermsLibrary（terms.tsv）：replaceRules 是手动输入的扁平字符串，
  TermsLibrary 是结构化 TSV 数据库。

## 输出 (Output / Injection)

- **InputInjector** — 将最终文本注入到活动窗口的类。使用 WinAPI 模拟输入。
- **AutoTool.exe** — 项目的可执行文件名。是 ImeAudio 在桌面环境的落地形态。
- **Continuous Mode** — 连续录音模式：VAD 检测到语音自动分段，每次 segment
  立即转录。≠ Manual Mode：手动按键开始/停止一次录音。

## 术语库 (TermsLibrary)

- **TermItem** — 术语条目：`wrong` (错误写法/发音)、`correct` (正确写法)、
  `aliases` (同义词/谐音)、`mode` (all/replace/ai/hotword)、`tags`, `notes`。
- **mode** — "all" 表示所有场合使用；"replace" 表示在 postProcess 中直接替换；
  "ai" 表示传给 LLM 作为字典；"hotword" 表示传给 ASR 热词。一个 TermItem 可以
  同时属于多个 mode。
- **applyReplaceRules** — 静态方法：解析 `wrong=>correct;...` 字符串，按 wrong
  长度降序排序，占位符法一次性替换，避免二次替换。
- **buildAllRules** — 从 TSV 构建 replaceRules + aiVocab + hotwords 三类规则串。

## 性能 (Performance / Resource)

- **资源占用基线 (Resource Baseline)** — CPU/内存的硬性阈值，用于 CI 回归检测。
  例如"待机状态下 ≤ XX MB"。每次 PR 需通过基线检查才合并。
- **Baseline Template** — 针对特定使用场景调校的 ASR/后端参数集合。例如
  "Meeting" (高准确度, 长上下文), "Email" (简洁风格), "CodeComment" (技术词表)。
  用于降低新用户配置门槛。
