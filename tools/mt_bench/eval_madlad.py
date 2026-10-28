# @brief MADLAD-400 3B（google/madlad400-3b-mt，Apache-2.0）在 FLORES-200 devtest 上的翻译质量，
#        与 eval_mt.py 的 Bergamot 结果用同一份数据、同一套评分口径比较。
#
# 直接在 Windows 主机上跑（需要 GPU，PyTorch 用 CUDA），不走 Docker。
# 译文写入 results/madlad_<源>-<目标>.tsv（原文\t译文\t参考），指标写入 results/madlad.json。
# 贪心解码、bf16，批量翻译；另外抽样测单句延迟。
#
# 用法（在 tools/mt_bench 下，用 asr_bench 的 venv）：
#   ../asr_bench/.venv/Scripts/python.exe eval_madlad.py --pairs en-zh-Hans zh-Hans-en en-ja ja-en en-ko ko-en
import argparse
import json
import statistics
import time
from pathlib import Path

import torch
from transformers import AutoModelForSeq2SeqLM, AutoTokenizer

import eval_mt

HERE = Path(__file__).resolve().parent
eval_mt.WORK = HERE   # eval_mt 默认是容器里的 /work，这里改成本目录，复用它的数据加载与评分
MODEL_DIR = HERE / "models" / "madlad400-3b-mt"

# MADLAD 的目标语言标记用的是 ISO 639 代码，不是 FLORES 代码
MADLAD_CODE = {"en": "en", "zh-Hans": "zh", "ja": "ja", "ko": "ko", "es": "es", "fr": "fr", "de": "de", "ru": "ru"}


def madlad_run(src: str, tgt: str, sents: list[str], batch: int):
    tok = AutoTokenizer.from_pretrained(str(MODEL_DIR))
    model = AutoModelForSeq2SeqLM.from_pretrained(str(MODEL_DIR), torch_dtype=torch.bfloat16).to("cuda").eval()
    prefix = f"<2{MADLAD_CODE[tgt]}> "   # MADLAD 用前缀标记目标语言，源语言自动识别

    def translate(batch_sents: list[str]) -> list[str]:
        enc = tok([prefix + s for s in batch_sents], return_tensors="pt", padding=True, truncation=True,
                  max_length=256).to("cuda")
        with torch.inference_mode():
            out = model.generate(**enc, num_beams=1, do_sample=False, max_new_tokens=256)
        return tok.batch_decode(out, skip_special_tokens=True)

    # 按长度排序分批，减少 padding；结果再按原顺序还原
    order = sorted(range(len(sents)), key=lambda i: len(sents[i]))
    hyps = [""] * len(sents)
    t0 = time.perf_counter()
    for start in range(0, len(order), batch):
        idx = order[start:start + batch]
        for i, h in zip(idx, translate([sents[i] for i in idx])):
            hyps[i] = h
    total = time.perf_counter() - t0

    # 单句延迟：取前 30 句逐句翻译，模拟听写时一句一句译
    lat = []
    for s in sents[:30]:
        t = time.perf_counter()
        translate([s])
        lat.append(time.perf_counter() - t)

    del model
    torch.cuda.empty_cache()
    return hyps, lat, total


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", nargs="+", required=True, help="如 en-zh-Hans zh-Hans-en ja-en")
    ap.add_argument("-n", type=int, default=1012)
    ap.add_argument("--batch", type=int, default=16)
    args = ap.parse_args()

    assert torch.cuda.is_available(), "PyTorch 看不到 GPU"
    print("GPU:", torch.cuda.get_device_name(0), "| torch", torch.__version__, flush=True)

    res_dir = HERE / "results"
    res_dir.mkdir(exist_ok=True)
    res_path = res_dir / "madlad.json"
    results = json.loads(res_path.read_text(encoding="utf-8")) if res_path.exists() else {}

    for pair in args.pairs:
        src, tgt = ("en", pair[3:]) if pair.startswith("en-") else (pair[:-3], "en")
        key = f"{src}->{tgt}"
        if key in results:
            continue
        refs, sents = eval_mt.load(tgt, args.n), eval_mt.load(src, args.n)
        hyps, lat, total = madlad_run(src, tgt, sents, args.batch)
        bleu, chrf = eval_mt.score(hyps, refs, tgt)
        lat_ms = sorted(x * 1000 for x in lat)
        results[key] = {"bleu": bleu, "chrf": chrf, "n": len(hyps), "batch_total_s": round(total, 1),
                        "latency_ms_median": round(statistics.median(lat_ms)),
                        "latency_ms_p95": round(lat_ms[int(len(lat_ms) * .95)])}
        (res_dir / f"madlad_{src}-{tgt}.tsv").write_text(
            "\n".join(f"{s}\t{h}\t{r}" for s, h, r in zip(sents, hyps, refs)), encoding="utf-8")
        res_path.write_text(json.dumps(results, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"{key}: BLEU {bleu}  chrF {chrf}  batch {total:.0f}s  single median {results[key]['latency_ms_median']} ms",
              flush=True)


if __name__ == "__main__":
    main()
