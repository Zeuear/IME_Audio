# @brief 翻译质量基准（Ollama 模型）：与软件实际发给 Ollama 的请求一致（见 OpenAiProvider::buildOllamaChatBody），
#        在 FLORES-200 devtest 上算 BLEU / chrF，并记录每句耗时。用来比较 Ollama 里的翻译模型（如 HY-MT 1.8B 对 Index-Translate 9B）。
#
# 用法（在 tools/mt_bench 下）：
#   ../asr_bench/.venv/Scripts/python.exe eval_ollama.py --model hf.co/tencent/HY-MT1.5-1.8B-GGUF:Q4_K_M --tag hymt18 \
#       --pairs en-zh-Hans ja-zh-Hans ko-zh-Hans es-zh-Hans zh-Hans-en --n 100
#   --cpu 只用 CPU 跑（num_gpu=0），模拟没有独显的低配电脑
# 译文逐句写入 results/ollama_<tag>_<源>-<目标>.tsv（原文\t译文\t参考），指标写入 results/ollama_<tag>.json；已有的方向会跳过。
import argparse
import json
import re
import statistics
import time
import unicodedata
from pathlib import Path

import requests
import sacrebleu

HERE = Path(__file__).parent
FLORES = {"en": "eng_Latn", "zh-Hans": "zho_Hans", "es": "spa_Latn", "fr": "fra_Latn", "de": "deu_Latn",
          "ja": "jpn_Jpan", "ko": "kor_Hang", "ru": "rus_Cyrl",
          "hi": "hin_Deva", "tl": "tgl_Latn", "id": "ind_Latn"}
TOKENIZE = {"zh-Hans": "zh", "ja": "char", "ko": "char"}
# 软件里 promptLanguageName 的叫法：翻译模型按这些说法训练
LANG_NAME = {"en": "英语", "zh-Hans": "简体中文", "ja": "日语", "ko": "韩语", "es": "西班牙语", "fr": "法语",
             "de": "德语", "ru": "俄语", "hi": "印地语", "tl": "菲律宾语", "id": "印尼语"}
import os
OLLAMA = os.environ.get("OLLAMA_URL", "http://localhost:11434")


def load(lang: str, n: int) -> list[str]:
    path = HERE / "flores" / f"{FLORES[lang]}.devtest"
    return [l.rstrip("\n") for l in path.open(encoding="utf-8")][:n]


def clean(reply: str) -> str:
    # 与 TextPolishPrompts 去思考段的逻辑一致
    reply = re.sub(r"<think>.*?</think>", "", reply, flags=re.S)
    if "</think>" in reply:
        reply = reply.rsplit("</think>", 1)[1]
    return reply.strip()


def translate(model: str, text: str, tgt: str, cpu: bool) -> tuple[str, float, str]:
    prompt = f"请将以下文本翻译为{LANG_NAME[tgt]}，直接输出翻译结果，不要进行任何解释。\n\n{text}"
    options = {"temperature": 0.0}
    if cpu:
        options["num_gpu"] = 0
    body = {"model": model, "messages": [{"role": "user", "content": prompt}], "stream": False,
            "think": False, "keep_alive": "30m", "options": options}
    t0 = time.perf_counter()
    r = requests.post(f"{OLLAMA}/api/chat", json=body, timeout=600)
    dt = time.perf_counter() - t0
    if r.status_code != 200 and "think" in r.text:   # 个别模型不支持 think 参数，与软件一样去掉重试
        body.pop("think")
        t0 = time.perf_counter()
        r = requests.post(f"{OLLAMA}/api/chat", json=body, timeout=600)
        dt = time.perf_counter() - t0
    r.raise_for_status()
    return clean(r.json()["message"]["content"]), dt, ""


def norm(text: str, tgt: str) -> str:
    text = unicodedata.normalize("NFKC", text)
    return "".join(text.split()) if tgt in TOKENIZE else text


def score(hyps: list[str], refs: list[str], tgt: str) -> tuple[float, float]:
    hyps, refs = [norm(h, tgt) for h in hyps], [norm(r, tgt) for r in refs]
    bleu = sacrebleu.corpus_bleu(hyps, [refs], tokenize=TOKENIZE.get(tgt, "13a"))
    chrf = sacrebleu.corpus_chrf(hyps, [refs], word_order=0 if tgt in TOKENIZE else 2)
    return round(bleu.score, 1), round(chrf.score, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--pairs", nargs="+", required=True)
    ap.add_argument("--n", type=int, default=100)
    ap.add_argument("--cpu", action="store_true")
    args = ap.parse_args()

    out_dir = HERE / "results"
    out_dir.mkdir(exist_ok=True)
    metrics_path = out_dir / f"ollama_{args.tag}.json"
    metrics = json.loads(metrics_path.read_text(encoding="utf-8")) if metrics_path.exists() else {}

    for pair in args.pairs:
        # 语言码里有 "-"（zh-Hans），按已知语言切分
        src = next(s for s in FLORES if pair.startswith(s + "-") and pair[len(s) + 1:] in FLORES)
        tgt = pair[len(src) + 1:]
        tsv = out_dir / f"ollama_{args.tag}_{pair}.tsv"
        if pair in metrics and tsv.exists():
            print(f"skip {pair} (done)")
            continue
        srcs, refs = load(src, args.n), load(tgt, args.n)
        hyps, times = [], []
        for i, s in enumerate(srcs):
            h, dt, _ = translate(args.model, s, tgt, args.cpu)
            hyps.append(h)
            times.append(dt)
            if i == 0:   # 第一句含模型加载，不计入速度
                times.pop()
            if (i + 1) % 20 == 0:
                print(f"  {pair} {i + 1}/{len(srcs)}", flush=True)
        bleu, chrf = score(hyps, refs, tgt)
        empty = sum(1 for h in hyps if not h.strip())
        metrics[pair] = {"n": len(srcs), "bleu": bleu, "chrf": chrf, "empty": empty,
                         "sec_per_sentence": round(statistics.mean(times), 2),
                         "sec_p95": round(sorted(times)[int(len(times) * 0.95) - 1], 2)}
        tsv.write_text("\n".join(f"{s}\t{h}\t{r}".replace("\r", " ") for s, h, r in zip(srcs, hyps, refs)),
                       encoding="utf-8")
        metrics_path.write_text(json.dumps(metrics, ensure_ascii=False, indent=1), encoding="utf-8")
        print(pair, metrics[pair], flush=True)


if __name__ == "__main__":
    main()
