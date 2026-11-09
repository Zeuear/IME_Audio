# 导出 Qwen3-ASR 的 sherpa-onnx fp32 版本

官方 sherpa 版 Qwen3-ASR 只有 int8。ONNX Runtime 的 CUDA 后端没有 int8 算子内核，
int8 模型开 CUDA 基本还是在 CPU 上算，GPU 没有动静。这个脚本把官方权重导出成 fp32 包，供 CUDA 使用。

## 环境

只在导出时需要，不进软件。用 uv 建独立环境：

```bash
uv venv --python 3.12 .venv
uv pip install --python .venv/Scripts/python.exe torch --index-url https://download.pytorch.org/whl/cpu
uv pip install --python .venv/Scripts/python.exe "transformers==4.57.6" onnx onnxruntime numpy huggingface_hub sherpa-onnx
```

`transformers` 必须是 4.57.x。5.x 会在加载权重时报 `KeyError: 'factor'`。

## 导出

```bash
.venv/Scripts/python.exe export_qwen3_asr.py --size 0.6B --outdir ./out --fp16 --pack
.venv/Scripts/python.exe export_qwen3_asr.py --size 1.7B --outdir ./out --fp16 --pack
```

- 在 CPU 上导出，不需要显卡。
- `--fp16` 在 fp32 包之外再生成 fp16 包（输入输出仍是 fp32，sherpa 无需改动）。**GPU 推荐用 fp16**：体积减半，8 GB 显存也放得下 1.7B。
- 体积：0.6B fp32 约 3.6 GB、fp16 约 1.9 GB（单文件）；1.7B fp32 约 8.8 GB、fp16 约 4.7 GB。导出前要保证磁盘空间够，中间文件（`work/`）另占约 30 GB。
- 脚本会下载官方权重、拉取并固定上游导出脚本、导出、校验（和 PyTorch 对比）、整理目录、用 sherpa-onnx 加载试跑。

## 产物

```
sherpa-onnx-qwen3-asr-0.6B-fp32/
  conv_frontend.onnx
  encoder.onnx   encoder.onnx.data
  decoder.onnx   decoder.onnx.data     # 权重在 .data 里，必须和 .onnx 同目录
  tokenizer/ (merges.txt, vocab.json, tokenizer_config.json)
```

## 已知限制

- `--max-total-len` 默认 512：提示词加音频 token 加生成的 token 总长不能超过它。
  软件按 VAD 切成 30 秒以内的小段，够用；整段喂入很长的音频时只会输出 `language`（官方 int8 包同样如此）。
- 外部数据文件超过 2 GB 的 protobuf 限制，所以 fp32 不能合成单个文件。
