# @brief 对已安装的 Sherpa 模型跑 FLEURS 子集，计算 CER/WER/RTF 并生成排行榜。
#
# 识别结果逐条写入 results/raw/<语言>__<模型>.jsonl；已存在则跳过，
# 所以改了归一化规则只需重新打分（--score-only），不必重跑模型。
import argparse
import gc
import json
import re
import shutil
import tarfile
import time
import unicodedata
import urllib.request
from pathlib import Path

import numpy as np
import soundfile as sf
from opencc import OpenCC

from langs import LANGS, MODELS, NOT_IN_APP, create_recognizer

HERE = Path(__file__).parent
DATA = HERE / "data" / "fleurs"
RAW = HERE / "results" / "raw"
# 软件已安装的模型直接用、不删除；其余下载到 CACHE，测完即删，避免占满硬盘
INSTALLED_ROOTS = [HERE.parents[1] / "app" / c / "sherpa" / "models" for c in ("RelWithDebInfo", "Release")]
CACHE = HERE / "model_cache"

_t2s = OpenCC("t2s")


def normalize(text: str, lang: str) -> str:
    # 比较前两边统一口径：大小写、全半角、繁简、标点都不计入错误
    text = unicodedata.normalize("NFKC", text).lower()
    if lang in ("Chinese", "Cantonese"):
        text = _t2s.convert(text)
    # 去掉标点(P*)和符号(S*)；保留字母/数字/组合附标(M*)，泰文等依赖附标
    text = "".join(" " if unicodedata.category(c)[0] in "PS" else c for c in text)
    return re.sub(r"\s+", " ", text).strip()


def edit_distance(ref: list, hyp: list) -> int:
    prev = list(range(len(hyp) + 1))
    for i, r in enumerate(ref, 1):
        cur = [i] + [0] * len(hyp)
        for j, h in enumerate(hyp, 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (r != h))
        prev = cur
    return prev[-1]


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_name(dest.name + ".part")
    # 连接中途断开时 urlopen 不一定报错，会留下截断文件（表现为 Protobuf parsing failed），
    # 所以按 Content-Length 校验，不符就重下
    for attempt in range(3):
        with urllib.request.urlopen(url) as r, tmp.open("wb") as f:
            expected = int(r.headers.get("Content-Length") or -1)
            shutil.copyfileobj(r, f, 1 << 20)
        if expected < 0 or tmp.stat().st_size == expected:
            tmp.replace(dest)
            return
        print(f"  truncated download ({tmp.stat().st_size}/{expected}), retry {attempt + 1}")
    raise IOError(f"download incomplete after retries: {url}")


def locate_model(key: str) -> tuple[Path, bool]:
    """返回 (models_root, 是否为临时下载)。"""
    m = MODELS[key]
    for root in INSTALLED_ROOTS:
        if all((root / m.dir / rel).exists() for rel in m.files.values()):
            return root, False
    target = CACHE / m.dir
    if m.archive:
        tarball = CACHE / Path(m.archive).name
        print(f"[{key}] downloading {m.archive}")
        _download(m.archive, tarball)
        with tarfile.open(tarball) as tar:
            tar.extractall(CACHE)
        tarball.unlink()
    else:
        for rel in m.files.values():
            if not (target / rel).exists():
                print(f"[{key}] downloading {rel}")
                _download(f"https://huggingface.co/{m.repo}/resolve/main/{rel}", target / rel)
    return CACHE, True


def _decode(rec, online: bool, sr: int, samples: np.ndarray) -> str:
    s = rec.create_stream()
    s.accept_waveform(sr, samples)
    if not online:
        rec.decode_stream(s)
        return s.result.text
    # 流式模型：尾部补静音把最后一帧推出来，再标记输入结束
    s.accept_waveform(sr, np.zeros(int(0.66 * sr), dtype=np.float32))
    s.input_finished()
    while rec.is_ready(s):
        rec.decode_stream(s)
    return rec.get_result(s)


def run_pair(lang: str, key: str, rec, online: bool, load_s: float) -> Path:
    out = RAW / f"{lang}__{key}.jsonl"
    code = LANGS[lang].fleurs
    rows = [l.rstrip("\n").split("\t", 1) for l in (DATA / code / "refs.tsv").open(encoding="utf-8")]
    print(f"[{lang} / {key}] {len(rows)} utterances")

    RAW.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(".part")
    with tmp.open("w", encoding="utf-8") as f:
        f.write(json.dumps({"load_s": load_s}) + "\n")
        for name, ref in rows:
            samples, sr = sf.read(DATA / code / name, dtype="float32")
            if samples.ndim > 1:
                samples = samples.mean(axis=1)
            t = time.perf_counter()
            hyp = _decode(rec, online, sr, np.ascontiguousarray(samples))
            dt = time.perf_counter() - t
            f.write(json.dumps({"file": name, "ref": ref, "hyp": hyp,
                                "dur": len(samples) / sr, "dt": dt}, ensure_ascii=False) + "\n")
    tmp.replace(out)  # 跑完才改名，中断不会留下半截结果被当成完成
    return out


def score(path: Path, lang: str) -> dict:
    lines = [json.loads(l) for l in path.open(encoding="utf-8")]
    load_s, utts = lines[0]["load_s"], lines[1:]
    c_err = c_n = w_err = w_n = empty = with_digits = 0
    worst = []
    for u in utts:
        # 参考文本写阿拉伯数字，模型常输出读法（2011 → 二零一一 / two thousand eleven），
        # 读法不唯一无法可靠互转，含数字的句子整句排除，所有模型用同一子集比较
        if any(unicodedata.category(c) == "Nd" for c in u["ref"]):
            with_digits += 1
            continue
        r, h = normalize(u["ref"], lang), normalize(u["hyp"], lang)
        if not r:
            continue
        rc, hc = list(r.replace(" ", "")), list(h.replace(" ", ""))
        rw, hw = r.split(), h.split()
        ce, we = edit_distance(rc, hc), edit_distance(rw, hw)
        c_err, c_n, w_err, w_n = c_err + ce, c_n + len(rc), w_err + we, w_n + len(rw)
        empty += not h
        worst.append((ce / len(rc), u["file"], u["ref"], u["hyp"]))
    worst.sort(reverse=True)
    return {
        "cer": c_err / c_n, "wer": w_err / w_n, "n": len(worst), "empty": empty,
        "with_digits": with_digits, "audio_s": sum(u["dur"] for u in utts),
        # 取逐句 RTF 中位数：机器休眠/抢占会让个别句子耗时暴涨，总和会被一条拖垮；
        # 有 --rtf-only 的复测值时以复测为准（原测量期间有编译等干扰）
        "rtf": lines[0].get("rtf_remeasured", float(np.median([u["dt"] / u["dur"] for u in utts]))),
        "load_s": load_s, "worst": worst[:3],
    }


def remeasure_rtf(lang: str, key: str, rec, online: bool, n: int) -> None:
    """只重测速度：解码前 n 句取 RTF 中位数，写回结果文件头，不动识别结果。"""
    path = RAW / f"{lang}__{key}.jsonl"
    lines = path.read_text(encoding="utf-8").splitlines()
    head = json.loads(lines[0])
    code = LANGS[lang].fleurs
    ratios = []
    for line in lines[1:n + 1]:
        u = json.loads(line)
        samples, sr = sf.read(DATA / code / u["file"], dtype="float32")
        if samples.ndim > 1:
            samples = samples.mean(axis=1)
        t = time.perf_counter()
        _decode(rec, online, sr, np.ascontiguousarray(samples))
        ratios.append((time.perf_counter() - t) / u["dur"])
    head["rtf_remeasured"] = float(np.median(ratios))
    lines[0] = json.dumps(head)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[{lang} / {key}] RTF remeasured {head['rtf_remeasured']:.3f}")


def write_report(results: list[tuple[str, str, dict]], failures: list) -> Path:
    pct = lambda x: f"{x * 100:.1f}%"
    md = ["# Sherpa 本地模型准确率基准（FLEURS test 子集）", "",
          "- 分数 = 100 × (1 − 主指标错误率)，下限 0；主指标：中/日/韩/泰/粤用 CER，其余用 WER",
          "- 归一化：NFKC、小写、繁→简（中文）、去标点符号",
          "- 参考文本含阿拉伯数字的句子不计分（读法不唯一，如 2011 / 二零一一），「排除」列为其条数",
          "- RTF = 识别耗时 / 音频时长，取逐句中位数（CPU，线程数同软件设置），越小越快",
          "- 参数与 ModelRegistry::GetConfig 一致；Whisper 不指定语言（自动识别），与软件行为相同",
          ""]
    md += ["| 语言 | 模型 | 分数 | CER | WER | RTF | 加载 | 空结果 | 计分条数 | 排除 |",
           "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    by_lang: dict[str, list] = {}
    for lang, key, s in results:
        by_lang.setdefault(lang, []).append((key, s))
    for lang in LANGS:
        rows = by_lang.get(lang, [])
        metric = LANGS[lang].metric
        rows.sort(key=lambda r: r[1][metric])
        for key, s in rows:
            m = MODELS[key]
            name = m.display + ("" if m.in_app else " †")
            sc = max(0.0, 100 * (1 - s[metric]))
            md.append(f"| {lang} | {name} | **{sc:.1f}** | {pct(s['cer'])} | {pct(s['wer'])} | "
                      f"{s['rtf']:.3f} | {s['load_s']:.1f}s | {s['empty']} | {s['n']} | {s['with_digits']} |")
    md += ["", "† 软件中尚未提供（新语种候选，或如 FireRed 在 SherpaConfig.cpp 中被注释），仅作参考。", ""]
    if failures:
        md += ["## 运行失败", ""] + [f"- `{k}`：{w}" for k, w in failures] + [""]

    skipped = [(l, v.note) for l, v in LANGS.items() if v.note]
    if skipped:
        md += ["## 未测试", ""] + [f"- **{l}**：{n}" for l, n in skipped] + [""]

    md += ["## 各组合错误最多的样例", ""]
    for lang, key, s in results:
        md.append(f"### {lang} / {MODELS[key].display}")
        for err, f, ref, hyp in s["worst"]:
            md += [f"- `{f}` CER {pct(err)}", f"  - 参考：{ref}", f"  - 识别：{hyp}"]
        md.append("")
    out = HERE / "results" / "leaderboard.md"
    out.write_text("\n".join(md), encoding="utf-8")
    return out


EXPORT = HERE.parents[1] / "resources" / "model_benchmarks.json"


def export_json(results: list[tuple[str, str, dict]]) -> Path:
    # 供软件“本地识别”页的模型列表显示；只导出软件里可选的模型，键与 SherpaConfig.cpp 一致
    langs: dict[str, dict] = {}
    for lang, key, s in results:
        m = MODELS[key]
        if not m.in_app or not LANGS[lang].in_app or (lang, key) in NOT_IN_APP:
            continue
        metric = LANGS[lang].metric
        langs.setdefault(lang, {})[m.repo] = {
            "score": round(max(0.0, 100 * (1 - s[metric])), 1),
            "rtf": round(s["rtf"], 3),
            "metric": metric,
            "cer": round(s["cer"], 4),
            "wer": round(s["wer"], 4),
            "n": s["n"],
        }
    import sherpa_onnx
    doc = {
        "dataset": "FLEURS test",
        "utterances": 120,
        "sherpa_onnx": sherpa_onnx.__version__,
        "generated": time.strftime("%Y-%m-%d"),
        # 分数 = 100 × (1 − 主指标)；rtf 为逐句中位数，CPU、线程数同软件默认
        "languages": langs,
    }
    EXPORT.write_text(json.dumps(doc, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    return EXPORT


def run_model(key: str, pairs: list[str], failures: list, rtf_only: int = 0) -> None:
    if rtf_only:
        todo = [l for l in pairs if (RAW / f"{l}__{key}.jsonl").exists()]
    else:
        todo = [l for l in pairs if not (RAW / f"{l}__{key}.jsonl").exists()]
    if not todo:
        return
    try:
        root, temporary = locate_model(key)
    except Exception as e:
        failures.append((key, f"download failed: {e}"))
        print(f"[{key}] download FAILED: {e}")
        shutil.rmtree(CACHE / MODELS[key].dir, ignore_errors=True)
        return
    try:
        t0 = time.perf_counter()
        rec, online = create_recognizer(key, root)
        load_s = time.perf_counter() - t0
        print(f"[{key}] loaded in {load_s:.1f}s")
        for lang in todo:
            if rtf_only:
                remeasure_rtf(lang, key, rec, online, rtf_only)
            else:
                run_pair(lang, key, rec, online, load_s)
    except Exception as e:
        failures.append((key, f"load/decode failed: {e}"))
        print(f"[{key}] FAILED: {e}")
    finally:
        rec = None
        gc.collect()  # Windows 下识别器释放前模型文件被占用，删不掉
        if temporary:
            shutil.rmtree(CACHE / MODELS[key].dir, ignore_errors=True)
            if (CACHE / MODELS[key].dir).exists():
                print(f"[{key}] WARNING: cache not fully removed")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--langs", nargs="*", help="app language names (default: all)")
    ap.add_argument("--models", nargs="*", help="model keys (default: all)")
    ap.add_argument("--score-only", action="store_true")
    ap.add_argument("--rtf-only", type=int, default=0, metavar="N",
                    help="只用前 N 句重测速度（需配合 --pairs）")
    ap.add_argument("--pairs", nargs="*", help="Language__model 对，限定运行范围")
    args = ap.parse_args()

    langs = [l for l in (args.langs or LANGS) if LANGS[l].fleurs]
    # 按模型分组：一个模型下载一次、把它负责的语种全跑完再删除
    by_model: dict[str, list[str]] = {}
    for lang in langs:
        for key in LANGS[lang].models:
            if args.models and key not in args.models:
                continue
            if args.pairs and f"{lang}__{key}" not in args.pairs:
                continue
            by_model.setdefault(key, []).append(lang)

    failures: list = []
    if not args.score_only:
        for key, pairs in by_model.items():
            run_model(key, pairs, failures, args.rtf_only)

    results = []
    for lang in langs:
        for key in LANGS[lang].models:
            path = RAW / f"{lang}__{key}.jsonl"
            if key in by_model and path.exists():
                s = score(path, lang)
                results.append((lang, key, s))
                print(f"[{lang} / {key}] CER {s['cer']:.3f}  WER {s['wer']:.3f}  RTF {s['rtf']:.3f}")
    for key, why in failures:
        print(f"FAILED {key}: {why}")
    print("report:", write_report(results, failures))
    # 只有全量打分时才覆盖导出文件，避免 --langs 子集运行把其它语种的数据冲掉
    if not args.langs and not args.models and not args.pairs:
        print("export:", export_json(results))


if __name__ == "__main__":
    main()
