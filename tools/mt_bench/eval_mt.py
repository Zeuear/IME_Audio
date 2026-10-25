# @brief 翻译质量基准：Bergamot（Firefox 翻译模型）与 Opus-MT(CTranslate2) 在 FLORES-200 devtest 上的
#        BLEU / chrF / 速度。Bergamot 只有 Linux 的预编译包，所以在容器里运行（见 README.md）。
#
# 译文逐句写入 results/<引擎>_<源>-<目标>.tsv（原文\t译文\t参考），指标写入 results/<引擎>.json。
# 已有结果的方向会跳过；改了评分规则只需 --rescore，不必重新翻译。
import argparse
import json
import statistics
import time
import unicodedata
from pathlib import Path

import sacrebleu

WORK = Path("/work")   # 容器里挂载的本目录
FLORES = {"en": "eng_Latn", "zh-Hans": "zho_Hans", "es": "spa_Latn", "fr": "fra_Latn", "de": "deu_Latn",
          "ja": "jpn_Jpan", "ko": "kor_Hang", "ru": "rus_Cyrl"}
# 无空格语言按字符计 BLEU（sacrebleu 的 ja/ko 分词需额外词典，统一用 char，口径一致）
TOKENIZE = {"zh-Hans": "zh", "ja": "char", "ko": "char"}


def load(lang: str, n: int) -> list[str]:
    return [l.rstrip("\n") for l in (WORK / "flores" / f"{FLORES[lang]}.devtest").open(encoding="utf-8")][:n]


def norm(text: str, tgt: str) -> str:
    # 只比内容：NFKC 统一全/半角标点，中日韩去掉空格（参考译文在数字两侧有空格）
    text = unicodedata.normalize("NFKC", text)
    return "".join(text.split()) if tgt in TOKENIZE else text


def score(hyps: list[str], refs: list[str], tgt: str) -> tuple[float, float]:
    hyps, refs = [norm(h, tgt) for h in hyps], [norm(r, tgt) for r in refs]
    bleu = sacrebleu.corpus_bleu(hyps, [refs], tokenize=TOKENIZE.get(tgt, "13a"))
    # chrF++ = 字符 n-gram + 词 n-gram；中日韩去掉空格后整句只是一个"词"，词级部分没有意义，只用字符级 chrF
    chrf = sacrebleu.corpus_chrf(hyps, [refs], word_order=0 if tgt in TOKENIZE else 2)
    return round(bleu.score, 1), round(chrf.score, 1)


def bergamot_run(src: str, tgt: str, sents: list[str], threads: int):
    from bergamot import ResponseOptions, Service, ServiceConfig, VectorString
    d = WORK / "models" / "bergamot" / f"{src}-{tgt}"
    files = {p.name: p for p in d.iterdir()}
    model = next(p for n, p in files.items() if n.startswith("model."))
    lex = next(p for n, p in files.items() if n.startswith("lex."))
    if any(n.startswith("srcvocab") for n in files):   # 中日韩目标：源、目标各一份词表
        vocabs = [files[next(n for n in files if n.startswith("srcvocab"))],
                  files[next(n for n in files if n.startswith("trgvocab"))]]
    else:
        v = next(p for n, p in files.items() if n.startswith("vocab"))
        vocabs = [v, v]
    # 参数与 Firefox 翻译一致：贪心解码（beam 1）、int8
    cfg = f"""
models: [{model}]
vocabs: [{vocabs[0]}, {vocabs[1]}]
shortlist: [{lex}, false]
beam-size: 1
normalize: 1.0
word-penalty: 0
max-length-break: 128
mini-batch-words: 1024
workspace: 128
max-length-factor: 2.0
skip-cost: true
cpu-threads: 0
quiet: true
quiet-translation: true
gemm-precision: int8shiftAlphaAll
alignment: soft
ssplit-mode: paragraph
"""
    service = Service(ServiceConfig(numWorkers=threads))
    t0 = time.perf_counter()
    m = service.modelFromConfig(cfg)
    load_s = time.perf_counter() - t0
    opts = ResponseOptions(alignment=False, qualityScores=False, HTML=False)
    out, lat = [], []
    for s in sents:   # 逐句翻译：模拟"一句话一句话地译"的使用方式，同时得到单句延迟
        t = time.perf_counter()
        r = service.translate(m, VectorString([s]), opts)
        lat.append(time.perf_counter() - t)
        out.append(r[0].target.text)
    return out, lat, load_s


def opus_run(src: str, tgt: str, sents: list[str], threads: int):
    import ctranslate2
    import sentencepiece as spm
    d = WORK / "models" / "opus" / f"{src.replace('-Hans', '')}-{tgt.replace('-Hans', '')}"
    t0 = time.perf_counter()
    tr = ctranslate2.Translator(str(d), device="cpu", intra_threads=threads, inter_threads=1, compute_type="int8")
    sp_s, sp_t = spm.SentencePieceProcessor(str(d / "source.spm")), spm.SentencePieceProcessor(str(d / "target.spm"))
    load_s = time.perf_counter() - t0
    # en→zh 需要目标语言标记（Opus-MT 模型卡要求）
    prefix = ">>cmn_Hans<< " if (src, tgt) == ("en", "zh-Hans") else ""
    out, lat = [], []
    for s in sents:
        t = time.perf_counter()
        toks = sp_s.encode(prefix + s, out_type=str) + ["</s>"]   # Marian 模型要求源句以结束符收尾，否则会一直生成到最大长度
        res = tr.translate_batch([toks], beam_size=1, max_decoding_length=256)
        out.append(sp_t.decode(res[0].hypotheses[0]))
        lat.append(time.perf_counter() - t)
    return out, lat, load_s


def rescore(engine: str) -> None:
    res_path = WORK / "results" / f"{engine}.json"
    results = json.loads(res_path.read_text(encoding="utf-8"))
    for key in results:
        src, tgt = key.split("->")
        rows = [l.split("\t") for l in
                (WORK / "results" / f"{engine}_{src}-{tgt}.tsv").read_text(encoding="utf-8").split("\n")]
        results[key]["bleu"], results[key]["chrf"] = score([r[1] for r in rows], [r[2] for r in rows], tgt)
        print(f"{key}: BLEU {results[key]['bleu']}  chrF {results[key]['chrf']}")
    res_path.write_text(json.dumps(results, ensure_ascii=False, indent=1), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", choices=["bergamot", "opus"], required=True)
    ap.add_argument("--pairs", nargs="*", default=[], help="如 en-zh-Hans zh-Hans-en ja-en")
    ap.add_argument("-n", type=int, default=1012)
    ap.add_argument("--threads", type=int, default=2)
    ap.add_argument("--rescore", action="store_true", help="不重新翻译，用已保存的译文按当前评分规则重算")
    args = ap.parse_args()
    if args.rescore:
        rescore(args.engine)
        return

    res_path = WORK / "results" / f"{args.engine}.json"
    res_path.parent.mkdir(exist_ok=True)
    results = json.loads(res_path.read_text(encoding="utf-8")) if res_path.exists() else {}
    for pair in args.pairs:
        src, tgt = ("en", pair[3:]) if pair.startswith("en-") else (pair[:-3], "en")
        key = f"{src}->{tgt}"
        if key in results:
            continue
        refs, sents = load(tgt, args.n), load(src, args.n)
        try:
            hyps, lat, load_s = (bergamot_run if args.engine == "bergamot" else opus_run)(src, tgt, sents, args.threads)
        except Exception as e:
            print(f"{key} FAILED: {e!r}", flush=True)
            continue
        bleu, chrf = score(hyps, refs, tgt)
        lat_ms = sorted(x * 1000 for x in lat)
        results[key] = {"bleu": bleu, "chrf": chrf, "n": len(hyps), "load_s": round(load_s, 2),
                        "latency_ms_median": round(statistics.median(lat_ms)),
                        "latency_ms_p95": round(lat_ms[int(len(lat_ms) * .95)])}
        (WORK / "results" / f"{args.engine}_{src}-{tgt}.tsv").write_text(
            "\n".join(f"{s}\t{h}\t{r}" for s, h, r in zip(sents, hyps, refs)), encoding="utf-8")
        res_path.write_text(json.dumps(results, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"{key}: BLEU {bleu}  chrF {chrf}  median {results[key]['latency_ms_median']} ms", flush=True)


if __name__ == "__main__":
    main()
