# ImeAudio

[![License](https://img.shields.io/badge/license-AGPL%20v3-blue.svg)](LICENSE.txt)
[![Platform](https://img.shields.io/badge/platform-Multi-blueviolet.svg)]()

ImeAudio is a high-performance, AI-driven real-time speech transcription and text polishing utility. Designed for seamless workflow integration, it leverages state-of-the-art local and cloud-based speech recognition engines to turn your voice into refined text instantly.

## 🌟 Key Features

<p align="center">
  <img src="resources/screenshots/main-window.png" alt="ImeAudio main window" width="800">
</p>

### 🎙️ Intelligent Transcription
* **Multi-Backend Support:** Choose between local processing with **Sherpa** or high-speed cloud engines like **Gemini**, **Groq**, and **Gladia**.
* **Multilingual Recognition:** Local models cover 100+ languages, selectable by language on the Local Recognition page.
* **Live Captions:** While you speak, a floating window shows the text being recognized. Can be toggled in Settings.
* **Voice Activity Detection (VAD):** Automatically detects speech segments for efficient processing.
* **Real-time Visualizer:** Beautiful FFT-based spectrum and waveform animations.
* **File Transcription:** Batch-import audio files, preview them segment by segment, and export as txt or subtitles with timestamps.

### ✨ AI-Powered Text Refinement
* **Smart Polishing:** Use LLMs to fix grammar, adjust tone, or reformat text automatically.
* **Local Translation:** Download local translation models; polishing and translation are now separate features (local translation is still being refined).
* **Customizable Intelligence:** Define custom prompts, specialized AI vocabularies, and text replacement rules.
* **Terminology Management:** Maintain a dedicated `Terms Library` to ensure technical accuracy.

### 🛠️ Seamless Workflow
* **Global Hotkeys:** Trigger transcription anytime, anywhere with a single keystroke, or use push-to-talk.
* **Modern UI/UX:** A sleek, macOS-inspired interface built with Qt Widgets, featuring smooth animations and modern aesthetics; supports light, gray, and dark themes.
* **Background Operation:** Lightweight system tray integration with quick tray-menu actions for non-intrusive use.
* **Recognition Log:** Keeps each recognition result; the log header can copy all records at once.
* **Single Instance:** Launching again brings the running window to the front.
* **Instant Settings:** Changes apply immediately; the Settings page is temporarily locked while recording to avoid mid-session changes.
* **Auto-Update:** Stay current with the built-in automatic update system.

## 🚀 Getting Started

### Supported Platforms
* 💻 Windows (Current stable)
* 🍎 macOS (Feature-complete)
* 🐧 Linux (Planned)

### Prerequisites
* Windows 10/11
* Internet connection (for Cloud API backends)

### Installation
1. Download the latest release from the [Releases](#) page.
2. Run the installer or extract the portable version.
3. Configure your preferred API keys in the Settings menu.

## ⚙️ Configuration
* **Engine Selection:** Local vs Cloud; the Local Recognition page lets you pick a model per language.
* **API Credentials:** Gemini, Groq, Gladia, etc.; you can test a key directly in Settings.
* **Live Captions:** Toggle in Settings; supports streaming languages such as Chinese, English, Korean, and French.
* **UI Themes:** Light, Dark, Gray, or System default.
* **Speech Rules:** Custom vocabulary (key-value cards) and polishing styles.

## 🛠 Technical Stack
* **Framework:** Qt 6 (C++/Widgets)
* **Audio Capture:** Qt Multimedia (QAudioSource)
* **Audio Decoding:** FFmpeg (file transcription)
* **Global Hotkeys:** QHotkey
* **DSP:** KissFFT
* **AI Engines:** sherpa-onnx (Local), Gemini, Groq, Gladia, OpenAI
* **Local Translation:** a separate worker process that talks to the main app over a line-based protocol

## 📄 License
Distributed under the AGPL v3 License. See `LICENSE.txt` for more information.