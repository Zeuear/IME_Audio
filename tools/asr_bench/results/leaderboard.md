# Sherpa 本地模型准确率基准（FLEURS test 子集）

- 分数 = 100 × (1 − 主指标错误率)，下限 0；主指标：中/日/韩/泰/粤用 CER，其余用 WER
- 归一化：NFKC、小写、繁→简（中文）、去标点符号
- 参考文本含阿拉伯数字的句子不计分（读法不唯一，如 2011 / 二零一一），「排除」列为其条数
- RTF = 识别耗时 / 音频时长，取逐句中位数（CPU，线程数同软件设置），越小越快
- 参数与 ModelRegistry::GetConfig 一致；Whisper 不指定语言（自动识别），与软件行为相同

| 语言 | 模型 | 分数 | CER | WER | RTF | 加载 | 空结果 | 计分条数 | 排除 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Chinese | Qwen3-ASR 0.6B | **93.3** | 6.7% | 36.9% | 0.262 | 4.2s | 0 | 96 | 24 |
| Chinese | FunASR Nano | **92.9** | 7.1% | 44.6% | 0.164 | 4.2s | 0 | 96 | 24 |
| Chinese | Zipformer CTC zh | **92.8** | 7.2% | 98.4% | 0.028 | 3.0s | 0 | 96 | 24 |
| Chinese | FireRed ASR large † | **92.8** | 7.2% | 98.1% | 1.035 | 4.6s | 0 | 96 | 24 |
| Chinese | SenseVoice 2024-07-17 | **92.2** | 7.8% | 57.3% | 0.032 | 2.1s | 0 | 96 | 24 |
| Chinese | Paraformer zh | **92.1** | 7.9% | 97.1% | 0.034 | 1.6s | 0 | 96 | 24 |
| Chinese | SenseVoice 2025-09-09 (WSYue) | **92.0** | 8.0% | 97.1% | 0.030 | 1.0s | 0 | 96 | 24 |
| Chinese | Zipformer multi zh-en | **88.3** | 11.7% | 98.7% | 0.016 | 1.8s | 0 | 96 | 24 |
| English | Whisper small.en | **95.1** | 2.3% | 4.9% | 0.286 | 1.8s | 0 | 100 | 20 |
| English | SenseVoice 2024-07-17 | **93.7** | 3.2% | 6.3% | 0.032 | 2.1s | 0 | 100 | 20 |
| English | Whisper base.en | **92.3** | 3.5% | 7.7% | 0.092 | 0.6s | 0 | 100 | 20 |
| English | FireRed ASR large † | **91.7** | 4.5% | 8.3% | 1.457 | 5.3s | 0 | 100 | 20 |
| English | Paraformer en | **91.0** | 3.8% | 9.0% | 0.023 | 2.0s | 0 | 100 | 20 |
| English | Parakeet TDT 0.6B v3 | **89.9** | 6.4% | 10.1% | 0.070 | 2.1s | 2 | 100 | 20 |
| English | Whisper tiny.en | **89.5** | 4.8% | 10.5% | 0.062 | 0.5s | 0 | 100 | 20 |
| English | NeMo FastConformer 10-lang | **88.3** | 7.6% | 11.7% | 0.022 | 3.3s | 3 | 100 | 20 |
| English | Moonshine base en | **87.1** | 7.6% | 12.9% | 0.036 | 1.5s | 4 | 100 | 20 |
| English | SenseVoice 2025-09-09 (WSYue) | **74.6** | 9.7% | 25.4% | 0.028 | 1.0s | 0 | 100 | 20 |
| English | Zipformer multi zh-en | **35.0** | 60.6% | 65.0% | 0.016 | 1.8s | 0 | 100 | 20 |
| Georgian | Omnilingual 1B | **81.1** | 3.5% | 18.9% | 0.239 | 1.8s | 0 | 98 | 22 |
| Georgian | NeMo FastConformer ka | **69.1** | 20.1% | 30.9% | 0.044 | 413.7s | 6 | 98 | 22 |
| Georgian | Omnilingual 300M | **65.9** | 5.8% | 34.1% | 0.101 | 0.7s | 0 | 98 | 22 |
| Georgian | Whisper small | **0.0** | 100.2% | 103.8% | 0.521 | 1.6s | 59 | 98 | 22 |
| Georgian | Whisper base | **0.0** | 100.9% | 118.7% | 0.155 | 1.2s | 0 | 98 | 22 |
| Armenian | Omnilingual 1B | **89.0** | 1.9% | 11.0% | 0.279 | 1.8s | 0 | 99 | 21 |
| Armenian | NeMo FastConformer hy | **82.0** | 14.0% | 18.0% | 0.020 | 2.9s | 8 | 99 | 21 |
| Armenian | Omnilingual 300M | **77.2** | 3.4% | 22.8% | 0.100 | 0.7s | 0 | 99 | 21 |
| Armenian | Whisper small | **0.4** | 87.5% | 99.6% | 0.464 | 1.6s | 19 | 99 | 21 |
| Armenian | Whisper base | **0.0** | 99.4% | 103.3% | 0.168 | 1.2s | 35 | 99 | 21 |
| Arabic | NeMo FastConformer ar | **90.2** | 2.6% | 9.8% | 0.018 | 3.3s | 0 | 105 | 15 |
| Arabic | Whisper base | **44.2** | 23.1% | 55.8% | 0.153 | 0.8s | 0 | 105 | 15 |
| Cantonese | SenseVoice 2024-07-17 | **94.1** | 5.9% | 91.0% | 0.083 | 2.1s | 0 | 100 | 20 |
| Cantonese | SenseVoice 2025-09-09 (WSYue) | **93.9** | 6.1% | 98.6% | 0.028 | 1.0s | 0 | 100 | 20 |
| Cantonese | WeNetSpeech-Yue CTC | **92.0** | 8.0% | 99.7% | 0.021 | 0.6s | 0 | 100 | 20 |
| Cantonese | Zipformer MDCC | **64.9** | 35.1% | 99.7% | 0.018 | 3.0s | 0 | 100 | 20 |
| French | Parakeet TDT 0.6B v3 | **92.7** | 2.9% | 7.3% | 0.069 | 4.4s | 0 | 94 | 26 |
| French | Whisper base | **69.3** | 13.0% | 30.7% | 0.119 | 0.8s | 0 | 94 | 26 |
| German | NeMo FastConformer RNNT de | **95.3** | 0.9% | 4.7% | 0.021 | 2.5s | 0 | 95 | 25 |
| German | Parakeet TDT 0.6B v3 | **93.4** | 2.6% | 6.6% | 0.068 | 4.4s | 0 | 95 | 25 |
| German | NeMo FastConformer CTC de | **93.4** | 2.0% | 6.6% | 0.027 | 1.1s | 1 | 95 | 25 |
| German | Whisper base | **78.6** | 7.5% | 21.4% | 0.104 | 0.8s | 0 | 95 | 25 |
| Japanese | SenseVoice 2024-07-17 | **92.7** | 7.3% | 115.2% | 0.031 | 2.1s | 0 | 76 | 44 |
| Japanese | ReazonSpeech k2 v2 | **92.4** | 7.6% | 97.3% | 0.039 | 4.4s | 0 | 76 | 44 |
| Japanese | Whisper base | **68.9** | 31.1% | 106.6% | 0.117 | 0.8s | 0 | 76 | 44 |
| Japanese | SenseVoice 2025-09-09 (WSYue) | **23.4** | 76.6% | 100.0% | 0.030 | 1.0s | 0 | 76 | 44 |
| Korean | SenseVoice 2024-07-17 | **93.8** | 6.2% | 21.4% | 0.033 | 2.1s | 0 | 93 | 27 |
| Korean | Whisper base | **75.2** | 24.8% | 52.2% | 0.111 | 0.8s | 0 | 93 | 27 |
| Korean | Zipformer ko | **46.2** | 53.8% | 99.9% | 0.028 | 3.0s | 41 | 93 | 27 |
| Korean | SenseVoice 2025-09-09 (WSYue) | **26.0** | 74.0% | 90.4% | 0.032 | 1.0s | 0 | 93 | 27 |
| Portuguese | Parakeet TDT 0.6B v3 | **94.0** | 2.5% | 6.0% | 0.077 | 4.4s | 0 | 88 | 32 |
| Portuguese | NeMo FastConformer RNNT pt | **93.2** | 2.4% | 6.8% | 0.021 | 2.4s | 0 | 88 | 32 |
| Portuguese | NeMo FastConformer CTC pt | **93.1** | 2.6% | 6.9% | 0.027 | 1.1s | 0 | 88 | 32 |
| Portuguese | Whisper base | **84.8** | 5.8% | 15.2% | 0.106 | 0.8s | 0 | 88 | 32 |
| Russian | GigaAM v3 RNNT | **95.8** | 0.8% | 4.2% | 0.044 | 1.9s | 0 | 97 | 23 |
| Russian | GigaAM v3 RNNT (punct) | **95.7** | 1.0% | 4.3% | 0.045 | 2.2s | 0 | 97 | 23 |
| Russian | GigaAM v2 CTC | **95.2** | 0.9% | 4.8% | 0.118 | 1.5s | 0 | 97 | 23 |
| Russian | GigaAM v3 CTC | **95.2** | 0.9% | 4.8% | 0.067 | 1.0s | 0 | 97 | 23 |
| Russian | GigaAM v2 RNNT | **94.9** | 0.9% | 5.1% | 0.087 | 2.6s | 0 | 97 | 23 |
| Russian | GigaAM v1 RNNT | **94.3** | 1.2% | 5.7% | 0.097 | 1.9s | 0 | 97 | 23 |
| Russian | Vosk ru | **93.7** | 1.4% | 6.3% | 0.015 | 2.5s | 0 | 97 | 23 |
| Russian | GigaAM v1 CTC | **93.3** | 1.3% | 6.7% | 0.130 | 1.2s | 0 | 97 | 23 |
| Russian | Parakeet TDT 0.6B v3 | **92.8** | 1.5% | 7.2% | 0.065 | 4.4s | 0 | 97 | 23 |
| Russian | Whisper base | **76.2** | 6.4% | 23.8% | 0.121 | 0.8s | 0 | 97 | 23 |
| Spanish | Parakeet TDT 0.6B v3 | **97.2** | 1.1% | 2.8% | 0.064 | 4.4s | 0 | 95 | 25 |
| Spanish | NeMo FastConformer CTC es | **94.0** | 1.6% | 6.0% | 0.027 | 1.3s | 0 | 95 | 25 |
| Spanish | Whisper base | **88.6** | 3.8% | 11.4% | 0.108 | 0.8s | 0 | 95 | 25 |
| Thai | Zipformer GigaSpeech2 th | **91.6** | 8.4% | 99.7% | 0.023 | 3.3s | 0 | 93 | 27 |
| Thai | Whisper base | **39.9** | 60.1% | 119.6% | 0.162 | 0.8s | 0 | 93 | 27 |
| Vietnamese | Zipformer vi | **92.9** | 5.3% | 7.1% | 0.018 | 2.8s | 0 | 101 | 19 |
| Vietnamese | Whisper base | **56.9** | 28.0% | 43.1% | 0.124 | 0.8s | 0 | 101 | 19 |
| Tagalog | NeMo FastConformer tl | **89.3** | 3.0% | 10.7% | 0.019 | 2.9s | 0 | 100 | 20 |
| Tagalog | Omnilingual 1B | **88.3** | 2.8% | 11.7% | 0.343 | 1.8s | 0 | 100 | 20 |
| Tagalog | Omnilingual 300M | **81.9** | 4.0% | 18.1% | 0.107 | 0.7s | 0 | 100 | 20 |
| Tagalog | Whisper small | **73.3** | 6.9% | 26.7% | 0.288 | 1.6s | 0 | 100 | 20 |
| Tagalog | Qwen3-ASR 0.6B | **57.0** | 11.5% | 43.0% | 0.226 | 4.3s | 0 | 100 | 20 |
| Tagalog | Whisper base | **43.5** | 16.9% | 56.5% | 0.106 | 1.2s | 0 | 100 | 20 |
| Italian | Parakeet TDT 0.6B v3 | **96.5** | 1.3% | 3.5% | 0.062 | 4.4s | 0 | 92 | 28 |
| Italian | NeMo FastConformer 10-lang | **93.0** | 1.7% | 7.0% | 0.020 | 3.3s | 0 | 92 | 28 |
| Italian | Qwen3-ASR 0.6B | **91.1** | 3.9% | 8.9% | 0.232 | 3.9s | 0 | 92 | 28 |
| Italian | Whisper base | **79.6** | 4.9% | 20.4% | 0.105 | 0.8s | 0 | 92 | 28 |
| Ukrainian | NeMo FastConformer 10-lang | **85.3** | 6.3% | 14.7% | 0.020 | 3.3s | 1 | 102 | 18 |
| Ukrainian | Parakeet TDT 0.6B v3 | **84.6** | 7.9% | 15.4% | 0.086 | 4.4s | 1 | 102 | 18 |
| Ukrainian | Whisper base | **46.2** | 18.1% | 53.8% | 0.142 | 0.8s | 0 | 102 | 18 |
| Ukrainian | Moonshine base uk | **27.3** | 69.9% | 72.7% | 0.027 | 0.7s | 57 | 102 | 18 |
| Polish | NeMo FastConformer 10-lang | **87.6** | 3.5% | 12.4% | 0.022 | 3.3s | 0 | 97 | 23 |
| Polish | Parakeet TDT 0.6B v3 | **81.3** | 11.3% | 18.7% | 0.064 | 4.4s | 5 | 97 | 23 |
| Polish | Qwen3-ASR 0.6B | **57.2** | 16.4% | 42.8% | 0.286 | 3.9s | 0 | 97 | 23 |
| Polish | Whisper base | **56.3** | 13.9% | 43.7% | 0.132 | 0.8s | 0 | 97 | 23 |
| Dutch | Parakeet TDT 0.6B v3 | **84.2** | 10.0% | 15.8% | 0.064 | 4.4s | 4 | 98 | 22 |
| Dutch | Qwen3-ASR 0.6B | **81.5** | 6.8% | 18.5% | 0.326 | 3.9s | 0 | 98 | 22 |
| Dutch | Whisper base | **54.3** | 16.2% | 45.7% | 0.137 | 0.8s | 0 | 98 | 22 |
| Turkish | Qwen3-ASR 0.6B | **72.4** | 7.4% | 27.6% | 0.228 | 3.9s | 0 | 105 | 15 |
| Turkish | Whisper base | **66.9** | 9.2% | 33.1% | 0.115 | 0.8s | 0 | 105 | 15 |
| Indonesian | Streaming Zipformer 8-lang | **90.0** | 4.1% | 10.0% | 0.114 | 3.8s | 0 | 108 | 12 |
| Indonesian | Qwen3-ASR 0.6B | **86.0** | 5.4% | 14.0% | 0.207 | 3.0s | 0 | 108 | 12 |
| Indonesian | Whisper base | **61.1** | 13.6% | 38.9% | 0.102 | 0.8s | 0 | 108 | 12 |
| Hindi | Qwen3-ASR 0.6B | **67.5** | 22.0% | 32.5% | 0.446 | 3.0s | 0 | 102 | 18 |
| Hindi | Whisper base | **0.0** | 105.6% | 106.1% | 0.160 | 0.8s | 1 | 102 | 18 |
| Persian | Shenava Koochik fa † | **76.8** | 4.9% | 23.2% | 0.029 | 0.9s | 0 | 95 | 25 |
| Persian | Shenava Rizeh fa (32M) † | **71.9** | 7.1% | 28.1% | 0.013 | 0.9s | 0 | 95 | 25 |
| Persian | Qwen3-ASR 0.6B | **28.7** | 58.0% | 71.3% | 0.274 | 3.0s | 0 | 95 | 25 |
| Persian | Whisper base | **4.3** | 41.7% | 95.7% | 0.161 | 0.8s | 0 | 95 | 25 |
| Hebrew | Omnilingual 1B | **68.4** | 10.6% | 31.6% | 0.230 | 2.2s | 0 | 93 | 27 |
| Hebrew | Omnilingual 300M | **51.7** | 15.8% | 48.3% | 0.101 | 0.7s | 0 | 93 | 27 |
| Hebrew | Whisper small | **35.3** | 34.0% | 64.7% | 0.515 | 1.7s | 0 | 93 | 27 |
| Hebrew | Whisper base | **20.7** | 44.3% | 79.3% | 0.156 | 0.8s | 0 | 93 | 27 |

† 软件中尚未提供（新语种候选，或如 FireRed 在 SherpaConfig.cpp 中被注释），仅作参考。

## 未测试

- **Tibetan**：FLEURS / Common Voice have no Tibetan; no open test set with a stable download

## 各组合错误最多的样例

### Chinese / Paraformer zh
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普埃尔多安通话后发表了声明
- `12033218428575912024.wav` CER 38.8%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：胡娜已经辞职他在内阁中的位置将由国会议员艾德代维解体预计国会议员诺曼栏姆将解替艾维空处的商务部长一职

### Chinese / Zipformer CTC zh
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普埃尔多安通话后发表了声明
- `12033218428575912024.wav` CER 35.8%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：胡稳已经辞职他在内阁中的位置将由国会议员艾德戴维解体预计国会议员诺曼兰姆将解体艾维空出的商务部长一职

### Chinese / Zipformer multi zh-en
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地
- `10949290146151676233.wav` CER 40.6%
  - 参考：也可以到北部参观法蒂玛圣母院（圣地），它是举世闻名的圣母玛丽亚显现之地。
  - 识别：也可以到北部参观法地马上染学院胜地它是居于市文明的圣母马莉亚显现之地
- `12033218428575912024.wav` CER 40.3%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：虐已经辞职他在内格中的位置将由国会议员爱德戴维接体预计国会议员诺曼栏姆将解体艾维空出的商务部长一致

### Chinese / SenseVoice 2025-09-09 (WSYue)
- `10604423531103587528.wav` CER 44.8%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼布尔的联合国维和部队在不知情的情况下将这种疾病带到了海地
- `12033218428575912024.wav` CER 41.8%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：胡奈已经辞职他在内阁中的位置将由国会议员艾德代为解体预计国会议员诺曼兰姆将解体艾为空处的商务部长一职
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普埃尔多安通话后发表了声明

### Chinese / SenseVoice 2024-07-17
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普、埃尔多安通话后发表了声明。
- `12033218428575912024.wav` CER 35.8%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：胡娜已经辞职，他在内阁中的位置将由国会议员艾德代维解体。预计国会议员诺曼兰姆将解替艾维空出的商务部长一职。

### Chinese / Qwen3-ASR 0.6B
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下，将这种疾病带到了海地。
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安通话后，发表了声明。
- `11405581964812220670.wav` CER 37.3%
  - 参考：研究该疾病的联合国专家丹妮尔·拉塔涅 (Danielle Lantagne) 表示，疫情爆发很可能是由维和人员引起的。
  - 识别：研究该疾病的联合国专家丹尼尔·拉坦耶表示：“疫情爆发很可能是由维和人员引起的。”

### Chinese / FunASR Nano
- `12033218428575912024.wav` CER 58.2%
  - 参考：胡纳已经辞职，他在内阁中的位置将由国会议员埃德·戴维 (Ed Davey) 接替。预计国会议员诺曼·兰姆 (Norman Lamb) 将接替戴维空出的商务部长一职。
  - 识别：胡娜已经辞职，她在内阁中的位置将由国会议员艾维空出的商务部长一职。
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下，将这种疾病带到了海地。
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普埃尔多安通话后发表了声明。

### Chinese / FireRed ASR large
- `10604423531103587528.wav` CER 43.7%
  - 参考：海地正义与民主研究所 (Haitian Institute for Justice and Democracy) 引用的独立研究表明，是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地。
  - 识别：海地正义与民主研究所引用的独立研究表明是尼泊尔的联合国维和部队在不知情的情况下将这种疾病带到了海地
- `10325559490685159122.wav` CER 40.0%
  - 参考：特朗普与土耳其总统雷杰普·塔伊普·埃尔多安（Recep Tayyip Erdoğan）通话后发表了声明。
  - 识别：特朗普与土耳其总统雷杰普塔伊普埃尔多安通话后发表了声明
- `11405581964812220670.wav` CER 33.3%
  - 参考：研究该疾病的联合国专家丹妮尔·拉塔涅 (Danielle Lantagne) 表示，疫情爆发很可能是由维和人员引起的。
  - 识别：研究该疾病的联合国专家丹尼尔拉塔涅表示疫情爆发很可能是由维和人员引起的

### English / Parakeet TDT 0.6B v3
- `12230326991067783666.wav` CER 100.0%
  - 参考：USA Gymnastics and the USOC have the same goal — making the sport of gymnastics, and others, as safe as possible for athletes to follow their dreams in a safe, positive and empowered environment.
  - 识别：
- `11554335886429174193.wav` CER 100.0%
  - 参考：Generally speaking, two behaviors can emerge as managers begin to lead their former peers. One end of the spectrum is trying to remain “one of the guys” (or gals).
  - 识别：
- `12725120105000747485.wav` CER 29.7%
  - 参考：The announcement was made after Trump had a phone conversation with Turkish President Recep Tayyip Erdoğan.
  - 识别：The announcement was made after Trump had a phone conversation with Pres in Turkis President. Is that the type who edited one?

### English / Whisper tiny.en
- `10471564175308895403.wav` CER 30.5%
  - 参考：But there are a lot of things about birds that still look like a dinosaur.
  - 识别：But there are other things about the stuff I looked like a dinosaur.
- `12725120105000747485.wav` CER 25.3%
  - 参考：The announcement was made after Trump had a phone conversation with Turkish President Recep Tayyip Erdoğan.
  - 识别：The announcement was made after Trump had a phone conversation with in Turkish President was that the type we had or the one
- `12570605547501325774.wav` CER 24.1%
  - 参考：The same month saw another airliner overrun a runway at Mashhad and strike a wall, killing seventeen.
  - 识别：This same month saw another airliner over Runaway at Moshan in Strikelwall, killing 17.

### English / Whisper base.en
- `10471564175308895403.wav` CER 16.9%
  - 参考：But there are a lot of things about birds that still look like a dinosaur.
  - 识别：But there are other things about those that still look like a dinosaur.
- `12618421744672029626.wav` CER 15.8%
  - 参考：Beyond Wednesday's event, Carpanedo competed in two individual races at the Championships.
  - 识别：Beyond one Sazy Bank, Carponado competed in two individual races at the championships.
- `12570605547501325774.wav` CER 15.7%
  - 参考：The same month saw another airliner overrun a runway at Mashhad and strike a wall, killing seventeen.
  - 识别：The same month saw another airliner overrun a runway at Michade and strike a wall, killing 17.

### English / Whisper small.en
- `12741024238657315067.wav` CER 17.1%
  - 参考：Unfortunately, studying traffic flow is difficult because driver behavior cannot be predicted with one-hundred percent certainty.
  - 识别：Unfortunately, stunning traffic flow is difficult because driver behavior cannot be predicted with 100% certainty.
- `12570605547501325774.wav` CER 15.7%
  - 参考：The same month saw another airliner overrun a runway at Mashhad and strike a wall, killing seventeen.
  - 识别：The same month saw another airliner overrun a runway at Michaud and Strike a wall killing 17.
- `12164368102080603077.wav` CER 13.3%
  - 参考：The governor's office said nineteen of the injured were police officers.
  - 识别：The governor's office said 19 of the injured were police officers.

### English / Moonshine base en
- `12782801286775256949.wav` CER 100.0%
  - 参考：If you want to be close to the action you're going to have to get in early to get a camping site close to the music.
  - 识别：
- `1277588711703410007.wav` CER 100.0%
  - 参考：A well rounded athlete, the tiger can climb (though not well), swim, leap great distances and pull with five times the force of a strong human.
  - 识别：
- `1242235796224521830.wav` CER 100.0%
  - 参考：The parade of buildings that make the Hong Kong skyline has been likened to a glittering bar chart that is made apparent by the presence of the waters of Victoria Harbour.
  - 识别：

### English / Paraformer en
- `12725120105000747485.wav` CER 23.1%
  - 参考：The announcement was made after Trump had a phone conversation with Turkish President Recep Tayyip Erdoğan.
  - 识别：the announcement was made after trump had a phone conversation with president turkish president resptaipu edited one
- `1218355507714944126.wav` CER 17.0%
  - 参考：In its early days, the show was featured solely at the long-running internet radio site TogiNet Radio, a site focused on talk radio.
  - 识别：even its its early days show show was uured solely at the long running internet radio site tokey net radio a site focus 'on' talk radio
- `12570605547501325774.wav` CER 14.5%
  - 参考：The same month saw another airliner overrun a runway at Mashhad and strike a wall, killing seventeen.
  - 识别：this same month thought another airliner overrun a runway at mhad and strike a wall killing seventeen

### English / Zipformer multi zh-en
- `12970636128542220038.wav` CER 100.0%
  - 参考：Mass car ownership also leads to a higher incidence of accidents on the roads, which leads to the invention of new techniques in healthcare for repairing damaged bodies.
  - 识别：嗯
- `12966312239837756963.wav` CER 100.0%
  - 参考：This is called a chemical's pH. You can make an indicator using red cabbage juice.
  - 识别：啊
- `12953138808800823203.wav` CER 100.0%
  - 参考：With Kundalini Yoga the Kundalini energy (enlightenment energy) is awakened through yoga postures, breathing exercises, mantras and visualizations.
  - 识别：啥

### English / SenseVoice 2025-09-09 (WSYue)
- `10938576782105608041.wav` CER 41.3%
  - 参考：Blogs can also help improve student writing. While students often begin their blog experience with sloppy grammar and spelling, the presence of an audience generally changes that.
  - 识别：BLOCS CANLOLPOEOUNT WIN WHL STENTS OTENEN THEIR BOG EXPECE WIT SOPGMERNDELIN TE PRSES AUIERALLY CHANES THAT
- `11504385604540375365.wav` CER 36.2%
  - 参考：Scientists hope to understand how planets form, especially how the Earth formed, since comets collided with the Earth long ago.
  - 识别：CIENCES HOPE TO UNNEND HOW PLANTS FORM ESPECIALLY HOW THEAR FRENEMIT COLIET TEAE LNGO
- `10849792007407044602.wav` CER 27.1%
  - 参考：Virtual teams are held to the same standards of excellence as conventional teams, but there are subtle differences.
  - 识别：RITUAL TEAS ARE HEL THE SAE STANDAR OF EXCELECE AS CONVEIONAL TEASU THERETLE DIFECES

### English / SenseVoice 2024-07-17
- `12741024238657315067.wav` CER 17.1%
  - 参考：Unfortunately, studying traffic flow is difficult because driver behavior cannot be predicted with one-hundred percent certainty.
  - 识别：Unfortunately, stunning traffic flow is difficult because driver behavior cannot be predicted with 100% certainty.
- `12725120105000747485.wav` CER 16.5%
  - 参考：The announcement was made after Trump had a phone conversation with Turkish President Recep Tayyip Erdoğan.
  - 识别：The announcement was made after Trump had a phone conversation with Turkish President Roette Bu Dau editor one.
- `12164368102080603077.wav` CER 13.3%
  - 参考：The governor's office said nineteen of the injured were police officers.
  - 识别：The governor's office said 19 of the injured were police officers.

### English / NeMo FastConformer 10-lang
- `12230326991067783666.wav` CER 100.0%
  - 参考：USA Gymnastics and the USOC have the same goal — making the sport of gymnastics, and others, as safe as possible for athletes to follow their dreams in a safe, positive and empowered environment.
  - 识别：
- `11554335886429174193.wav` CER 100.0%
  - 参考：Generally speaking, two behaviors can emerge as managers begin to lead their former peers. One end of the spectrum is trying to remain “one of the guys” (or gals).
  - 识别：
- `11483786862560523973.wav` CER 100.0%
  - 参考：To the north and within easy reach is the romantic and fascinating town of Sintra and which was made famous to foreigners after a glowing account of its splendours recorded by Lord Byron.
  - 识别：

### English / FireRed ASR large
- `12725120105000747485.wav` CER 22.0%
  - 参考：The announcement was made after Trump had a phone conversation with Turkish President Recep Tayyip Erdoğan.
  - 识别：THE ANNOUNCEMENT WAS MADE AFTER TRUMP HAD A PHONE CONVERSATION WITH IN TURKISH PRESIDENT ROUSSETTE BUT DAIPU ADDED ONE
- `10383288285905292158.wav` CER 20.7%
  - 参考：For example, each year students from Bennet School in North Carolina design a website about their trip to the State Capital, each year the website gets remodeled, but old versions are kept online to serve as a scrapbook.
  - 识别：FOR EXAMPLE EACH YEAR STUDENTS FROM BENNETT SCHOOL IN NORTH CAROLINA TO SIGN A WEBSITE ABOUT THEIR TRIPS TO THE STATE CAPITOL EACH YEAR THE WEBSITE GETS REMODELED BUT OLD VERSIONS
- `11827077161070295147.wav` CER 20.0%
  - 参考：The scenes are displayed on the pyramids and the different pyramids are lit up.
  - 识别：THE SCENES ARE DISPLAYED ON THE PYRAMIDS AND THE DIFFERENT PYR

### Georgian / NeMo FastConformer ka
- `11968884803861528168.wav` CER 100.0%
  - 参考：პროგრამულ უზრუნველყოფაში ჩაშენებული ვირტუალური ხარაჩოების მიზანია კითხვების დასმა, მითითებების გაცემა და ისეთი პროცედურების ახსნა, რომლებიც შესაძლოა სტუდენტისთვის მარტო თავის გასართმევად რთული აღმოჩნდეს.
  - 识别：.
- `11668790354027877311.wav` CER 100.0%
  - 参考：დასასრულს, ბევრი პატარა კატისებრი ოჯახის წევრი არსებობს (მათ შორის შინაური კატები), რომლებიც ბევრად უფრო დიდი ოდენობით მცირე ზომის ნადავლს მიირთმევენ, მაგალითად, მწერებს, მღრღნელებს, ხვლიკებს და ჩიტებს.
  - 识别：.
- `11287630536642593108.wav` CER 100.0%
  - 参考：პრემიერ მინისტრთან აუდიენციისას ლეიტონმა კონსერვატორების გარემოსდაცვით კანონპროექტში განახლებები და კონსერვატიული პარტიის წარმოდგენილი გარემოსდაცვითი კანონპროექტის „საფუძვლიანი და სრული გადაწერა“ მოითხოვა.
  - 识别：.

### Georgian / Whisper base
- `11060635965957888061.wav` CER 136.0%
  - 参考：ლაკჰა საინმა ასევე იმღერა სიმღერა სახელწოდებით „ორმოცდათექვსმეტი ბჰაჯანი“. მას აკომპანიმენტი გაუწია მომღერალმა რაჯუ კჰანდევალმა.
  - 识别：Lakhra signmar aso im garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim garasim
- `10956408060292013388.wav` CER 121.7%
  - 参考：უძველესი კულტურის წარმომადგენლემა და ტომებმა რძის, თმის, ხორცისა და ტყავის უფრო მარტივად მოსაპოვებლად დაიწყეს მათი მოშენება.
  - 识别：Uzwele se kudurist armomodgen lebmedat amemar zist miz hortisatat kavisuprmerti wat muzap oblatatatatatatatatatatatatatatatatatatatatatatatat
- `10086793514196598344.wav` CER 110.4%
  - 参考：გიზას დიდი პირამიდა შვიდი საოცრებიდან ჩვენამდე მოღწეული ერთადერთი საოცრებაა.
  - 识别：Kizas di di piramida, schwidi sautzrebidantur namde mochtsauhli erthaderti sautzreba

### Georgian / Whisper small
- `11923421650806157830.wav` CER 111.3%
  - 参考：pH-ის დონე განისაზღვრება შემოწმებულ ქიმიკატში წყალბადის (pH-ში H) იონების რაოდენობით.
  - 识别：Behech tonneganselgwa shemots oboly kemika cheets khalbadi spehechi heiche iona bizrauda rovid.
- `11616443610572080198.wav` CER 106.0%
  - 参考：მიუხედავად იმისა, რომ ერთ ექსპერიმენტალურ ვაქცინას როგორც ჩანს შეუძლია ებოლას სიკვდილიანობის შემცირება, ამ დრომდე, არც ერთი წამალი აშკარად არ ჩაითვალა შესაფერისად, რომ არსებული ინფექცია განეკურნა.
  - 识别：Miuhhe davadi misarom ert eksperimendalur vaktsinia srogorti chanshse uzgliya e bulasik biljano bishemsireba. Androm de artserti dzamali aşgara darchaitola shesaperi sadrum arse buli impektia gana ekupna.
- `115422705925168078.wav` CER 106.0%
  - 参考：ასევე ჩრდილოეთით შეგიძლიათ ეწვიოთ ფატიმას დიდ ტაძარს (სალოცავი), ადგილი, რომელიც მსოფლიოში მარიამ ღვთისმშობლის გამოცხადებით არის ცნობილი.
  - 识别：Asa wech diloetit shagit liet hech yoyot pathim vas tithadzarsal o tsav yadgili romelit so piliyoshi marien tige jobelis gamotskha debi taris snob

### Georgian / Omnilingual 300M
- `1127530875370738149.wav` CER 23.5%
  - 参考：Aerosmith-მა გააუქმა ტურის ფარგლებში დარჩენილი კონცერტები.
  - 识别：აეროსმიტმა გაოქმატურის ფარგლებში დარჩენინი კონცერტები
- `11060635965957888061.wav` CER 18.0%
  - 参考：ლაკჰა საინმა ასევე იმღერა სიმღერა სახელწოდებით „ორმოცდათექვსმეტი ბჰაჯანი“. მას აკომპანიმენტი გაუწია მომღერალმა რაჯუ კჰანდევალმა.
  - 识别：ლაკრა საინმა ასევე იმღერა სიმღერა სახელწოდებითტ ბჯაჰანი მას აკომპანიმენტი გავწეა მომღერალმა რაჯუ კჰანდევალმა
- `11923421650806157830.wav` CER 14.1%
  - 参考：pH-ის დონე განისაზღვრება შემოწმებულ ქიმიკატში წყალბადის (pH-ში H) იონების რაოდენობით.
  - 识别：pს დონე განისადღვევა შემოწებული ქიმიკათში წყალბადის pში  იონების რაოდენობით

### Georgian / Omnilingual 1B
- `1127530875370738149.wav` CER 21.6%
  - 参考：Aerosmith-მა გააუქმა ტურის ფარგლებში დარჩენილი კონცერტები.
  - 识别：აეროს მიტბა გააოქმა ტურის ფარგლებში დარჩენილი კონცერტები
- `11060635965957888061.wav` CER 17.1%
  - 参考：ლაკჰა საინმა ასევე იმღერა სიმღერა სახელწოდებით „ორმოცდათექვსმეტი ბჰაჯანი“. მას აკომპანიმენტი გაუწია მომღერალმა რაჯუ კჰანდევალმა.
  - 识别：ლაკრასაინმა ასევე იმღერა სიმღერა სახელწოდებით  ბჯაჰანი მას აკომპანიმენტი გაუწია მომღერალმა რაჯუ კჰანდევალმა
- `11923421650806157830.wav` CER 15.5%
  - 参考：pH-ის დონე განისაზღვრება შემოწმებულ ქიმიკატში წყალბადის (pH-ში H) იონების რაოდენობით.
  - 识别：პეჩს დონე განისაზღვეა შემოწებული ქიმიკათში წყალბადის პეჩში h იონების რაოდენობით

### Armenian / NeMo FastConformer hy
- `12163641903107321106.wav` CER 100.0%
  - 参考：Բլոգները կարող են նաև օգտակար լինել ուսանողների գրավորները բարելավելու հարցում: Մինչ ուսանողները հաճախ իրենց բլոգի փորձառությունն սկսում են անփույթ քերականությամբ և ուղղագրությամբ, ընթերցողների ներկայությունը գլխավորապես փոխում է դա:
  - 识别：֊։
- `12106312764549982827.wav` CER 100.0%
  - 参考：Կոչամո Վելլին Չիլիի մագլցման առաջնային նպատակակետն է, որը հայտնի է որպես Հարավային Ամերիկայի Յոսեմիտ՝ խոշոր գրանիտե քարերի ու ժայռերի զանազանությամբ։
  - 识别：
- `11773856758866219973.wav` CER 100.0%
  - 参考：ԱՄՆ-ի Մարմնամարզական կառույցն աջակցում է Միացյալ Նահանգների Օլիմպիական հանձնաժողովի նամակը և ընդունում, որ Օլիմպիական ընտանիքը միանշանակ կարիք ունի խթանելու մեր բոլոր մարզիկների անվտանգ միջավայրը։
  - 识别：֊։

### Armenian / Whisper base
- `1090851664395997994.wav` CER 106.9%
  - 参考：Շատերը դրանց դինոզավր չեն համարում, որովհետև դրանք փետուրներ ունեն և կարող են թռչել:
  - 识别：Și aterea de râns din o zavăr ce în amalum, volove tip de râng pe turnelunen, e văgalor în tercell.
- `11297430287426752644.wav` CER 106.6%
  - 参考：Նա չսահմանեց կրճատումների թիվը՝ ասելով, որ դրանք կարվեն Չինաստանի տնտեսական արտադրանքի ծավալների հիման վրա:
  - 识别：Ne ce sa am anet scarce atum ne ritiba la se le vura drancar vencini asta n-ai tante sa canata dranquita val ne ritima in v
- `11706349553923688309.wav` CER 104.3%
  - 参考：Հունեն լքել է իր պաշտոնը և աշխատասենյակում նրան կփոխարինի խորհրդարանի անդամ Էդ Դավեյը։ Ակնալվում է, որ խորհրդարանի անդամ Նորման Լեմբը կստանձնի Առևտրի և արդյունաբերության նախարարի պաշտոնը, քանի որ Դավեյն այն ազատում է:
  - 识别：Hounel leke leitbastonu jebashkathasinyakum nerengapukaini horthalani antam eth dhaveye. Akangalvame horthalani antam norman lenbe, qastansni arfri jebashkna bölccanna khalarri bastonakhani vordh dhaveinna ayn azatumai

### Armenian / Whisper small
- `12190835238185776926.wav` CER 100.0%
  - 参考：Պառակտման ռումբն աշխատում է այն սկզբունքով, որ էներգիա է պահանջվում ՝ բազում պրոտոններով և նեյտրոններով միջուկը միացնելու համար:
  - 识别：
- `12163641903107321106.wav` CER 100.0%
  - 参考：Բլոգները կարող են նաև օգտակար լինել ուսանողների գրավորները բարելավելու հարցում: Մինչ ուսանողները հաճախ իրենց բլոգի փորձառությունն սկսում են անփույթ քերականությամբ և ուղղագրությամբ, ընթերցողների ներկայությունը գլխավորապես փոխում է դա:
  - 识别：
- `12106312764549982827.wav` CER 100.0%
  - 参考：Կոչամո Վելլին Չիլիի մագլցման առաջնային նպատակակետն է, որը հայտնի է որպես Հարավային Ամերիկայի Յոսեմիտ՝ խոշոր գրանիտե քարերի ու ժայռերի զանազանությամբ։
  - 识别：

### Armenian / Omnilingual 300M
- `11428943224316834848.wav` CER 11.7%
  - 参考：Արդյունքում ձկների երկու տեսակ է ոչնչացվել, իսկ մյուս երկուսը՝ ներառյալ սապատավոր ծածանաձուկը, ոչնչացման եզրին է հայտնվել։
  - 识别：արդյունքում ձկների երկուտեսակ է ոչնչացվել իսկ մյուներառյալ հսապատավոր ծածանաձուկը ոչնչացման ես լինե հայտնվել
- `12145662317146966180.wav` CER 11.5%
  - 参考：Իտալիայի մի շարք այլ քաղաքներում և աշխարհի մնացած մասերում, մասնավորապես Լեհաստանում, նմանատիպ հեռարձակում էին իրականացվել, որը մեծաթիվ մարդիկ էին դիտել:
  - 识别：իտարիայի միշար կայրքաղաքներում աշխարհի մնացած մասերում մասնաորապես լեհաստանում նմատապնատիպ հեր արձակումներէին իրականացվել որ մեծապթիվ մարդիկ էին դիտել
- `12106312764549982827.wav` CER 11.0%
  - 参考：Կոչամո Վելլին Չիլիի մագլցման առաջնային նպատակակետն է, որը հայտնի է որպես Հարավային Ամերիկայի Յոսեմիտ՝ խոշոր գրանիտե քարերի ու ժայռերի զանազանությամբ։
  - 识别：կոչա մովելին չիլիհի մագլցման առաջին նպատակագետն է որը հայտնի է որպես հավայանհարավային ամերիկայի յոսեմիտ խոշոր գրանիտեք արերի ու ժայրերի զանազանությամբ

### Armenian / Omnilingual 1B
- `1175852557561390534.wav` CER 10.3%
  - 参考：Սա լավ հնարավորություն է ընձեռնում տեսնելու Հյուսիափայլը, երբ երկինքը գիշեր-ցերեկ քիչ թե շատ մութ կլինի։
  - 识别：սա լավ հնարավորություն այն ձեռում տեսնելու հյուսիսափալը երբ երկին քգիշերցրեք քիչ թե շատ մուտ կլինի
- `12225538588628822762.wav` CER 9.0%
  - 参考：Երեկ ինը հոգուց բաղկացած նոր Ժամանակավոր ընտրական խորհրդում Մարտելլին երդվեց:
  - 识别：երեք հոգուց զբաղկացած նոր ժամանակավոր ընտրական խորհրդում մարտելլին երթվեց
- `12145662317146966180.wav` CER 8.4%
  - 参考：Իտալիայի մի շարք այլ քաղաքներում և աշխարհի մնացած մասերում, մասնավորապես Լեհաստանում, նմանատիպ հեռարձակում էին իրականացվել, որը մեծաթիվ մարդիկ էին դիտել:
  - 识别：իտալիային մի շարկ այլ քաղաքներում աշխարհի մնացած մասերում մասնավորապես լեհաստանում նմատապ նա տիպ հեռարձակումներ էին իրականացվել որը մեծապթիվ մարդիկ էին դիտել

### Arabic / NeMo FastConformer ar
- `13833367626757001960.wav` CER 21.7%
  - 参考：وقع حاكم كاليفورنيا أرنولد شوارزنيجر على مشروع قانون يحظر بيع أو تأجير ألعاب الفيديو العنيفة للقصر.
  - 识别：وقع حاكم كليفانيا أو تشينغ على مشروع قانون يحظو ببيع أو تأجيل ألعاب الفيديو العنيفة للقصة.
- `10366294495925707527.wav` CER 12.9%
  - 参考：بصفته رياضيّاً شاملاً، يمكن للنّمر التّسلق (وإن لم يكن ذلك بشكلٍ جيد) والسّباحة والقفز لمسافاتٍ طويلةٍ والسّحب بقوّة خمسة أضعاف قوّة الإنسان.
  - 识别：بصفة رياضية شاملا، يمكن للنمر التسلق، وإن لم يكن ذلك بشكل جيد والسباحة والقفز لمسافات طويلة والسحب بقوة خمسة أضعاف قوة الإنسان.
- `1312592379649713976.wav` CER 11.2%
  - 参考：يُوفّر ذلك فرصةً جيدةً لمشاهدةِ الشفقِ القطبيِّ ، حيث ستكون السماء مظلمةً بصورةٍ أكثر أو أقل على مدار الساعة.
  - 识别：يوفر ذلك فرصة جيدة لمشاهدة الشفق القطبي حيث ستكون السماء مظلمة بصورة أكثر أو أقل على مدار الساعة.

### Arabic / Whisper base
- `14191884865856198701.wav` CER 94.7%
  - 参考：ومن بين أكثر الطرق شيوعاً التي تستخدم لتوضيح أهمية التنشئة الاجتماعية، الاعتماد على الحالات القليلة المؤسفة للأطفال الذين عانوا، من خلال الإهمال أو سوء الحظ أو الإيذاء المتعمد، غير مرتبطين اجتماعياً من جانب البالغين أثناء نشأتهم.
  - 识别：ومن بين أكثر 
- `13697395296517873707.wav` CER 85.3%
  - 参考：تمنح جزيرة هونج كونج إقليم هونج كونج اسمه وهي المكان الذي يعتبره العديد من السائحين قبلتهم.
  - 识别：تم نحجزر تهنقققققققققققققققققققققققققققققققققققققققققققققققققق
- `13036037027843587599.wav` CER 63.1%
  - 参考：يؤدي الامتلاك الجماعي للسيارات أيضاً إلى ارتفاع معدل الحوادث على الطرق، مما يؤدي إلى ابتكار تقنيات جديدة في الرعاية الصحية لعلاج الأجسام المتضررة.
  - 识别：يؤجل امتلك الجمعي للصياط ايضا ايضا ايضا ايضا افام عضل حواجسة لطوق منما يؤجل ابتكارت اكنات

### Cantonese / WeNetSpeech-Yue CTC
- `12013988314189994505.wav` CER 31.4%
  - 参考：它指出將圖像垂直和水平三等分後，線條的交點便是拍攝主體最有效的位置（見範例）。
  - 识别：他指出张图像水直和水平三等喷后先掉得胶点便是发摄主体最有效的位置见返礼
- `1145510056922159946.wav` CER 29.4%
  - 参考：網路結合了大眾與人際溝通這兩種元素。
  - 识别：网络结合了大众与人际沟通这两大元素两种元素
- `12190602084850197116.wav` CER 25.0%
  - 参考：首先，大部份的騎士都穿著有鞋跟且鞋底平滑狹窄的馬靴。
  - 识别：首先大部分的骑士都穿着有巾扯鞋底平滑杂狭窄的马靴

### Cantonese / Zipformer MDCC
- `11589268020485293259.wav` CER 85.7%
  - 参考：史密斯飛船已經取消他們巡迴演唱會的剩餘場次。
  - 识别：小思非常言出要他們秦的正如長刺
- `11663891407050835566.wav` CER 78.6%
  - 参考：山羊似乎是在一萬年前左右，在伊朗的札格洛斯山脈首次被馴養的。
  - 识别：生人只會似咗一萬年前咗一對依戀的集落絲三物守持樣的
- `1192969723645644630.wav` CER 73.3%
  - 参考：把滑雪路線想像成類似的健行路線。
  - 识别：把話說老禪路似的鏡頭腦線

### Cantonese / SenseVoice 2025-09-09 (WSYue)
- `12002677380123138202.wav` CER 30.0%
  - 参考：船舶（包括遊樂船和有遠端資料與音訊需求的探險船）時常使用這項服務。
  - 识别：船拍包括游乐场和邮软端资料与音讯需求的鉴片说时常使用者看服务
- `1145510056922159946.wav` CER 29.4%
  - 参考：網路結合了大眾與人際溝通這兩種元素。
  - 识别：网络结合了大众与人际沟通这两大元素两种元素
- `11365352224266599578.wav` CER 26.5%
  - 参考：利金斯 (Liggins) 爵士於醫院任職時，就會利用下班後的時間研究早產現象。
  - 识别：利金斯 在时于医院任职时就会利用下班后的时间研究早产现象

### Cantonese / SenseVoice 2024-07-17
- `1145510056922159946.wav` CER 29.4%
  - 参考：網路結合了大眾與人際溝通這兩種元素。
  - 识别：网络结合了大众与人际沟通这两大元素两种元素。
- `11450239886935362355.wav` CER 20.8%
  - 参考：早期，這個節目只有在一個經營許久的網路電台 TogiNet 上播出，TogiNet 是一個聚焦於談話性廣播節目的電台。
  - 识别：早期这个节目只有在一个经营许久的网络电台 net 上播出 net 是一个最超于谈话性广播节目的电台。
- `12002677380123138202.wav` CER 20.0%
  - 参考：船舶（包括遊樂船和有遠端資料與音訊需求的探險船）時常使用這項服務。
  - 识别：船舶包括游乐船和游氧端车料与音讯需求的间片说时常使用这项服务。

### French / Parakeet TDT 0.6B v3
- `10334413597118469552.wav` CER 13.8%
  - 参考：Malheureusement, il est difficile d'étudier le flux de circulation car le comportement des conducteurs ne peut être prédit avec cent pour cent de certitude.
  - 识别：Malheureusement, il est difficile d'étudier le flux de circulation car les comportements des conducteurs ne peuvent être prédits avec 100% de certitude.
- `10143749263702132703.wav` CER 11.7%
  - 参考：Le rugissement du tigre ne ressemble pas au rugissement ample du lion, mais plutôt à une phrase dont les mots seraient des cris et des grondements.
  - 识别：Le gissement du ting ne ressemble pas au gissement en plus du viant, mais plutôt à une phrase dont les mots seraient des cris et des grondements.
- `12265382986326314541.wav` CER 10.2%
  - 参考：Des chercheurs de l'université de Princeton aux États-Unis et de l'université d'Uppsala en Suède ont indiqué que la nouvelle espèce avait évolué en seulement deux générations, bien que ce processus ait été estimé beaucoup plus long, suite à la reproduction entre un pinson de Darwin endémique, Geospiza fortis, et le pinson immigrant à bec conique, Geospiza conirostris.
  - 识别：Des chercheurs de l'université de Princeton aux États Unis et de l'Université d'Uppsala en Suède ont indiqué que la nouvelle espèce avait évolué en seulement deux générations, bien que ce processus était estimé beaucoup plus long suite à la reproduction de Darwin endémique, Jospisa Fortis, et le pinson immigrant avec conic, Jospisa Conyostris.

### French / Whisper base
- `11575460016199460203.wav` CER 43.1%
  - 参考：Les Parisiens ont la réputation d'être égocentriques, grossiers et arrogants.
  - 识别：Par exemple, on a réputation d'être egocentric grossée à Rouman.
- `10703796668246155041.wav` CER 42.3%
  - 参考：Le chocolat chaud est conforme aux normes belges. Les jus de fruits sont chers mais excellents.
  - 识别：Je suis un conforment d'arm-belles, il est juste de frissons cher mais excellent.
- `10876086819338968684.wav` CER 41.2%
  - 参考：Martelly a intronisé hier un nouveau Conseil électoral provisoire (CEP) composé de neuf membres.
  - 识别：Marteline entrenait d'ailleurs un nouveau conseiller lectoral provisoire ce que peut composer de 9 mois

### German / Parakeet TDT 0.6B v3
- `10376367261184486277.wav` CER 49.5%
  - 参考："Ihr thermisches Verhalten ist nicht so gleichmäßig wie das großer Höhlen auf der Erde, die oft eine ziemlich konstante Temperatur haben, aber es stimmt damit überein, dass es sich um tiefe Löcher im Boden handelt"", sagte Glen Cushing vom Astrogeologie-Team des United States Geological Survey (USGS) und der Northern Arizona University in Flagstaff, Arizona."
  - 识别：But it stimmt that it um tiefe Löcher in boden handelt, sagte Glenn Cushing from Astro Geology Team the United States Geological Survey, USGS, and the Northern Arizona University in Flagstaff, Arizona.
- `10938532437386272690.wav` CER 28.6%
  - 参考：Er wurde infolgedessen in das Addenbrooke’s Hospital in Cambridge verlegt.
  - 识别：Er wurde in Folgden in the hospital in Cambridge verlegt
- `103625751236710165.wav` CER 13.5%
  - 参考：Lakkha Singh präsentierte den Chhappan Bhog Bhajan ebenfalls. Der Sänger Raju Khandelwal begleitete ihn.
  - 识别：Lakas Singh präsentierte den Chapan Bog Baian ebenfalls, der Sänger Rayu Khandel begleitete ihn.

### German / NeMo FastConformer RNNT de
- `103625751236710165.wav` CER 14.6%
  - 参考：Lakkha Singh präsentierte den Chhappan Bhog Bhajan ebenfalls. Der Sänger Raju Khandelwal begleitete ihn.
  - 识别：La Casingh präsentierte den Charpan Bog Bayan ebenfalls, der Sänger Rayu Can Delvaal begleitete ihn.
- `10523281885564082084.wav` CER 8.0%
  - 参考：Im dritten Jahrhundert v. Chr. von den Ägyptern erbaut, ist die Cheops-Pyramide eine von vielen großen Pyramidenstrukturen, die zur Verehrung verstorbener Pharaonen erbaut wurden.
  - 识别：Im dritten Jahrhundert vor Christus von den Ägyptern erbaut, ist die Schiropyramide eine von vielen großen Pyramidenstrukturen, die zur Verehrung verstorbener Pharaonen erbaut wurden.
- `1123411366127027322.wav` CER 6.7%
  - 参考：Im dritten Jahrhundert v. Chr. von den Ägyptern erbaut, ist die Cheops-Pyramide eine von vielen großen Pyramidenstrukturen, die zur Verehrung verstorbener Pharaonen erbaut wurden.
  - 识别：Im dritten Jahrhundert vor Christus, von den Ägyptern erbaut, ist die Schirops Pyramide eine von vielen großen Pyramidenstrukturen, die zur Verehrung verstorbener Pharaonen erbaut wurden.

### German / NeMo FastConformer CTC de
- `10400220672903290490.wav` CER 100.0%
  - 参考：Bald nach dem Ausbruch von Kampfhandlungen initiierte Großbritannien eine Seeblockade gegen Deutschland.
  - 识别：
- `103625751236710165.wav` CER 12.4%
  - 参考：Lakkha Singh präsentierte den Chhappan Bhog Bhajan ebenfalls. Der Sänger Raju Khandelwal begleitete ihn.
  - 识别：Laka Singh präsentierte den Charpan Boog Bajaan ebenfalls , der Sänger Rajyu Can Del Val begleitete ihn
- `10523281885564082084.wav` CER 8.0%
  - 参考：Im dritten Jahrhundert v. Chr. von den Ägyptern erbaut, ist die Cheops-Pyramide eine von vielen großen Pyramidenstrukturen, die zur Verehrung verstorbener Pharaonen erbaut wurden.
  - 识别：Im dritten Jahrhundert vor Christus von den Ägypten erbaut , ist die Schobspyramide eine von vielen großen Pyramidenstruturen , die zur Verehrung verstorbener Pharaonen erbaut wurden .

### German / Whisper base
- `10938532437386272690.wav` CER 142.9%
  - 参考：Er wurde infolgedessen in das Addenbrooke’s Hospital in Cambridge verlegt.
  - 识别：Avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin, avoodin
- `10990997007524611628.wav` CER 39.7%
  - 参考：Er wurde infolgedessen in das Addenbrooke’s Hospital in Cambridge verlegt.
  - 识别：I voted for a contestant in the sudden Brooks Hospital in Cambridge, for leg.
- `10376367261184486277.wav` CER 30.2%
  - 参考："Ihr thermisches Verhalten ist nicht so gleichmäßig wie das großer Höhlen auf der Erde, die oft eine ziemlich konstante Temperatur haben, aber es stimmt damit überein, dass es sich um tiefe Löcher im Boden handelt"", sagte Glen Cushing vom Astrogeologie-Team des United States Geological Survey (USGS) und der Northern Arizona University in Flagstaff, Arizona."
  - 识别：Hier termisches Verhalten ist nicht so gleichmäßig wie das große Höllen auf der Erde, die oft eine ziemlich konstante Temperatur haben. Aber es stimmt damit überein, dass es sich um tiefe Löcher im Boden handelt, sagte Glenn Kraschen vom Astrogiologie-Team des Units

### Japanese / ReazonSpeech k2 v2
- `1166557209541388763.wav` CER 71.7%
  - 参考：サンクトペテルブルクのクルーズには街での滞在時間が含まれています。クルーズの乗客はビザが免除されます（条件を確認してください）。
  - 识别：クルーズの乗客はビザが免除されます
- `10413033184319592580.wav` CER 67.3%
  - 参考：私たちは植物で家を造り、植物から衣類を作ります。日々食べる食材の多くが植物です。植物がなければ動物は生きていけません。
  - 识别：植物がなければ動物は生きていけません
- `11411683150648548702.wav` CER 53.7%
  - 参考：カジノでは通常、特別な飲食やエンターテイメントを用意しています。ゲストが気分良く施設内に留まるようにするためです。
  - 识别：カジノでは通常特別ライン宿やエンターテインメントを用意しています

### Japanese / SenseVoice 2025-09-09 (WSYue)
- `11839084987799465654.wav` CER 93.5%
  - 参考：ジャンカルロ・フィジケラがマシンをコントロールできなくなり、スタート直後にレースから脱落しました。
  - 识别：CONTOLOL 直后脱落
- `10228875750894489453.wav` CER 89.2%
  - 参考：メインステージの音楽が終わっても、フェスティバルには夜遅くまで演奏を流し続けるセクションがあるかもしれないことを覚えておいてください。
  - 识别：音乐夜演奏流続覚
- `11728507178922830420.wav` CER 88.5%
  - 参考：しかし、シェンゲン圏は、この点では一国のように機能します。
  - 识别：新源券点一刻记能

### Japanese / SenseVoice 2024-07-17
- `12061962287389604640.wav` CER 36.1%
  - 参考：pH レベルは、検査した化学物質に含まれる水素イオン（pHのH）の量で示されます。
  - 识别：レールは、検索者化学物質に含まれる水素用の量で示されます。
- `12552266672764386489.wav` CER 31.7%
  - 参考：月の引力で地球に潮の満ち引きが起きるように、天の川は射手座銀河にも力を及ぼしています。
  - 识别：月のイドで地球に使用の未識が起きるように、天川は射ザガにも力を及ぼしています。
- `11728507178922830420.wav` CER 26.9%
  - 参考：しかし、シェンゲン圏は、この点では一国のように機能します。
  - 识别：しかし、宣源権は この点では一刻の ように機能します。

### Japanese / Whisper base
- `11711375377447019354.wav` CER 112.9%
  - 参考：シンガポールを訪れた彼は、ウォン・カン・セン副首相から出迎えを受け、リー・シェン・ルーン首相と会談で貿易やテロ問題について話し合いました。
  - 识别：新型ポールをれたからは、1感染があるデモ改良系、D-Shen-Roon-Short-Kaidan-D-Boi-Kia-Teremon-Dai-Ti-Tanashima-Shita。
- `11688140237357537206.wav` CER 65.2%
  - 参考：馬車に乗った彼らは、国王と王妃に向かって叫び、脅しの言葉を浴びせる暴徒に囲まれてパリに戻りました。
  - 识别：バシャリーのったカレラは、ココオトをひり向かって叫び、 下ろしのことはびセレボートに書こまれて、パリにりました。
- `12526861755097705133.wav` CER 60.5%
  - 参考：警察のチャンドラ・シェカール・ソランキ総監によれば、被告人は顔を隠して法廷に出頭したそうです。
  - 识别：警察のちゃんとらしくある空気相におれば 否に向かうをして方程に出動したそうです

### Korean / Zipformer ko
- `14474925680236221199.wav` CER 100.0%
  - 参考：지각(crust)이 더 얇기 때문에 가까운 쪽에 마리아(maria)가 더 많을 수 있다. 용암이 수면 위로 솟아오르기가 더 쉬운 조건이다.
  - 识别：
- `14336290879561136744.wav` CER 100.0%
  - 参考：구조물을 긁거나 낙서해서 현장을 더럽히지 마시오.
  - 识别：
- `14286397823994122188.wav` CER 100.0%
  - 参考：자신의 의견이 아닌 정부의 의견을 듣고 싶을 수도 있지만, 각국 정부의 조언들은 자국민에 맞춰져 있다.
  - 识别：

### Korean / SenseVoice 2025-09-09 (WSYue)
- `13945086694115527705.wav` CER 100.0%
  - 参考：그는 싱가포르 부총리 웡칸셍의 환영을 받았고 싱가포르 총리 리셴룽과 무역 및 테러 문제를 논의하였습니다.
  - 识别：嫩 SN加L冲王堪性E花  SIN加L冲李 SC伦舞O MEE门都
- `12788118264869112521.wav` CER 100.0%
  - 参考：에어로스미스는 투어 콘서트의 남아있는 공연들을 취소했다.
  - 识别：EOL史M斯 TO  CONST那一公D驱T
- `10231261934313595333.wav` CER 100.0%
  - 参考：교전이 발발한 직후 영국은 독일에 대한 해상 봉쇄를 시작한다.
  - 识别：桥真快直古羊途计轻生梦些

### Korean / SenseVoice 2024-07-17
- `12822450452598008094.wav` CER 34.2%
  - 参考：로스비 수(Rossby number)가 작을수록 자기 역전 면에서 항성의 활동성 역시 작다.
  - 识别：로스비 수 작을수록 자기 역전면에서 항성의 활동성 역시 작다.
- `12050639697247972736.wav` CER 25.0%
  - 参考：코차모 밸리(Cochamó Valley) - 남아메리카의 요세미티 계곡으로 알려진 칠레 최고의 등반지이며 큰 화강암 벽과 바위산이 다양하게 존재합니다.
  - 识别：코차모벨리 남아메카의 요새미티 계곡으로 알려진 칠레 최고의 등반지이며 큰 화강암 벽과 바위산이 다양하게 존재합니다.
- `10315643998667715978.wav` CER 24.2%
  - 参考：사건 발생 이후, 깁슨(Gibson)은 병원으로 이송되었으나 얼마 후 숨을 거뒀다.
  - 识别：사건 발생 이후깁씨는 병원으로 이송되었으나 얼마 후 숨을 거뒀다.

### Korean / Whisper base
- `13876340249631994162.wav` CER 52.9%
  - 参考：지역별 및 계절별 악천후 현상에는 폭풍설, 눈보라, 진눈깨비 및 황사 등이 포함됩니다.
  - 识别：지역에 개 악천호 현상에는 폭설  는 개이 사 등이 포니다.
- `12988724693829869998.wav` CER 48.4%
  - 参考：모로코 술탄은 다루 이 바드야(Daru l-Badya)라는 도시로 재건축하고, 여기에 무역 거점을 세운 스페인 상인들은 이곳을 카사블랑카라고 불렀다.
  - 识别：모코 술타는 다이 마는 도시 재하고 여기에 무 거점을 세운 스인 상인은 이곳을  블가고 불다.
- `12822450452598008094.wav` CER 47.4%
  - 参考：로스비 수(Rossby number)가 작을수록 자기 역전 면에서 항성의 활동성 역시 작다.
  - 识别：로스 수가 자글수 자기 역자에서 항성의 활동성 역시 작다.

### Portuguese / Parakeet TDT 0.6B v3
- `1060610497899453249.wav` CER 22.7%
  - 参考：A palavra civilização vem do latim civilis, que significa civil, relacionado com o latim civis, que significa cidadão, e civitas, que significa cidade ou cidade-estado, e que também, de alguma forma, define o tamanho da sociedade.
  - 识别：A palavra civilização vem do latim civilis, que significa civil, relacionada com o latim civis, que significa cidadão. E que também, de alguma forma, definem o tamanho da sociedade.
- `11661983053571683307.wav` CER 13.4%
  - 参考：Aconteceu novamente no mesmo mês em Mashhad, outro avião comercial entrou em uma pista e atingiu uma parede, matando dezessete pessoas.
  - 识别：Aconteceu novamente no mês em Mashhat, outro avião comercial entrou em uma pista e atingiu uma parede matando 17 pessoas.
- `10103427908302527934.wav` CER 11.1%
  - 参考：O romantismo tinha um grande elemento de determinismo cultural, extraído de escritores como Goethe, Fichte e Schlegel.
  - 识别：O romantismo tinha um grande elemento de determinismo cultural, extraído de escritores como Gold, Fish e Skellagel.

### Portuguese / NeMo FastConformer CTC pt
- `10103427908302527934.wav` CER 10.1%
  - 参考：O romantismo tinha um grande elemento de determinismo cultural, extraído de escritores como Goethe, Fichte e Schlegel.
  - 识别：O romantismo tinha um grande elemento de determinismo cultural, extraído de escritores como Golth, ist e kelagel
- `10083615766068544069.wav` CER 9.7%
  - 参考：Giancarlo Fisichella perdeu o controle do carro e acabou a corrida logo após a largada.
  - 识别：Sancaro feziquella, perdeu o controle do carro e acabou a corrida logo após a largada.
- `11675925594484415342.wav` CER 9.1%
  - 参考：Inicialmente, ele foi hospitalizado no James Paget Hospital, em Great Yarmouth.
  - 识别：IInicialmente, ele foi hospitalizado no James Pargett Hospital em Grerate Armooth.

### Portuguese / NeMo FastConformer RNNT pt
- `10083615766068544069.wav` CER 12.5%
  - 参考：Giancarlo Fisichella perdeu o controle do carro e acabou a corrida logo após a largada.
  - 识别：Xancarro fez se que ela, perdeu o controle do carro e acabou a corrida logo após a largada.
- `11056406331523792259.wav` CER 8.0%
  - 参考：Em seus primeiros dias, o programa era apresentado exclusivamente no longevo TogiNet Radio, site focado em rádio falado.
  - 识别：Em seus primeiros dias, o programa era apresentado exclusivamente no Nondval Tognet Rader, site focado em rádio falado.
- `1123924841621724509.wav` CER 7.9%
  - 参考：Durante a luta pela independência organizada pelo movimento Mau, uma reunião pacífica na cidade resultou no assassinato do chefe supremo Tupua Tamasese Lealofi III.
  - 识别：Durante a luta pela Independência Organizada pelo movimento Mau, uma reunião pacífica na cidade resultou no assassinato do chefe sSupremo Tupuatamascesse Lealoffi, terceiro.

### Portuguese / Whisper base
- `10083615766068544069.wav` CER 30.6%
  - 参考：Giancarlo Fisichella perdeu o controle do carro e acabou a corrida logo após a largada.
  - 识别：O checo de carros e o fisiquela perdeu o controle do carro e acabou a corrida no oposto da largada.
- `11697137159751372374.wav` CER 25.9%
  - 参考："Seu comportamento térmico não é tão estável quanto as grandes cavernas do planeta Terra, que costumam preservar uma temperatura bem constante, mas é consistente com o fato de serem buracos de grande profundidade no solo", afirmou Glen Cushing, da Equipe de Astrogeologia do Serviço Geológico dos Estados Unidos (USGS) e da Universidade do Norte do Arizona, localizada em Flagstaff, Arizona.
  - 识别：Seu comportamento térmico não é tão estável com das grandes cavernas do planeta, que costuma preservar uma temperatura bem constante, mas é consistente com o fato de serem buracos de grande profundidade no solo afirmou Glenn Cushion da equipe de astról, geologia do serviço geólico dos Estados Unidos,
- `10975421434030537102.wav` CER 24.1%
  - 参考：Se você não tem o costume de dirigir em estradas do interior, tenha juízo: declives íngremes, pistas estreitas e curvas acentuadas são comuns.
  - 识别：Se você não tem a construída de dirigir em estratos no interior, tem a juízo da Clívor, em gremes, pistas, trem e daí corvas, assintuadas são comuns.

### Russian / GigaAM v3 CTC
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пиаш определяется степенью насыщенности ионами водорода аш пиаш исследуемого химического соединения
- `11720526414504187623.wav` CER 9.0%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：в начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн радио тоги нет радио посвященного радиобеседам
- `10128565049419377161.wav` CER 8.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：также может быть выгодно купить вилдкарт которая позволяет посещать национальные парки юар либо выборочно либо все

### Russian / GigaAM v2 CTC
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пиаш определяется степенью насыщенности ионами водорода аш пиаш исследуемого химического соединения
- `11720526414504187623.wav` CER 9.0%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：в начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн радио тогинет радио посвященного радиобеседам
- `10128565049419377161.wav` CER 8.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：также может быть выгодно купить вилдкарт которая позволяет посещать национальные парки юар либо выборочно либо все

### Russian / GigaAM v1 CTC
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пиаш определяется степенью насыщенности ионами водорода аш пиаш исследуемого химического соединения
- `11105320946375128409.wav` CER 9.1%
  - 参考：В последствие он был перемещён в госпиталь Адденбрук в Кембридже.
  - 识别：впоследствии он был перемещен в госпиталь аденбруг в кембредже
- `11720526414504187623.wav` CER 9.0%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：в начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн радио тоги нет радио посвященного радиобеседам

### Russian / GigaAM v3 RNNT
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пиаш определяется степенью насыщенности ионами водорода аш пиаш исследуемого химического соединения
- `11720526414504187623.wav` CER 9.0%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：в начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн радио тоги нет радио посвященного радиобеседам
- `10128565049419377161.wav` CER 8.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：также может быть выгодно купить вилдкард которая позволяет посещать национальные парки юар либо выборочно либо все

### Russian / GigaAM v2 RNNT
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пи аш определяется степенью насыщенности ионами водорода аш пи аш исследуемого химического соединения
- `10270509144878549012.wav` CER 10.0%
  - 参考：Лаккха Сингх исполнил чхаппан бхог бхаджан.  Певец Раджу Кханделвал пел вместе с ним.
  - 识别：лак хассинг исполнил чхаппанпхоп хаджан певец раджо кханделвал пел вместе с ним
- `11720526414504187623.wav` CER 9.0%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：в начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн радио тогинет радио посвященного радиобеседам

### Russian / GigaAM v1 RNNT
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пиаш определяется степенью насыщенности ионамиводорода аш пиаш исследуемого химического соединения
- `11105320946375128409.wav` CER 9.1%
  - 参考：В последствие он был перемещён в госпиталь Адденбрук в Кембридже.
  - 识别：в последствии он был перемещен в госпиталь оденбук в кембридже
- `10128565049419377161.wav` CER 9.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：также может быть выгодно купить уилд карту которая позволяет посещать национальные парки юар либо выборочно либо все

### Russian / GigaAM v3 RNNT (punct)
- `10270509144878549012.wav` CER 14.3%
  - 参考：Лаккха Сингх исполнил чхаппан бхог бхаджан.  Певец Раджу Кханделвал пел вместе с ним.
  - 识别：Лак Хасинг исполнил Чхабхопхаджан. Певец Раджу Канделал пел вместе с ним.
- `11720526414504187623.wav` CER 9.7%
  - 参考：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио TogiNet Radio, посвященного радиобеседам.
  - 识别：В начальный период своего существования это шоу транслировалось только на сайте давно существующего онлайн-радио Тогинет радио, посвящённого радиобеседам.
- `10894987436796657762.wav` CER 8.6%
  - 参考：В репортажах по телевидению сообщается  о белом дыме, идущем от завода.
  - 识别：В репортажах по телевизору сообщается о белом дыме, идущем от завода.

### Russian / Parakeet TDT 0.6B v3
- `10270509144878549012.wav` CER 8.6%
  - 参考：Лаккха Сингх исполнил чхаппан бхог бхаджан.  Певец Раджу Кханделвал пел вместе с ним.
  - 识别：Лакхасинг исполнил Чхапан Хопхаджан, певец Раджу Кханделвал пел вместе с ним.
- `10128565049419377161.wav` CER 8.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：Также может быть выгодно купить вилдкар, которая позволяет посещать национальные парки ЮАР, либо выборочно, либо все.
- `11105320946375128409.wav` CER 7.3%
  - 参考：В последствие он был перемещён в госпиталь Адденбрук в Кембридже.
  - 识别：Впоследствии он был перемещен в госпиталь Аденбрук в Кембридж.

### Russian / Whisper base
- `12252376104195968963.wav` CER 34.0%
  - 参考：Для "Спрингбокс" это прервало пятиматчевую серию поражений.
  - 识别：Для Springbox это прервала 5 мачивую серию поражений.
- `11625131477558873455.wav` CER 28.6%
  - 参考：Первоначально он был госпитализирован в госпитале Джеймса Педжета в Грейт-Ярмуте.
  - 识别：Первого начальна он был госпитализирован в госпитале Jameson PageZeta в Грейдер Муте.
- `10270509144878549012.wav` CER 21.4%
  - 参考：Лаккха Сингх исполнил чхаппан бхог бхаджан.  Певец Раджу Кханделвал пел вместе с ним.
  - 识别：Лака Синг исполнил чхап-пан-пхап-хаджан, бьет с раджу. Хандал Валппел вместе с ним.

### Russian / Vosk ru
- `1027147027471254370.wav` CER 10.5%
  - 参考：Показатель pH определяется степенью насыщенности ионами водорода (H в pH) исследуемого химического соединения.
  - 识别：показатель пи аш определяется степенью насыщенности ионами водорода аш пиаш исследуемого химического соединения
- `10128565049419377161.wav` CER 10.1%
  - 参考：Также может быть выгодно купить Wild Card, которая позволяет посещать национальные парки ЮАР либо выборочно, либо все.
  - 识别：также может быть выгодно купить вилткарт которая позволяет посещать национальные парки юар либо выброшно либо все
- `10270509144878549012.wav` CER 10.0%
  - 参考：Лаккха Сингх исполнил чхаппан бхог бхаджан.  Певец Раджу Кханделвал пел вместе с ним.
  - 识别：лак хосинг исполнил чхапанхоп хаджан певец раджу кханделвал пел вместе с ним

### Spanish / Parakeet TDT 0.6B v3
- `11449333388417771996.wav` CER 10.7%
  - 参考：En la mañana de ayer, en Gaziantep, Turquía, un coche bomba mató a dos oficiales de policía e hirió a más de otras veinte personas.
  - 识别：En la mañana de ayer, en Gazetp, Turquía, un coche bomba mató a dos oficiales de policía y hirió a más de otras 20 personas.
- `10944171741786227410.wav` CER 10.1%
  - 参考：No es necesario aclarar que, si domina alguna lengua romance, le resultará más sencillo aprender portugués.
  - 识别：No es necesario aclarar que, si domina alguna lengua romance, le resultará más sencill portugués.
- `1008205615316519958.wav` CER 9.8%
  - 参考：Fue tanta la cantidad de gente que se concentró, que no todos pudieron acceder al funeral en la Plaza de San Pedro.
  - 识别：Fue tanta la cantidad de gente que se concentró que no todos pudier al funeral en la plaza de San Pedro.

### Spanish / Whisper base
- `10889086033863410971.wav` CER 19.2%
  - 参考：El filósofo Aristóteles desarrolló la teoría de que todo está formado por una combinación de uno o varios de cuatro elementos: tierra, agua, aire y fuego.
  - 识别：El filósofo aristotelec de desarrollo de la teoría de que todo está formado por una combinación de 1 o 4 elementos. Tierra agua aire y fuego.
- `11449333388417771996.wav` CER 15.5%
  - 参考：En la mañana de ayer, en Gaziantep, Turquía, un coche bomba mató a dos oficiales de policía e hirió a más de otras veinte personas.
  - 识别：en la mañana ayer en gaseantep turquía un coche bomba a todos oficiales de policía y heriva más de otras 20 personas
- `10854190301744279509.wav` CER 13.3%
  - 参考：Las manadas están formadas por uno a tres machos adultos del mismo grupo familiar, unidos con hasta treinta hembras y sus crías.
  - 识别：Las manadas se están formadas por uno a 3 machos adultos del mismo grupo familiar, unidos con hasta 30 embras y sus crías.

### Spanish / NeMo FastConformer CTC es
- `10492774437691150637.wav` CER 8.9%
  - 参考：Duvall, que está casado y tiene dos hijos adultos, no causó una buena impresión a Miller, que fue a quien le relató la historia.
  - 识别：Duval, que está casado y tiene dos hizo, dos hijos adultos, no causó una buena impresión a Míller, que fue a quien le relató la historia.
- `10800534950017924150.wav` CER 6.8%
  - 参考：El anuncio se hizo tras la conversación telefónica que Trump mantuvo con con el presidente de Turquía, Recep Tayyip Erdoğan.
  - 识别：El anuncio se hizo tras la conversación telefónica que Trump mantuvo con el presidente de Turquía, Resep Tayib Erdogan.
- `11603681517704231177.wav` CER 6.2%
  - 参考：Cuanto más baja sea la tensión, más positiva es la fuerza de la vida con la que contemos. Cada uno de nosotros tiene el potencial de hallar paz y dicha absolutas.
  - 识别：Cuanto más va a hacer la atención más positiva es la fuerza de la vida con la que contemos, cada uno de nosotros tiene el potencial de hallar paz y dichas absolutas.

### Thai / Zipformer GigaSpeech2 th
- `11075400783402805173.wav` CER 211.0%
  - 参考：ปัจจุบันนี้ แมลงที่ไม่สามารถพับปีกไปข้างหลังได้มีเพียงแมลงปอและแมลงชีปะขาว
  - 识别：ปัจจุบันนี้แมลงที่ไม่สามารถพับปีกไปด้านไปข้างหลังได้มีเพียงแมลงปอและแมลงชีปากขาวปัจจุบันนี้แมลงที่ไม่สามารถพับปีกไปด้านหลังได้มีเพียงแมลงปอและแมลงชีปากขาวปัจจุบันนี้แมลงที่ไม่สามารถพับปีกไปข้างหลังได้มีเพียงแมลงปอและแมลงชีปะขาว
- `10993366174322153571.wav` CER 26.4%
  - 参考：องค์กรประชาชนแอฟริกาใต้ (SWAPO) ซึ่งเป็นพรรครัฐบาลยังคงครองเสียงข้างมากในการเลือกตั้งรัฐสภา
  - 识别：องค์กรประชาชนแอฟริกาใต้เอสเตอร์เป็นยูเอพีโอซึ่งเป็นพักรัฐบาลยังคงครองเสียงข้างมากในการเลือกตั้งรัฐสภา
- `11559893457595283546.wav` CER 24.1%
  - 参考：องค์กรประชาชนแอฟริกาใต้ (SWAPO) ซึ่งเป็นพรรครัฐบาลยังคงครองเสียงข้างมากในการเลือกตั้งรัฐสภา
  - 识别：องค์กรประชาชนแอฟริกาใต้เอสเตอร์บียูเอพีโอซึ่งเป็นพักรัฐบาลยังคงครองเสียงข้างมากในการเลือกตั้งรัฐสภา

### Thai / Whisper base
- `11535681910822974139.wav` CER 101.0%
  - 参考：พืชผลิตก๊าซออกซิเจนที่มนุษย์ใช้หายใจและรับเอาคาร์บอนไดออกไซด์ที่มนุษย์ขับออกมา (ซึ่งก็คือการหายใจออก) เข้าไป
  - 识别：Pull palette + oxygen in the menu chai hai jai.Let's wrap our carbon dioxide in the menu chai hot maa.Sing a khe ghan hai jai o.Chao bai.
- `11651580432035802486.wav` CER 99.1%
  - 参考：เมืองนี้ยังเป็นฐานที่ตั้งสำหรับการปีนภูเขาไฟ Nyiragongo พร้อมด้วยการเดินตามหากอริลลาภูเขาที่ถูกที่สุดในแอฟริกา
  - 识别：คุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณคุณ
- `1019167928630402440.wav` CER 98.2%
  - 参考：Oliver Sacks ระบุในงานวิจัยของเขาที่ชื่อว่า ว่าด้วยสุนทรพจน์ของประธานาธิบดี ว่าผู้ที่ไม่สามารถเข้าใจคำพูดเนื่องจากสมองกระทบกระเทือนจะสามารถประเมินความจริงใจได้อย่างไร
  - 识别：Everyone is sex and the best way to get the best of the world.

### Vietnamese / Zipformer vi
- `10631977326382455765.wav` CER 60.9%
  - 参考：Cuối Chủ Nhật, Tổng thống Hoa Kỳ Donald Trump, trong một tuyên bố đã gửi thông qua thư ký báo chí thông báo quân đội Mỹ sẽ rút quân khỏi Syria.
  - 识别：CUỐI CHỦ NHẬT TỔNG THỐNG HOA KỲ DONALD TRUMP KHỎI SI A
- `10945777525729584175.wav` CER 31.2%
  - 参考：Ion hydro là proton đã bị loại bỏ electron (vì nguyên tử hydro bao gồm một proton và một electron).
  - 识别：EOM HAI PRO LÀ PROTENG ĐÃ BỊ LOẠI BỎ ETROM TIỀN NGUYÊN TỬ HAIRO BAO GỒM MỘT PROM VÀ MỘT ETRO
- `11672619076350815814.wav` CER 25.0%
  - 参考：Ông ta sau đó được chuyển đến Bệnh viện Addenbrooke ở Cambridge.
  - 识别：ÔNG TA SAU ĐÓ ĐƯỢC CHUYỂN ĐẾN BỆNH VIỆN A ĐEN GỐC Ở CAM RÍ

### Vietnamese / Whisper base
- `10684685439757401963.wav` CER 100.0%
  - 参考：Thời gian di chuyển của tàu Saint Petersburg bao gồm cả thời gian trong thành phố. Hành khách trên tàu được miễn thị thực (kiểm tra các điều khoản).
  - 识别：Tại gian dưi chuyển cô tao xin, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía, phía,
- `10865708684535274985.wav` CER 99.3%
  - 参考：Tác phẩm của ông có phẩm chất và chi tiết được tán dương đến mức ông là một trong số rất ít "tên tuổi lớn" nổi bật trong giới sưu tầm tem. Một số nhà sưu tập chỉ chuyên sưu tầm tác phẩm của ông.
  - 识别：Đ
- `11806133943011932690.wav` CER 83.7%
  - 参考："Đặc tính nhiệt của chúng không ổn định như các hang động lớn trên Trái Đất vốn thường duy trì nhiệt độ khá ổn định, nhưng điều này phù hợp với các hố sâu trong lòng đất", Glen Cushing thuộc Toán Địa Chất Học, Cơ quan Thăm dò Địa chất Hoa Kỳ (USGS) và trường Northern Arizona University ở Flagstaff, Arizona, cho biết.
  - 识别：Nguyễn Điều này phù hợp với khói xao trong lòng đắc lên, cú xin, thuộc, tán đĩa, chắc hợp, cơ quan thăm dọ để chắc hoa k, u-s, g-s, g-j, norder, g-j, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi, dừa vợi

### Tagalog / NeMo FastConformer tl
- `10650397448123148596.wav` CER 18.5%
  - 参考：Mayroon siyempreng mga paliwanag sa teolohiyang Kristiyano para sa tradisyong ito, nguni't malamang na ito ay isang ritwal ng Tagsibol at Kasaganaan bago pa ang Kristyanismo.
  - 识别：mayroong paliwanag sa teolohiyang kristiyano para sa tradisyong ito nguni't malalaman na ito malamang na ito ay isang ritwal ng tagsibol at kasaganaan bago pa ang kristiyanismo
- `11304311345276491753.wav` CER 18.1%
  - 参考：CochamÃ³ Valley - ang nangungunang lugar na inaakyat sa Chile, na kilala bilang ang Yosemite ng South America, at may iba't ibang malalaking granitong pader at dalisdis.
  - 识别：kasamali ang unang lugar na inaakyat sa chile na kilala bilang angity ng south america at may iba't ibang malalaking granitong pader at dalisttis
- `11692642647176975996.wav` CER 14.9%
  - 参考：Ginawa ang anunsyo pagkatapos makipag-usap sa telepono ni Trump sa Pangulo ng Turkey na si Recep Tayyip ErdoÄŸan.
  - 识别：ginawa ang anunsiyo pagkatapos makipagusap sa telepono ni trunk sa pangulo ng turkey na si regierdan

### Tagalog / Whisper base
- `10105826173618659509.wav` CER 114.4%
  - 参考：Ang mga opisyal na lengguwahe ng Barcelona ay Catalan at Espanyol. Mga kalahati ang mas gustong magsalita ng Catalan, naiintindihan ito ng karamihan, at halos ay lahat alam ang Espanyol.
  - 识别：ang ang apexan na lingguain ng Barcelona ay katalan at espanyol ang kalahati ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang
- `11579079956575993195.wav` CER 100.0%
  - 参考：Sa katunayan, hindi talaga ito madaling mahanap kahit na may nakakaalam na umiral ito. Kapag nasa loob na ng kuweba, talagang nakahiwalay na.
  - 识别：سقطنايا اندي تلكي تمدلي معينة كائد من عقى للمنة أمير لتو كبقنا سلوبنا نقوى با تلكي نقعى او لين
- `10552017754701384075.wav` CER 89.0%
  - 参考：Ang mga paglalakbay sa dagat sa Saint Petersburg ay nangangailangan ng paggugol ng panahon sa bayan. Ang mga manlalakbay ay libre mula sa mga pangangailangan para sa bisa (tingnan ang mga tuntunin).
  - 识别：ang mga paglalak ba sa daga ng Saint Petersburg ang ang ang ang ang pagwagul ng panahon sa bayan ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang ang

### Tagalog / Qwen3-ASR 0.6B
- `10796235136732120924.wav` CER 34.2%
  - 参考：Ang kanilang mainit na pag-uugali ay hindi kasing nakapirmi na tulad ng malalaking kuweba sa Earth na malimit nagpapanatili ng medyo hindi nagbabagong temperatura, ngunit ito ay tugmang-tugma sa mga iyon bilang malalalim na butas sa lupa," sabi ni Glen Cushing ng United States Geological Survey (USGS) Astrogeology Team at ng Northern Arizona University na nasa Flagstaff, Arizona.
  - 识别：Ang kanyang mga inihintan pagugadig hindi kasi ng nakaperym na tulad ng malalaking koyab sa arred, naman limit ng papanatidin ng madjong hindi nakbabago ng tempel atura. Muna ituito gumantug mga semang ayon bilang malalim na butas sa lupat. Sabihin, Glenn Kushing ng United States Geological Survey, astrologjality at na
- `10650397448123148596.wav` CER 28.1%
  - 参考：Mayroon siyempreng mga paliwanag sa teolohiyang Kristiyano para sa tradisyong ito, nguni't malamang na ito ay isang ritwal ng Tagsibol at Kasaganaan bago pa ang Kristyanismo.
  - 识别：Miran champeneng paliwangan sa tiyolohi ang Christiano, para sa tradisyon ito. Muna, madalang na ito, maliwang na ito ay isang ritual ng taksibol at kasaganaan bag-o pa ang Christianismo.
- `1016394520605232520.wav` CER 27.4%
  - 参考：Matapos maganap ang aksidente, si Gibson ay dinala sa ospital nguni't namatay di-nagtagal pagkatapos.
  - 识别：Mataasong maganap ang accidente sa Gibson Hayden na lalaso atpetal, mung nitamatay din ng tagal po ketapos.

### Tagalog / Whisper small
- `10650397448123148596.wav` CER 24.0%
  - 参考：Mayroon siyempreng mga paliwanag sa teolohiyang Kristiyano para sa tradisyong ito, nguni't malamang na ito ay isang ritwal ng Tagsibol at Kasaganaan bago pa ang Kristyanismo.
  - 识别：Mayran siyang parang paliwanag sa Teolo Hiyang Cristiano para sa tradusiyong ito. Mungunit, malalaman na ito, malamang na ito ay isang ritual ng tagsibul at kasaganahan bago pa ang Cristianismo.
- `10186072561424790316.wav` CER 20.8%
  - 参考："Hindi ito mauuwi sa pamamaalam. Pagsasara ito ng isang yugto at pagbubukas ng panibago."
  - 识别：Hindi ito mga uwizap amamamaalam, pagsasari ito ang isang yukto at pagpapokasit ang panibago.
- `10912486236040763164.wav` CER 18.7%
  - 参考：Ang opisyal na pera ng Falklands ay ang Falkland pound (FKP) na ang halaga ay katumbas ng isang British pound (GBP).
  - 识别：Ang official na pera ng Poclants ay ang Poclant Pound o FKP na ang halaga ay katumbas ng isang British Pound o GDP.

### Tagalog / Omnilingual 300M
- `10650397448123148596.wav` CER 19.9%
  - 参考：Mayroon siyempreng mga paliwanag sa teolohiyang Kristiyano para sa tradisyong ito, nguni't malamang na ito ay isang ritwal ng Tagsibol at Kasaganaan bago pa ang Kristyanismo.
  - 识别：meron syempreng paliwanag sa teolohi ang cristyano para sa tradusyong ito ngunit malalamang na ito malamang na ito ay isang ritwal ng taxibol at kasaganaan bago ba ang kristyanismo
- `11692642647176975996.wav` CER 17.0%
  - 参考：Ginawa ang anunsyo pagkatapos makipag-usap sa telepono ni Trump sa Pangulo ng Turkey na si Recep Tayyip ErdoÄŸan.
  - 识别：ginawaang anon siyo pagkatapos makipag usap sa telepono ni trunk sa pangulo ng terqi nasi regyptai erduan
- `10075725219579551953.wav` CER 14.6%
  - 参考：Ang Finland ay napakagandang destinasyon para sa pagbabangka. Ang "Lupain ng libo-libong lawa" ay mayroon ding libo-libong isla, na nasa mga lawa at mga kapuluan sa kahabaan ng baybayin.
  - 识别：ang finland ay napakagandang destinasyon para sa pagbabangka ang lupain ng lawa ay mayroonding isla na nasa mga lawa at mga kapuloan sa kahabaan ng baybahin

### Tagalog / Omnilingual 1B
- `10650397448123148596.wav` CER 15.1%
  - 参考：Mayroon siyempreng mga paliwanag sa teolohiyang Kristiyano para sa tradisyong ito, nguni't malamang na ito ay isang ritwal ng Tagsibol at Kasaganaan bago pa ang Kristyanismo.
  - 识别：meyron siyempreng paliwanag sa teolohiyang kristyano para sa tradusyong ito nguni't malalamang na ito malamang na ito ay isang ritwal ng tagsibol at kasaganaan bago pa ang kristyanismo
- `11865776414324297906.wav` CER 10.7%
  - 参考：Karaniwan ay may mga alok silang espesyal na pagkain, inumin at libangan, upang mapanatiling maganda ang pakiramdam ng mga bisita, at mapanatili sila sa lugar.
  - 识别：kariniwan ay may mga alok silang espesyal na pagkain inumin at libangan upang manatiling mapagandamaganda ang pakiramdam ng mga bisita at mapanatiliti sila sa lugar
- `11304311345276491753.wav` CER 10.1%
  - 参考：CochamÃ³ Valley - ang nangungunang lugar na inaakyat sa Chile, na kilala bilang ang Yosemite ng South America, at may iba't ibang malalaking granitong pader at dalisdis.
  - 识别：kacsamavalli ang nangungunang lugar na inaakkat sa tsile na kilalabilang ang yasimiti ng south america at may iba't ibang malalaking granitong pader at dalistis

### Italian / Parakeet TDT 0.6B v3
- `1184559682288866813.wav` CER 18.7%
  - 参考：L'ufficio del governatore ha riferito che tra i feriti, diciannove erano agenti di polizia.
  - 识别：L'ufficio del governatore ha riferito, che 3 feriti, 19 erano agenti di polizia.
- `1131233198965973441.wav` CER 12.6%
  - 参考：L'australiano Mitchell Gourley ha ottenuto l'undicesimo posto nel Super-G maschile, mentre l'avversario ceco Oldrich Jelinek ha ottenuto sedicesimo posto nella stessa specialità.
  - 识别：L'australiano Michael Girley ha ottenuto l'undicesimo posto nel Super G maschile, mentre l'avversario ceco, Hrick Jellinek ha ottenuto il 16<unk> posto nella stessa specialità.
- `12246544248459645757.wav` CER 8.6%
  - 参考：I successivi concerti del tour sono stati annullati dagli Aerosmith.
  - 识别：I successivi concerti del tour sono stati annullati dai Ermith.

### Italian / NeMo FastConformer 10-lang
- `12013566675598043608.wav` CER 12.9%
  - 参考：Costruita dagli Egizi nel terzo secolo a.C., la Grande Piramide è una delle grandi strutture piramidali edificate in onore del faraone morto.
  - 识别：Costruita dagli Egizi nel I secolo avanti Cristo, la grande piramide è una delle grandi strutture piramidali edificate in onore del faraone morto.
- `11697287891158582963.wav` CER 9.4%
  - 参考：È stato poi trasferito all'Addenbrooke's Hospital di Cambridge.
  - 识别：È stato poi trasferito all'Edinbroks Hospital di Cambridge.
- `10159398106668856572.wav` CER 8.1%
  - 参考：Se avete visto il film "Il mistero dei Templari", potreste pensare che sul retro della Dichiarazione d'Indipendenza sia disegnata una mappa del tesoro.
  - 识别：Se avete visto il film, il mistero dei Templari, potreste pensare che il surretro della digarazia d'indipendenza sia disegnato una mappa del tesoro

### Italian / Qwen3-ASR 0.6B
- `10896702735150958436.wav` CER 36.4%
  - 参考：Da quanto appreso dal sito web di notizie di intrattenimento TMZ, il fotografo ha fermato il suo veicolo dalla parte opposta di Sepulveda Boulevard e ha cercato di fotografare l'ufficiale di polizia che dirigeva il traffico allo stop, poi ha attraversato la strada e ha proseguito, costringendo l'ufficiale di polizia della California Highway Patrol che stava effettuando il controllo del traffico allo stop a intimargli per due volte di tornare indietro.
  - 识别：Da quanto appreso dal sito web di notizie di intrattenimento TMZ, il fotografo ha fermato il suo veicolo dalla parte opposta di sepolvio da Boulevard e ha cercato di fotografare l'ufficiale di polizia che dirigeva il traffico allo stop. Poi ha attraversato la strada e ha proseguito, costringendo
- `10765581971846086650.wav` CER 20.3%
  - 参考：Da quanto appreso dal sito web di notizie di intrattenimento TMZ, il fotografo ha fermato il suo veicolo dalla parte opposta di Sepulveda Boulevard e ha cercato di fotografare l'ufficiale di polizia che dirigeva il traffico allo stop, poi ha attraversato la strada e ha proseguito, costringendo l'ufficiale di polizia della California Highway Patrol che stava effettuando il controllo del traffico allo stop a intimargli per due volte di tornare indietro.
  - 识别：Da quanto appreso dal sito web di notizie di intrattenimento TMZ, il fotografo ha fermato il suo veicolo dalla parte opposta di Sepulveda Boulevard e ha cercato di fotografare l'ufficiale di polizia che dirigeva il traffico allo stop. Poi ha attraversato la strada e ha proseguito, costringendo l'ufficiale di polizia della California Highway Patrol che stava eff
- `1184559682288866813.wav` CER 18.7%
  - 参考：L'ufficio del governatore ha riferito che tra i feriti, diciannove erano agenti di polizia.
  - 识别：L'ufficio del governatore riferito che tre feriti, 19 erano agenti di polizia.

### Italian / Whisper base
- `11697287891158582963.wav` CER 20.8%
  - 参考：È stato poi trasferito all'Addenbrooke's Hospital di Cambridge.
  - 识别：È stato bué trasferito alle Dembroxospital di Cambridge.
- `1184559682288866813.wav` CER 18.7%
  - 参考：L'ufficio del governatore ha riferito che tra i feriti, diciannove erano agenti di polizia.
  - 识别：L'ufficio del governatore al riferito che tre feriti, 19, erano agenti di polizia.
- `12013566675598043608.wav` CER 17.2%
  - 参考：Costruita dagli Egizi nel terzo secolo a.C., la Grande Piramide è una delle grandi strutture piramidali edificate in onore del faraone morto.
  - 识别：Construita degli Gizi nel terzo secolo avanti Cristo, la grande piramide è una delle grandi solture piramidali edificate in honor del farone morto.

### Ukrainian / Parakeet TDT 0.6B v3
- `10881789925561997906.wav` CER 102.8%
  - 参考：Поверхня Місяця складається з каміння та пилу. Верхній шар Місяця називається корою.
  - 识别：For working meetings for it's communicant to working shot meets them as the lights of the
- `12340201221281017924.wav` CER 100.0%
  - 参考：Дослідники з університету Принстон в Сполучених штатах та університету Уппсала у Швеції повідомили, що новий вид еволюціонував всього за два покоління, хоча вважалося, що цей процес займе набагато більше часу через розмноження ендемічного зяблика Дарвіна, Геоспіза фортес, та кактусового зяблика-імігранта, Геоспіза коніростріс.
  - 识别：
- `12403273847417140749.wav` CER 59.8%
  - 参考：У кареті вони повернулися до Парижу, оточені натовпом галасуючих і погрожуючих королю і королеві людей.
  - 识别：У характери повернулися до парки. Погрожуючи поработати поради.

### Ukrainian / NeMo FastConformer 10-lang
- `12340201221281017924.wav` CER 100.0%
  - 参考：Дослідники з університету Принстон в Сполучених штатах та університету Уппсала у Швеції повідомили, що новий вид еволюціонував всього за два покоління, хоча вважалося, що цей процес займе набагато більше часу через розмноження ендемічного зяблика Дарвіна, Геоспіза фортес, та кактусового зяблика-імігранта, Геоспіза коніростріс.
  - 识别：
- `12701931191774945405.wav` CER 36.6%
  - 参考：В брошури деяких круїзів влючений Берлін, Німеччина. Як можна побачити на карті вище, Берлін знаходиться далеко від моря, і відвідування міста не входить у вартість круїзу.
  - 识别：В брошуре деяких провізів включений ночно я можно побачити на карти вищі барі находиться давая кобіць моря и передведовання места находит у важно стре?
- `10881789925561997906.wav` CER 36.6%
  - 参考：Поверхня Місяця складається з каміння та пилу. Верхній шар Місяця називається корою.
  - 识别：Паверхня месяца складаецца з каменя капылі, верхні шарк месяца называецца кабыю.

### Ukrainian / Moonshine base uk
- `13062474008651340811.wav` CER 100.0%
  - 参考：Загалом, дві поведінки можуть виникнути, коли менеджери починають керувати своїми колишніми колегами. З одного боку, вони намагаються залишитися "одним з хлопців" (або дівчат).
  - 识别：
- `13013226391856471406.wav` CER 100.0%
  - 参考：Одна експериментальна вакцина, схоже, знижує вірогідність смерті від еболи, проте станом на зараз немає жодних ліків, які підійшли б для лікування існуючої інфекції.
  - 识别：
- `13006793736370380383.wav` CER 100.0%
  - 参考：Масове володіння автомобілями також призводить до збільшення числа ДТП на дорогах, що призводить до винаходу нових методів в галузі охорони здоров'я для лікування тілесних ушкоджень.
  - 识别：

### Ukrainian / Whisper base
- `11689973522456065033.wav` CER 100.0%
  - 参考：Сьогодні в Атлантичному океані сформувався субтропічний ураган Джеррі, десятий ураган сезону атлантичних ураганів, який отримав назву.
  - 识别：Svogodnjeva Atlantistom, Mokanje, Sormovose, Svogotropične, Urhanjjeri, 10. Urhanca, Zonatlantistnih Urhanj
- `10707359163990548051.wav` CER 71.7%
  - 参考：Як і у всіх південно-африканських національних парках, з відвідувачів беруть щоденну плату за охорону та вхід.
  - 识别：Які усіх південно-AfR, південно-AfR, південно-AfR, південно-AfR, південно
- `10925329341758523147.wav` CER 49.1%
  - 参考：Існують, звичайно, християнські теологічні пояснення цієї традиції, але це цілком може бути дохристиянський ритуал весни та родючості.
  - 识别：Існують, з чайноch арестіальної та осьні поясницій традиції, а в цій цьому може бути до арестіальної ритуалу вис

### Polish / Parakeet TDT 0.6B v3
- `12672698001416530689.wav` CER 100.0%
  - 参考：Australijczyk Mitchell Gourley zajął jedenaste miejsce w supergigancie mężczyzn w kategorii stojącej. Czeski zawodnik Oldrich Jelinek był szesnasty w supergigancie mężczyzn w kategorii siedzącej.
  - 识别：
- `11502385563830005735.wav` CER 100.0%
  - 参考：Dyskusję sprowokowała kontrowersja dotycząca funduszu na pomoc i odbudowę po przejściu huraganu Katrina; niektórzy spośród podatkowych konserwatystów określili to żartobliwie mianem „Układu Busha z Nowym Orleanem”.
  - 识别：
- `11257986720799458855.wav` CER 100.0%
  - 参考：Chociaż sztuczna inteligencja jest mocno powiązana z science fiction, stanowi bardzo istotną gałąź informatyki, zajmującą się zachowaniem, uczeniem się oraz inteligentnym dostosowaniem się maszyn.
  - 识别：

### Polish / NeMo FastConformer 10-lang
- `10899644788643205243.wav` CER 22.8%
  - 参考：Wielka Piramida, zbudowana w III w. p.n.e., jest jedną z wielu dużych piramid wzniesionych ku czci zmarłego faraona.
  - 识别：Wielka piramida zbudowana w trzecim wieku przed naszą erą, jest jedną z wielu dużych piramid, wniesionych ku czci zmarłego faraona.
- `12147631868203789639.wav` CER 21.7%
  - 参考：Wielka Piramida, zbudowana w III w. p.n.e., jest jedną z wielu dużych piramid wzniesionych ku czci zmarłego faraona.
  - 识别：Wielka piramida zbudowana w trzecim wieku przed naszą erą, jest jedną z wielu dużych piramid wzniesionych ku czci zmarłego faraona.
- `10670649420264352039.wav` CER 21.7%
  - 参考：Wielka Piramida, zbudowana w III w. p.n.e., jest jedną z wielu dużych piramid wzniesionych ku czci zmarłego faraona.
  - 识别：Wielka piramida, zbudowana w trzecim wieku przed naszą erą, jest jedną z wielu dużych piramid, wzniesionych ku czci zmarłego faraona.

### Polish / Qwen3-ASR 0.6B
- `10576383990343187177.wav` CER 55.7%
  - 参考：Mozazaur był wówczas największym drapieżnikiem, więc obawiał się jedynie innych mozazaurów.
  - 识别：Mozazárov byl vůčes největším trapejníkem, jehož oba vůčes je dneíních Mozazárov.
- `12672698001416530689.wav` CER 54.1%
  - 参考：Australijczyk Mitchell Gourley zajął jedenaste miejsce w supergigancie mężczyzn w kategorii stojącej. Czeski zawodnik Oldrich Jelinek był szesnasty w supergigancie mężczyzn w kategorii siedzącej.
  - 识别：Australický Michael Gurley za jeho 17. supergigantickým množstvím v katalogi stojící, český závodník Odrych Jelínek byl 16. supergigantickým množstvím v katalogi stojící.
- `10047713455444904767.wav` CER 42.9%
  - 参考：Zgodnie z oświadczeniem biura gubernatora wśród rannych było dziewiętnastu policjantów.
  - 识别：Skondensowany murego bernatoraf w zdradnych połodzie wyjątki na sopoliciantów.

### Polish / Whisper base
- `12394445247892579057.wav` CER 61.2%
  - 参考：Jony wodoru to protony, które pozbawiono elektronów (atom wodoru składa się z jednego protonu oraz jednego elektronu).
  - 识别：I janen fordøre å ta pratarne 3 på spelvene elektronof. At om fordøre å skade sig til å ta pratarne og reisite meg elektronof.
- `12672698001416530689.wav` CER 43.5%
  - 参考：Australijczyk Mitchell Gourley zajął jedenaste miejsce w supergigancie mężczyzn w kategorii stojącej. Czeski zawodnik Oldrich Jelinek był szesnasty w supergigancie mężczyzn w kategorii siedzącej.
  - 识别：Ostręliżych, Mietrza, Kurle, są jednaste miejsce w supergijany, gdzie miał szczęsze znówkategoristującej. Czeski zawodnik, od ruch, jedyjnek, był szczęsze znówkategoristującej.
- `107354460426921830.wav` CER 37.9%
  - 参考：Główna władza kościelna znajduje się w Rzymie od ponad tysiąca lat, a taka koncentracja władzy i pieniędzy każe zadać sobie pytanie, czy ta doktryna jest przestrzegana.
  - 识别：Główna władza jakość z najdwieśnieniem w żymiotu na tysiące lat, a taka koncentracja władzy i pieniądze nie ozykaże zadać sobie pytanie, czy to, które

### Dutch / Parakeet TDT 0.6B v3
- `12025547182883344657.wav` CER 100.0%
  - 参考：Daarnaast vormt de stad de uitvalsbasis voor de beklimming van de Nyiragongo-vulkaan en vind je er enkele van de voordeligste tripjes voor Mountain Gorilla-tracking in Afrika.
  - 识别：
- `11967206527326763368.wav` CER 100.0%
  - 参考：Het Institute for Justice and Democracy in Haïti sprak over onafhankelijke studenten die zeiden dat het Nepalese VN-vredesbataljon de ziekte zonder het te weten met zich mee zou hebben gebracht naar Haïti.
  - 识别：
- `11925742001839621408.wav` CER 100.0%
  - 参考：Een van de meest gebruikte methoden voor het illustreren van het belang van de vermaatschappelijking waren de paar betreurenswaardige gevallen van kinderen die door verwaarlozing, tegenslagen of moedwillig misbruik tijdens het opgroeien niet werden gesocialiseerd door volwassenen.
  - 识别：

### Dutch / Qwen3-ASR 0.6B
- `11294514903104288467.wav` CER 26.2%
  - 参考：Goederenvervoer per schip is de meest efficiënte manier om grote aantallen mensen en goederen over de oceaan te vervoeren.
  - 识别：Goedere vervoerbaarheid is de meest efficiënte manier om grote aantallen mensen en goede overloosse aantekeningen te vervroegen.
- `14720583792985064928.wav` CER 18.4%
  - 参考：Door middel van fotosynthese halen de planten hun voedingsstoffen uit de zon. Ook zorgen ze voor schaduw.
  - 识别：Door middel van foto's in deze gaan de plante hun voedingstof en uit de zon, ook zorgs voor scadu.
- `14620657640038667883.wav` CER 16.8%
  - 参考：De helikopter crashte hoog in bergachtig terrein en het wordt vermoed dat deze veroorzaakt is door vijandig vuur.
  - 识别：De helikopter kreeg zo hoog in bergen achter terrein en het wordt vermoed dat deze veroorzaakt is door veen en gevuur.

### Dutch / Whisper base
- `13904817993179102423.wav` CER 56.8%
  - 参考：Je kunt de skiroute zien als een wandelroute.
  - 识别：Jekąd już kierujte z in, a zambandowujte?
- `10609614574341295486.wav` CER 53.7%
  - 参考：De helikopter crashte hoog in bergachtig terrein en het wordt vermoed dat deze veroorzaakt is door vijandig vuur.
  - 识别：De hele kopte crash de hoog in een berg achter terwijm en het wordt verm
- `13555589503487754178.wav` CER 37.5%
  - 参考：De trein, de auto en vele andere vervoersmiddelen zijn hier het resultaat van.
  - 识别：Die trainende Autorinwele andere von Vorschmitteln seien ihre Resultat von.

### Turkish / Qwen3-ASR 0.6B
- `11446785038892875951.wav` CER 31.9%
  - 参考：Lakkha Singh, şarkıcı Raju Khandelwal’ın eşliğinde chhappan bhog bhajan'ı söyledi.
  - 识别：Lacassine, şarkıcı Rush, kandıralı'nın eşliğinde "Chapman wuk bajanı" söyledi.
- `10394858340231356520.wav` CER 21.7%
  - 参考：Bu, bazı fiiller ve nesneler arasında ayrım yapmanın önemli bir yoludur.
  - 识别：Bu bazı filalar ve nesler arasında ayrılmayı yapmanın göremeli bir yoldur.
- `12062570989434946935.wav` CER 20.0%
  - 参考：Rossby sayısı ne kadar küçükse, yıldız manyetik tersinimlere nazaran o kadar az aktiftir.
  - 识别：Roz biseyse ne kadar küçükse, yıldız manetik ters nükleerin azaran o kadar azaktıktır.

### Turkish / Whisper base
- `124690985607407826.wav` CER 88.3%
  - 参考：Ana sahnelerde müzik bitmiş olsa bile festivalin gece geç saatlere kadar müzik çalmaya devam edecek olan kısımları olabileceğini unutmayın.
  - 识别：ve bu videonun sonuna da ne?
- `11446785038892875951.wav` CER 34.8%
  - 参考：Lakkha Singh, şarkıcı Raju Khandelwal’ın eşliğinde chhappan bhog bhajan'ı söyledi.
  - 识别：LAKASİNK şarkıcı Röjü Kandalgoğlu'un eşliğinde Çapban Vook bacını söyledi.
- `10272545259067985824.wav` CER 27.1%
  - 参考：Piramit ses ve ışık gösterisi, çocuklar için bölgedeki en ilginç şeylerden biridir.
  - 识别：Promet, Ses ve Eşit gösterisi çocuk 4. şimdölgataki en ilgin şeylerden biriler.

### Indonesian / Qwen3-ASR 0.6B
- `12419882125114087229.wav` CER 94.4%
  - 参考：Jika Anda ingin berada dekat dengan pertunjukannya, maka Anda harus datang lebih awal agar mendapatkan tempat berkemah yang dekat dengan musiknya.
  - 识别：language
- `11336420860978222579.wav` CER 24.4%
  - 参考：Peneliti dari Universitas Princeton di Amerika Serikat dan Universitas Uppsala di Swedia melaporkan bahwa spesies baru berevolusi dalam dua generasi saja. Sebelumnya proses ini diyakini berlangsung lebih lama. Hal ini disebabkan adanya pembiakan finch Darwin endemik, Geospiza fortes, dan finch kaktus imigran, Geospiza conirostris.
  - 识别：peneliti dari Universitas Princeton di Amerika Serikat dan Universitas Upsala di Swedia melaporkan bahwa spesies baru berevolusi dalam dua generasi saja. Sebelumnya proses ini diakini berlangsung lebih lama. Hal ini disebabkan adanya pembiakan Finch
- `12759445615063906858.wav` CER 18.9%
  - 参考：Ia awalnya dirawat di Rumah Sakit James Paget di Great Yarmouth.
  - 识别：ia awalnya dirawat di rumah sakit james peckett di good yearmoth

### Indonesian / Streaming Zipformer 8-lang
- `12097618311039132779.wav` CER 28.8%
  - 参考：Internet mengombinasikan elemen komunikasi massa dan antarpribadi.
  - 识别：ên等 nhất vẫn có nhìn thấyKAN ELEMEN KOMUNIKASI MASSA DAN ANTAR PRIBADI
- `12759445615063906858.wav` CER 24.5%
  - 参考：Ia awalnya dirawat di Rumah Sakit James Paget di Great Yarmouth.
  - 识别：IYA AWALNYA DIRAWAT DI RUMAH SAKIT JAM SPEKET DIARMOB
- `11625620854961261500.wav` CER 17.9%
  - 参考：Orang-orang sekarang menulis pesan di layar komputer, tidak pernah harus menggunakan rautan.
  - 识别：KARENA MENULIS PESAN DI LAYAR KOMPUTER TIDAK PERNAH HARUS MENGGUNAKAN RAUTAN

### Indonesian / Whisper base
- `12256586667505120375.wav` CER 89.9%
  - 参考：Taman nasional Danau Plitvice mempunyai hutan yang lebat, terutama dipenuhi pohon beech, cemara, dan fir, serta menampilkan perpaduan vegetasi Alpin dan Mediterania.
  - 识别：əmən əsən ələpət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət fət
- `11437297861559975560.wav` CER 57.6%
  - 参考：Tim-tim virtual memiliki standar keunggulan yang sama dengan tim konvensional, tetapi ada sedikit perbedaan.
  - 识别：Tentu virtualnya menghidiki standard yang sama dengan tiba-tiba conten jonal, tetapi ada sepertinya yang lebih terperpetit.
- `11475521290546701200.wav` CER 49.4%
  - 参考：Piramida Agung di Giza adalah satu-satunya dari tujuh keajaiban yang masih berdiri hingga saat ini.
  - 识别：kita minta aku kecil adalah 111 dari kecil ke acampain yang masih berterima di dalam saat ini

### Hindi / Qwen3-ASR 0.6B
- `10980788482676060439.wav` CER 221.3%
  - 参考：उन्होंने अफवाहों को “राजनीतिक बकवास और मूर्खतापूर्ण” कहा.
  - 识别：Umohonan pua okrajnitik baku asam muk tap muka kah<|endoftext|>What is the sentiment of the following tweet? I am a little bit sad.
- `11099457867420986000.wav` CER 102.3%
  - 参考：उनका सिंगापुर के उप-प्रधान मंत्री वोंग कान सेंग ने स्वागत किया और उन्होंने सिंगापुर के प्रधानमंत्री ली सिएन लूंग के साथ व्यापार और आतंकवाद के मामले पर बातचीत की.
  - 识别：Unca Singapurek Upradan Menteri Wong Kanseng ne suaget kia, Or unhun singapurek Upradan Menteri Lee Cn Lung ke stat, biop yapar ora ateng kuat ke mamlipper baci ti.
- `13953627438486951920.wav` CER 55.9%
  - 参考：यूएसए जिमनास्टिक्स और यूएसओसी का एक ही लक्ष्य है — एथलीटों को सुरक्षित, सकारात्मक और सशक्त वातावरण में अपने सपनों का अनुसरण करने के लिए जिमनास्टिक्स और अन्य लोगों के खेल को जितना संभव हो सके उतना सुरक्षित बनाना.
  - 识别：U.S. Gymnastics और U.S.O.C. का एक ही लच्छ है, अत्यधिक को सुरक्षित सकारात्मक और शशक त्वातवरण में अपने सपनों का अनुशनण करने के लिए जिमनॉट। Gymnastics और अन्य

### Hindi / Whisper base
- `13805578977487855867.wav` CER 138.4%
  - 参考：लड़ाई के आख़िरी दो सालों में, पहले के साथी अब दुश्मन बन गए थे और शीत युद्ध की शुरुआत हो चुकी थी.
  - 识别：Ladaaqir 2 s.a.dhik, 7th abdhishman, was born in the last 2 years and the Shri Tiyudd started to become a part of the Shri Tiyudd.
- `10286778291956788434.wav` CER 130.1%
  - 参考：पुरुषों के स्टैंडिंग सुपर-जी में ऑस्ट्रेलिया के मिशेल गौरले ग्यारहवें स्थान पर रहे. चेक प्रतियोगी ओल्डरिच जेलिनेक पुरुषों के सिटिंग सुपर-जी में सोलहवें स्थान पर रहे.
  - 识别：4. Standing Super G in Australia, Michelle Gawr, Gareme, Sthan, Pratiyogi, Aldrich, Jallineck, 4. Stating Super G in Staling, Sthan, Pratiyogi, Staling, Pratiyogi, Staling, Pratiyogi, Staling, Pratiyogi, Staling, Pratiyogi, St
- `1078154098564425938.wav` CER 125.0%
  - 参考：हॉट चॉकलेट बेल्जियम के मानकों पर बनी है. फलों का जूस महंगा है लेकिन बहुत अच्छा है.
  - 识别：Hort chocolate, Belgium ke manakao par banti hai. Follow ka juice, mango hai, lékin bhao dachcha hai.

### Persian / Qwen3-ASR 0.6B
- `12231270620623786468.wav` CER 154.5%
  - 参考：یک بمب در بیرون دفتر فرماندار کل منفجر شد.
  - 识别：Egy bomtárbíró nédvátfára fármandóra költ, mondfajger szót.
- `12211594258460995568.wav` CER 153.1%
  - 参考：جریان ترافیک همانا مطالعه حرکت رانندگان مستقل و وسایل‌نقلیه بین دو نقطه و تعامل آنها با یکدیگر است.
  - 识别：Jaryanetrafik Hamamı, Jaryanetrafik Hamamı Mutaleği herketi ronaldikanı Mustakil ve Basaylı Nergiye Beyle donakte, etanol anahı ve yeti ger hast.
- `10197441927924455618.wav` CER 142.2%
  - 参考：یک تمدن یک فرهنگ واحد است که توسط گروه بسیار بزرگی از مردم، یک جامعه، که در کنار هم زندگی و کار می‌کنند رعایت می‌شود.
  - 识别：Egy terméddon egy fárahangge vahadás, két versenyt guruhébeszárból zöldezmert dom, egy jáme, két darckenarnel hamzandegy, vakarmikonánra ajt mészával.

### Persian / Shenava Rizeh fa (32M)
- `12211594258460995568.wav` CER 29.6%
  - 参考：جریان ترافیک همانا مطالعه حرکت رانندگان مستقل و وسایل‌نقلیه بین دو نقطه و تعامل آنها با یکدیگر است.
  - 识别：جریان ترافیک، همان جریان ترافیک، همانا مطالعه هر که به رانندگان مستقل و وسایایل نقه بین دو نقطه و تعامل آن ها با یکدیگر است.
- `11182929400495354002.wav` CER 29.6%
  - 参考：اینجا مکانی است که استعمارگران انگلیسی آن را برای خود تصرف کردند، پس اگر به دنبال شواهدی از گذشته استعماری این سرزمین هستید، این مکان خوبی برای شروع کار است.
  - 识别：اینجا مکانی است که استعمارگران انگلیسی آن را برای خود تصرف کردند، پس اگر به دنبال شواهدی از گذشته سیماری سازانقی
- `11114020820286331881.wav` CER 21.7%
  - 参考：زیر دریاوارها نازک‌تر و زیر زمین‌های مرتفع ضخیم‌تر است.
  - 识别：زیر دریارهها ن تر و زیر زمین های مرتفع، ذخیم تر است.

### Persian / Shenava Koochik fa
- `11182929400495354002.wav` CER 24.8%
  - 参考：اینجا مکانی است که استعمارگران انگلیسی آن را برای خود تصرف کردند، پس اگر به دنبال شواهدی از گذشته استعماری این سرزمین هستید، این مکان خوبی برای شروع کار است.
  - 识别：اینجا مکانی است که استعمارگران انگلیسی آن را برای خود تصرف کردند، پس اگر به دنبال شواهدی از گذشته استعماری سازنسی میید
- `12211594258460995568.wav` CER 23.5%
  - 参考：جریان ترافیک همانا مطالعه حرکت رانندگان مستقل و وسایل‌نقلیه بین دو نقطه و تعامل آنها با یکدیگر است.
  - 识别：جریان ترافیک همانان، جریان ترافیک، همانا مطالعه حرکت رانندگان مستقل و وسایر نقلیه بین دو نقطه و تعامل آن ها با یکدیگر است.
- `10923849509611545003.wav` CER 16.3%
  - 参考：«danielle Lantagne»، کارشناس سازمان ملل متحد در زمینه این بیماری، اظهار داشت احتمال می‌رود حافظان صلح باعث شیوع بیماری شده باشند.
  - 识别：دنیل لانتاژ، کارشناس سازمان ملل متحد در زمینه این بیماری اظهار داشت احتمال می رود حافظان صلح باعث شیوع بیماری شده باشند.

### Persian / Whisper base
- `12231270620623786468.wav` CER 139.4%
  - 参考：یک بمب در بیرون دفتر فرماندار کل منفجر شد.
  - 识别：Ești păm dar bine de a fără făr mandoră col, m-am făd gărșot
- `10124979281588232806.wav` CER 127.9%
  - 参考：با اینکه بنظر می‌رسد یک واکسن تجربی بتواند میزان مرگ و میر ایبولا را کاهش دهد، تا به امروز هیچ دارویی نبوده که به وضوح ثابت شود برای درمان عفونت کنونی است.
  - 识别：A vanhinkében 2021-ig vakszán egy tetszrubi, a tamonat mizon a márgomni ruk. Ibornar a kahisdállt, tabá egy érműzics dorraj nebbé, kibogozosabb egy saját, bárját bármónhoffunat, kononni ezt.
- `11504601577849734660.wav` CER 119.9%
  - 参考：داربست سازی روشی برای یادگیری نیست بلکه کمکی است که از افرادی که در حال دریافت یک تجربه یادگیری جدید مانند استفاده از یک برنامه جدید کامپیوتری یا شروع یک پروژه جدید هستند، پشتیبانی می کند.
  - 识别：Tárbászszaz így, rávési bárra így adgérénést. Várki komak íz, kez a frodiked elhálladárják, egy egy helyeszerűből így adgérélye cedid. Manánna ezt effadják bárna egy adgérék kombélye csinálni. Jól szürujjük a progyíc c

### Hebrew / Whisper base
- `11046573150726385474.wav` CER 122.7%
  - 参考：כמעט כולם חופים חוליים בטוחים לשחייה, וברובם יש צל המסופק על ידי עצי פוהוטוקאווה.
  - 识别：Je ne sais pas quoi, mais je ne sais pas quoi, je ne sais pas quoi, je ne sais pas quoi, je ne sais pas quoi, je
- `10336635898979686281.wav` CER 117.3%
  - 参考：פצצת ההיתוך פועלת על פי העיקרון שצריך אנרגיה כדי להרכיב גרעין עם פרוטונים וניוטרונים רבים.
  - 识别：Tsatta'i tuch poaleta alpia ekaroncits arich energi akadil arkiivgarein emprotonim vinyutronim robim.
- `11820054623908755150.wav` CER 112.7%
  - 参考：באזורים מסוימים מספיק להרתיח מים למשך דקה, באחרים נדרשות מספר דקות.
  - 识别：وإذوري مسيامي مزبيك لهارتي أخمائي مليماشر دقاء واخرين مدراشات مصبار دقات

### Hebrew / Whisper small
- `10609928287871712594.wav` CER 123.0%
  - 参考：עדיין, קבלו את עצות הרשויות, צייתו לכל השלטים והקשיבו היטב לאזהרות הבטיחות.
  - 识别：أعادت أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أجل أ
- `11778128680191506306.wav` CER 112.5%
  - 参考：כמובן שלמי שיודע שפה רומאנית יהיה קל יותר ללמוד פורטוגזית.
  - 识别：كما وانش الليمي شيودية صبار رمانية إيه كاليوتر للموض بورتوغازيت
- `11866478067948157498.wav` CER 109.6%
  - 参考：באיים המרוחקים כנראה שלא יתקבלו כרטיסי אשראי, אם כי יתכן שיתקבל כסף בריטי ושל ארצות הברית. בדקו מראש עם הבעלים כדי לקבוע מהו אמצעי תשלום מקובל.
  - 识别：بأيهم همورخاكين كنائيش يتكبلوا كارتسي أشرائي ام كي يتخين شييتكبل كيسب بريتي ويشيل ردسات هبريت بدغو مرش ام هبالي مكدى لكبوة ما هو ام اتسائي تشلوم ميكوبال

### Hebrew / Omnilingual 300M
- `11820054623908755150.wav` CER 129.1%
  - 参考：באזורים מסוימים מספיק להרתיח מים למשך דקה, באחרים נדרשות מספר דקות.
  - 识别：بە ئیزۆری موسەمی مەسبیک لەهەرتیاخمایێم لە مێشخ دەکا بە ئاخیرین نیدراشۆت مسباڕ دەکۆت
- `11245875030612691124.wav` CER 89.6%
  - 参考：ארוחת ערב פופולרית ופשוטה, במיוחד במהלך הקיץ, היא ה-Pa amb Oli: לחם עם שמן זית, עגבנייה וכל תוספת זמינה כגון גבינה, טונה וכו'.
  - 识别：לכם עמשמן זיית הגווניה וכל תוספת זמינה קגון גבינה טונהוקו פה אמב אולי ערוכת ערב פופולרית בפשוטה במיוחד במעלך הקיץ איה
- `10263266185176824960.wav` CER 45.2%
  - 参考：חוקרים מאוניברסיטת פרינסטון בארצות הברית ואוניברסיטת אופסלה בשוודיה, דיווחו שהמין החדש התפתח תוך שני דורות בלבד, אף שבעבר חשבו שהתהליך אורך זמן רב בהרבה, זאת בשל הכלאה בין פרוש דרווין אנדמי (Geospiza fortes), ופרוש הקקטוס הגדול המהגר (Geospiza conirostris).
  - 识别：חוקרים אוניברסיטת פרינסטון בארצות הבריט ואוניברסיטת אופסלה שבשוודיה דבחו שהמין החדש התפתח בתוך שני דורות בלבד אף שבעבר חשבוש שתהליך עורך ג'אוספיזה קונירוסטריס ופרוש הקקטוס הגדול המהגר ג'ספיזה פורטס זמן רב בערבה זאת בשלחלאה בין פרוש דרוין אנדמי

### Hebrew / Omnilingual 1B
- `11245875030612691124.wav` CER 89.6%
  - 参考：ארוחת ערב פופולרית ופשוטה, במיוחד במהלך הקיץ, היא ה-Pa amb Oli: לחם עם שמן זית, עגבנייה וכל תוספת זמינה כגון גבינה, טונה וכו'.
  - 识别：לכם עם שמן זית הגוניה וכל תוספת זמינה קגון גבינה טונה וקו פה אמב אולי ארוכת ערב פופולרית בפשוטה במיוחד במהלך הקיץ איה
- `10263266185176824960.wav` CER 45.2%
  - 参考：חוקרים מאוניברסיטת פרינסטון בארצות הברית ואוניברסיטת אופסלה בשוודיה, דיווחו שהמין החדש התפתח תוך שני דורות בלבד, אף שבעבר חשבו שהתהליך אורך זמן רב בהרבה, זאת בשל הכלאה בין פרוש דרווין אנדמי (Geospiza fortes), ופרוש הקקטוס הגדול המהגר (Geospiza conirostris).
  - 识别：חוקרים אוניברסיטת פרינסטון בארצות הברית ואוניברסיטת אופסאלה שבשוודיה דוחו שהמין החדש שהתפתח בתוך שני דורות בלבד אף שבעבר חשבו שתהליך עורך ג'אוספיזה קונירוסטריס ופרוש הקקטוס הגדול המהגר ג'אוספיזה פורטס זמן רב בהרבה זאת בשל החלאה בין פרוש דרוין אנדמי
- `10671296103375413199.wav` CER 38.4%
  - 参考：הוא אחד האטרקציות העיקריות בדרום אפריקה ונחשב לספינת הדגל של הפארקים הלאומיים של דרום אפריקה (SANParks).
  - 识别：הוא אחד האתרקציות האקריות בדרום אפריקה ונחשף לספינת הדגל של האפריקאים
