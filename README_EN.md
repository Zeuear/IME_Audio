# ImeAudio

[![License](https://img.shields.io/badge/license-AGPL%20v3-blue.svg)](LICENSE.txt)
[![Platform](https://img.shields.io/badge/platform-Multi-blueviolet.svg)]()

ImeAudio is a high-performance, AI-driven real-time speech transcription and text polishing utility. Designed for seamless workflow integration, it leverages state-of-the-art local and cloud-based speech recognition engines to turn your voice into refined text instantly.

## 🌟 Key Features

### 🎙️ Intelligent Transcription
* **Multi-Backend Support:** Choose between local processing with **Sherpa** or high-speed cloud engines like **Gemini**, **Groq**, and **Gladia**.
* **Voice Activity Detection (VAD):** Automatically detects speech segments for efficient processing.
* **Real-time Visualizer:** Beautiful FFT-based spectrum and waveform animations.

### ✨ AI-Powered Text Refinement
* **Smart Polishing:** Use LLMs to fix grammar, adjust tone, or reformat text automatically.
* **Customizable Intelligence:** Define custom prompts, specialized AI vocabularies, and text replacement rules.
* **Terminology Management:** Maintain a dedicated `Terms Library` to ensure technical accuracy.

### 🛠️ Seamless Workflow
* **Global Hotkeys:** Trigger transcription anytime, anywhere with a single keystroke.
* **Modern UI/UX:** A sleek, macOS-inspired interface built with Qt/QML, featuring smooth animations and modern aesthetics.
* **Background Operation:** Lightweight system tray integration for non-intrusive use.
* **Auto-Update:** Stay current with the built-in automatic update system.

## 🚀 Getting Started

### Supported Platforms
* 💻 Windows (Current stable)
* 🍎 macOS (Planned)
* 🐧 Linux (Planned)

### Prerequisites
* Windows 10/11
* Internet connection (for Cloud API backends)

### Installation
1. Download the latest release from the [Releases](#) page.
2. Run the installer or extract the portable version.
3. Configure your preferred API keys in the Settings menu.

## ⚙️ Configuration
The application uses an `AppConfig` system allowing you to customize:
* **Engine Selection:** Local vs Cloud.
* **API Credentials:** Gemini, OpenAI, etc.
* **UI Themes:** Light, Dark, Gray, or System default.
* **Speech Rules:** Custom vocabulary and polishing styles.

## 🛠 Technical Stack
* **Framework:** Qt 6 (C++/QML)
* **Audio Engine:** miniaudio / QAudioSource
* **DSP:** kiss_fft
* **AI Engines:** Sherpa (Local), Gemini, Groq, OpenAI

## 📄 License
Distributed under the GPL 3.0 License. See `LICENSE.txt` for more information.