# 实时字幕（流式）模型基准（FLEURS test 子集）

- 分数、归一化、数字句排除规则与整句识别排行榜相同
- 整句一次送入流式识别器，尾部补静音后取最终结果；线程数 2，与软件字幕引擎一致
- RTF 必须远小于 1 才能边说边出字；体积为额外下载量
- ★ 为软件当前使用的伴随模型

| 语言 | 模型 | 分数 | CER | WER | RTF | 体积 | 空结果 | 计分条数 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Chinese | Zipformer CTC 流式 zh 2025 | **91.8** | 8.2% | 99.0% | 0.088 | 162 MB | 0 | 96 |
| Chinese | Zipformer 流式 zh 2025 | **90.2** | 9.8% | 99.7% | 0.102 | 167 MB | 0 | 96 |
| Chinese | Zipformer 流式 zh-en 2023 | **89.6** | 10.4% | 99.0% | 0.056 | 199 MB | 0 | 96 |
| Chinese | Paraformer 流式 zh-en ★ | **89.6** | 10.4% | 98.1% | 0.074 | 237 MB | 0 | 96 |
| Chinese | Zipformer CTC 流式 zh small | **87.2** | 12.8% | 99.7% | 0.022 | 26 MB | 0 | 96 |
| Chinese | Paraformer 流式 zh-yue-en | **87.2** | 12.8% | 99.0% | 0.079 | 238 MB | 0 | 96 |
| Chinese | Streaming Zipformer 8-lang | **83.5** | 16.5% | 102.5% | 0.143 | 339 MB | 0 | 96 |
| English | Kroko 流式 en ★ | **85.1** | 10.5% | 14.9% | 0.030 | 71 MB | 0 | 100 |
| English | Nemotron Speech 流式 en 0.6B | **80.9** | 11.9% | 19.1% | 0.377 | 662 MB | 1 | 100 |
| English | Streaming Zipformer 8-lang | **79.9** | 13.2% | 20.1% | 0.135 | 339 MB | 0 | 100 |
| English | Nemotron 3.5 流式 0.6B (en/ja) | **78.7** | 12.5% | 21.3% | 0.358 | 682 MB | 0 | 100 |
| English | Paraformer 流式 zh-en | **62.5** | 18.7% | 37.5% | 0.085 | 237 MB | 0 | 100 |
| English | Parakeet Unified 流式 en 0.6B | **60.2** | 36.8% | 39.8% | 4.854 | 663 MB | 33 | 100 |
| English | NeMo FastConformer 流式 en 480ms | **58.6** | 32.9% | 41.4% | 0.126 | 137 MB | 9 | 100 |
| English | Zipformer 流式 en 2023 | **27.6** | 67.7% | 72.4% | 0.077 | 73 MB | 59 | 100 |
| English | Zipformer 流式 zh-en 2023 | **24.6** | 71.7% | 75.4% | 0.055 | 199 MB | 63 | 100 |
| English | Zipformer CTC 流式 zh 2025 | **0.0** | 98.8% | 100.0% | 0.088 | 162 MB | 0 | 100 |
| English | Zipformer 流式 zh 2025 | **0.0** | 99.3% | 100.0% | 0.123 | 167 MB | 5 | 100 |
| Cantonese | Paraformer 流式 zh-yue-en ★ | **74.9** | 25.1% | 103.5% | 0.110 | 238 MB | 0 | 100 |
| French | Kroko 流式 fr ★ | **90.6** | 3.7% | 9.4% | 0.028 | 71 MB | 0 | 94 |
| French | Zipformer 流式 fr 2023 | **87.3** | 4.6% | 12.7% | 0.070 | 129 MB | 0 | 94 |
| German | Kroko 流式 de ★ | **94.0** | 1.7% | 6.0% | 0.034 | 71 MB | 0 | 95 |
| Spanish | Kroko 流式 es ★ | **96.6** | 1.2% | 3.4% | 0.039 | 156 MB | 0 | 95 |
| Russian | Vosk 流式 ru small ★ | **90.1** | 2.0% | 9.9% | 0.034 | 29 MB | 0 | 97 |
| Russian | T-one 流式 ru | **88.8** | 2.3% | 11.2% | 0.094 | 144 MB | 0 | 97 |
| Russian | Streaming Zipformer 8-lang | **88.0** | 4.1% | 12.0% | 0.104 | 339 MB | 0 | 97 |
| Korean | Zipformer 流式 ko ★ | **39.3** | 60.7% | 99.6% | 0.067 | 141 MB | 46 | 93 |
| Japanese | Streaming Zipformer 8-lang ★ | **79.0** | 21.0% | 100.8% | 0.094 | 339 MB | 0 | 76 |
| Japanese | Nemotron 3.5 流式 0.6B (en/ja) | **72.6** | 27.4% | 98.4% | 0.327 | 682 MB | 0 | 76 |
| Arabic | Streaming Zipformer 8-lang ★ | **78.9** | 12.5% | 21.1% | 0.097 | 339 MB | 0 | 105 |
| Thai | Streaming Zipformer 8-lang ★ | **83.6** | 16.4% | 112.0% | 0.096 | 339 MB | 0 | 93 |
| Vietnamese | Streaming Zipformer 8-lang ★ | **92.1** | 5.0% | 7.9% | 0.094 | 339 MB | 0 | 101 |
| Indonesian | Streaming Zipformer 8-lang ★ | **90.0** | 4.1% | 10.0% | 0.138 | 339 MB | 0 | 108 |
| Bengali | Vosk 流式 bn ★ | **83.3** | 4.7% | 16.7% | 0.031 | 94 MB | 0 | 92 |

## 模型来源

- Zipformer 流式 zh-en 2023：`csukuangfj/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20`
- Zipformer 流式 zh 2025：`csukuangfj/sherpa-onnx-streaming-zipformer-zh-int8-2025-06-30`
- Zipformer CTC 流式 zh 2025：`csukuangfj/sherpa-onnx-streaming-zipformer-ctc-zh-int8-2025-06-30`
- Zipformer CTC 流式 zh small：`csukuangfj/sherpa-onnx-streaming-zipformer-small-ctc-zh-int8-2025-04-01`
- Paraformer 流式 zh-en：`csukuangfj/sherpa-onnx-streaming-paraformer-bilingual-zh-en`
- Paraformer 流式 zh-yue-en：`csukuangfj/sherpa-onnx-streaming-paraformer-trilingual-zh-cantonese-en`
- Streaming Zipformer 8-lang：`csukuangfj/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10`
- Zipformer 流式 en 2023：`csukuangfj/sherpa-onnx-streaming-zipformer-en-2023-06-26`
- Kroko 流式 en：`csukuangfj/sherpa-onnx-streaming-zipformer-en-kroko-2025-08-06`
- NeMo FastConformer 流式 en 480ms：`csukuangfj/sherpa-onnx-nemo-streaming-fast-conformer-transducer-en-480ms-int8`
- Nemotron Speech 流式 en 0.6B：`csukuangfj2/sherpa-onnx-nemotron-speech-streaming-en-0.6b-560ms-int8-2026-04-25`
- Nemotron 3.5 流式 0.6B (en/ja)：`csukuangfj2/sherpa-onnx-nemotron-3.5-asr-streaming-0.6b-560ms-int8-2026-06-11`
- Parakeet Unified 流式 en 0.6B：`csukuangfj2/sherpa-onnx-nemo-parakeet-unified-en-0.6b-int8-streaming-560ms`
- Zipformer 流式 fr 2023：`shaojieli/sherpa-onnx-streaming-zipformer-fr-2023-04-14`
- Kroko 流式 fr：`csukuangfj/sherpa-onnx-streaming-zipformer-fr-kroko-2025-08-06`
- Kroko 流式 de：`csukuangfj/sherpa-onnx-streaming-zipformer-de-kroko-2025-08-06`
- Kroko 流式 es：`csukuangfj/sherpa-onnx-streaming-zipformer-es-kroko-2025-08-06`
- Vosk 流式 ru small：`csukuangfj/sherpa-onnx-streaming-zipformer-small-ru-vosk-int8-2025-08-16`
- T-one 流式 ru：`csukuangfj/sherpa-onnx-streaming-t-one-russian-2025-09-08`
- Zipformer 流式 ko：`k2-fsa/sherpa-onnx-streaming-zipformer-korean-2024-06-16`
- Vosk 流式 bn：`csukuangfj2/sherpa-onnx-streaming-zipformer-bn-vosk-2026-02-09`
