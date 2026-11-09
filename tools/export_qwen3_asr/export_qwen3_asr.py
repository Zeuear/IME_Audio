#!/usr/bin/env python3
"""把官方 Qwen3-ASR 导出成 sherpa-onnx 可直接加载的 fp32 模型包（用于 CUDA）。

官方发布的 sherpa 版本只有 int8，ONNX Runtime 的 CUDA 后端没有 int8 算子内核，
int8 模型开 CUDA 也基本跑在 CPU 上。fp32 包才能真正用上 GPU。

导出脚本沿用 sherpa-onnx 官方包 README 里指明的 Wasser1462/Qwen3-ASR-onnx，固定到验证过的提交。

用法：
    python export_qwen3_asr.py --size 0.6B --outdir ./out
    python export_qwen3_asr.py --size 1.7B --outdir ./out --pack
依赖见 README.md。
"""
import argparse
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

UPSTREAM = "https://github.com/Wasser1462/Qwen3-ASR-onnx"
UPSTREAM_COMMIT = "0829a57"   # 2026-04-08，已用 transformers==4.57.6 验证
TOKENIZER_FILES = ("merges.txt", "vocab.json", "tokenizer_config.json")


def run(cmd, cwd=None):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], cwd=cwd, check=True)


def fetch_upstream(workdir: Path) -> Path:
    src = workdir / "upstream"
    if not src.exists():
        run(["git", "clone", "-q", UPSTREAM, src])
    run(["git", "checkout", "-q", UPSTREAM_COMMIT], cwd=src)
    return src


def fetch_weights(size: str, workdir: Path) -> Path:
    from huggingface_hub import snapshot_download
    return Path(snapshot_download(f"Qwen/Qwen3-ASR-{size}", local_dir=workdir / f"Qwen3-ASR-{size}"))


def export(src: Path, weights: Path, raw: Path, max_total_len: int):
    raw.mkdir(parents=True, exist_ok=True)
    run([sys.executable, "export_qwen3_asr_onnx.py", "--model", weights, "--outdir", raw,
         "--device", "cpu", "--max-total-len", max_total_len, "--no-int8", "--verify"], cwd=src)


def assemble(raw: Path, weights: Path, pkg: Path, size: str):
    # 导出目录里还有中间权重文件（onnx__MatMul_* 等），只取 sherpa 需要的
    if pkg.exists():
        shutil.rmtree(pkg)
    (pkg / "tokenizer").mkdir(parents=True)
    for name in ("conv_frontend.onnx", "encoder.onnx", "encoder.onnx.data", "decoder.onnx", "decoder.onnx.data"):
        shutil.copy2(raw / name, pkg / name)
    for name in TOKENIZER_FILES:
        shutil.copy2(weights / name, pkg / "tokenizer" / name)
    (pkg / "README.md").write_text(
        f"# sherpa-onnx Qwen3-ASR {size} (fp32)\n\n"
        f"由官方 Qwen/Qwen3-ASR-{size} 导出，供 sherpa-onnx 在 CUDA 上使用。\n"
        f"导出脚本：{UPSTREAM} @ {UPSTREAM_COMMIT}\n"
        "decoder.onnx / encoder.onnx 的权重在同名 .data 文件里，两个文件必须放在同一目录。\n",
        encoding="utf-8")


def to_fp16(pkg32: Path, pkg16: Path):
    """fp32 包转 fp16。输入输出仍是 fp32，sherpa 不需要任何改动；体积减半，GPU 上更省显存。"""
    import onnx
    from onnxruntime.transformers.float16 import convert_float_to_float16
    if pkg16.exists():
        shutil.rmtree(pkg16)
    pkg16.mkdir(parents=True)
    shutil.copytree(pkg32 / "tokenizer", pkg16 / "tokenizer")
    shutil.copy2(pkg32 / "conv_frontend.onnx", pkg16 / "conv_frontend.onnx")
    for name in ("encoder", "decoder"):
        model = onnx.load(str(pkg32 / f"{name}.onnx"), load_external_data=True)
        # decoder 的 fp32 版超过 2 GB，形状推断要把整个模型序列化，会失败，所以只对它关闭
        model16 = convert_float_to_float16(model, keep_io_types=True, disable_shape_infer=(name == "decoder"))
        # ByteSize() 同样受 2 GB 限制，按权重张量累加估算
        weight_bytes = sum(len(t.raw_data) for t in model16.graph.initializer)
        external = weight_bytes > 1_800_000_000
        onnx.save(model16, str(pkg16 / f"{name}.onnx"), save_as_external_data=external,
                  all_tensors_to_one_file=True, location=f"{name}.onnx.data" if external else None)
    (pkg16 / "README.md").write_text(
        (pkg32 / "README.md").read_text(encoding="utf-8").replace("(fp32)", "(fp16)"), encoding="utf-8")


def smoke_test(pkg: Path):
    import wave
    import numpy as np
    import sherpa_onnx
    r = sherpa_onnx.OfflineRecognizer.from_qwen3_asr(
        conv_frontend=str(pkg / "conv_frontend.onnx"), encoder=str(pkg / "encoder.onnx"),
        decoder=str(pkg / "decoder.onnx"), tokenizer=str(pkg / "tokenizer"), num_threads=4)
    sr = 16000
    s = r.create_stream()
    s.accept_waveform(sr, np.zeros(sr, dtype=np.float32))
    r.decode_stream(s)
    print("smoke test: model loads and decodes, text =", repr(s.result.text))


def pack(pkg: Path, outdir: Path) -> Path:
    # 权重文件本身压缩率很低，用 gz 比 bz2 快得多
    archive = outdir / f"{pkg.name}.tar.gz"
    with tarfile.open(archive, "w:gz", compresslevel=1) as t:
        t.add(pkg, arcname=pkg.name)
    return archive


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--size", choices=["0.6B", "1.7B"], default="0.6B")
    p.add_argument("--outdir", type=Path, required=True)
    p.add_argument("--max-total-len", type=int, default=512,
                   help="解码器 KV 缓存长度；越大越能处理长音频，也越占显存")
    p.add_argument("--fp16", action="store_true", help="再生成 fp16 版本（GPU 推荐；0.6B 为单文件）")
    p.add_argument("--pack", action="store_true", help="导出后打成 tar.gz")
    p.add_argument("--skip-smoke-test", action="store_true")
    args = p.parse_args()

    work = args.outdir / "work"
    work.mkdir(parents=True, exist_ok=True)
    src = fetch_upstream(work)
    weights = fetch_weights(args.size, work)
    raw = work / f"raw_{args.size}"
    export(src, weights, raw, args.max_total_len)

    pkg = args.outdir / f"sherpa-onnx-qwen3-asr-{args.size}-fp32"
    assemble(raw, weights, pkg, args.size)
    if not args.skip_smoke_test:
        smoke_test(pkg)
    packages = [pkg]
    if args.fp16:
        pkg16 = args.outdir / f"sherpa-onnx-qwen3-asr-{args.size}-fp16"
        to_fp16(pkg, pkg16)
        if not args.skip_smoke_test:
            smoke_test(pkg16)
        packages.append(pkg16)
    if args.pack:
        for p_ in packages:
            print("archive:", pack(p_, args.outdir))
    for p_ in packages:
        print("done:", p_)


if __name__ == "__main__":
    main()
