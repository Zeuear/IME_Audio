# @brief 用 COMET (Unbabel/wmt22-comet-da) 给 eval_mt.py 保存的译文打分，不重新翻译。
#
# 需要 GPU：RTX 50 系（sm_120）要用 CUDA 12.8 以上的 PyTorch，见 README.md。
# 输入 results/<引擎>_<源>-<目标>.tsv（原文\t译文\t参考），输出 results/comet.json。
# COMET 分数不是百分制，大致在 0.5~0.95，只在同一语言方向内比较大小。
import json
import os
import sys
import time
from pathlib import Path

# MT_BENCH_WORK 未设置时是容器里的挂载目录 /work；在 Windows 主机上直接跑时设成本目录
WORK = Path(os.environ.get("MT_BENCH_WORK", "/work"))
os.environ.setdefault("HF_HOME", str(WORK / "hf_cache"))   # 评分模型约 2.3 GB，缓存在挂载目录，重复运行不用再下载
import torch
from comet import download_model, load_from_checkpoint

RES = WORK / "results"
BATCH = int(sys.argv[1]) if len(sys.argv) > 1 else 64

assert torch.cuda.is_available(), "容器里 PyTorch 看不到 GPU：检查 --gpus all 和 PyTorch 的 CUDA 版本"
print("GPU:", torch.cuda.get_device_name(0), "| torch", torch.__version__, flush=True)

model = load_from_checkpoint(download_model("Unbabel/wmt22-comet-da"))
out_path = RES / "comet.json"
scores = json.loads(out_path.read_text(encoding="utf-8")) if out_path.exists() else {}

for tsv in sorted(RES.glob("*_*-*.tsv")):
    engine, pair = tsv.stem.split("_", 1)            # bergamot_en-zh-Hans → bergamot, en-zh-Hans
    src, tgt = ("en", pair[3:]) if pair.startswith("en-") else (pair[:-3], "en")
    key = f"{engine}:{src}->{tgt}"
    if key in scores:
        continue
    rows = [l.split("\t") for l in tsv.read_text(encoding="utf-8").split("\n") if l]
    data = [{"src": r[0], "mt": r[1], "ref": r[2]} for r in rows]
    t0 = time.perf_counter()
    result = model.predict(data, batch_size=BATCH, gpus=1, progress_bar=False)
    scores[key] = {"comet": round(float(result.system_score), 4), "n": len(data),
                   "seconds": round(time.perf_counter() - t0, 1)}
    out_path.write_text(json.dumps(scores, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{key}: COMET {scores[key]['comet']}  ({scores[key]['seconds']}s)", flush=True)
