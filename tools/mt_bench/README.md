# mt_bench：本地翻译模型质量基准

评估"软件里集成本地翻译"值不值得做：用 FLORES-200 devtest（1012 句，专业人工翻译、各语言一一对应），
比较 **Bergamot**（Firefox 翻译模型，MPL-2.0）和 **Opus-MT**（CTranslate2，Apache-2.0）的质量与速度。

## 运行

Bergamot 只有 Linux 的预编译包，所以翻译在 Docker 容器里跑；COMET 打分用 GPU。

```bash
cd tools/mt_bench
../asr_bench/.venv/Scripts/python.exe -I fetch_data.py          # 下载数据和模型，约 1.5 GB
export MT_BENCH_DATA=/d/mt_bench                                  # 见下方"Docker 挂载"
./run_docker.sh cpu eval_mt.py --engine bergamot --pairs en-zh-Hans zh-Hans-en ...
./run_docker.sh cpu eval_mt.py --engine opus --pairs en-zh-Hans zh-Hans-en ja-en ko-en es-en de-en
./run_docker.sh gpu score_comet.py 64                            # 约 12 分钟（RTX 5060）
```

- **Docker 挂载**：Docker Desktop 只能挂载它能访问的盘。若仓库所在的盘它看不到，容器里会是一个空目录
  （报 `can't open file '/work/...'`）。把数据复制到别的盘，用 `MT_BENCH_DATA` 指过去。
- **GPU**：RTX 50 系（sm_120）要用 CUDA 12.8 以上的 PyTorch，`run_docker.sh gpu` 用的镜像已满足。
- `eval_mt.py --rescore` 不重新翻译，只按当前评分规则重算。

## 评分口径

- BLEU：中文用 `zh` 分词，日、韩按字符；欧洲语言用 `13a`。chrF：中日韩只用字符级（去空格后整句只是一个"词"，词级部分无意义）。
- 评分前统一做 NFKC（全/半角标点）并去掉中日韩的空格，**只比内容**。Bergamot 的中文输出是半角标点，软件里需要后处理。
- **COMET**（`Unbabel/wmt22-comet-da`）不是百分制，大致 0.5~0.95，只在同一语言方向内比较。
- BLEU 不同语言之间不能直接比。

## 结果（2026-10，FLORES-200 devtest 全部 1012 句，2 线程 CPU，贪心解码）

见 [comet_table.md](comet_table.md)。摘要：

| 方向 | Bergamot BLEU / COMET | Opus-MT BLEU / COMET |
|---|---|---|
| 英→中 | 43.1 / 0.862 | 26.0 / 0.734 |
| 中→英 | 27.5 / 0.859 | 22.8 / 0.837 |
| 日→英 | 25.2 / 0.862 | 18.0 / 0.815 |
| 韩→英 | 27.5 / 0.859 | 18.9 / 0.814 |
| 西→英 | 26.8 / 0.855 | 27.6 / 0.858 |
| 德→英 | 40.8 / 0.879 | 40.5 / 0.881 |

- 中日韩 Bergamot 明显更好，欧洲语言与 Opus-MT 持平；两种指标排序一致。
- Bergamot 单句约 25 ms，每个方向 35~55 MB；Opus-MT 约 100 ms、约 150 MB。
- 抽样读译文：短句、日常句通顺；**长而复杂的句子会语义混乱，人名、专名容易错**。

## 局限

- FLORES 是书面新闻，没有测听写的口语。
- Bergamot 在 Windows 上没有预编译包，集成要自己编译 bergamot-translator，**这是最大的未知**。
- 许可：Bergamot 引擎 MPL-2.0，Firefox 翻译模型 MPL-2.0（Mozilla 官方自述），均可与 AGPL-3.0 并存；
  NLLB-200 是 CC-BY-NC，不能用。训练语料的许可 Mozilla 没有声明。
