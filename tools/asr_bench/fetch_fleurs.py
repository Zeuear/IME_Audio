# @brief 从 HuggingFace 下载 FLEURS 测试集的前 N 条音频及参考文本。
#
# FLEURS 每种语言的 test.tar.gz 有几百 MB，这里流式解压、取够 N 条就断开，
# 避免整包下载。输出：data/fleurs/<code>/{*.wav, refs.tsv}
import argparse
import csv
import io
import sys
import tarfile
import urllib.request
from pathlib import Path

from langs import LANGS

BASE = "https://huggingface.co/datasets/google/fleurs/resolve/main/data"
ROOT = Path(__file__).parent / "data" / "fleurs"


def fetch(code: str, n: int) -> None:
    out = ROOT / code
    refs_path = out / "refs.tsv"
    if refs_path.exists() and sum(1 for _ in refs_path.open(encoding="utf-8")) >= n:
        print(f"[{code}] already fetched, skip")
        return
    out.mkdir(parents=True, exist_ok=True)

    # test.tsv 列：id, file_name, raw_transcription, transcription, chars, num_samples, gender
    # 用 raw_transcription 作参考，归一化在打分时统一做，两边口径一致
    with urllib.request.urlopen(f"{BASE}/{code}/test.tsv") as r:
        text = r.read().decode("utf-8")
    refs = {}
    for row in csv.reader(io.StringIO(text), delimiter="\t", quoting=csv.QUOTE_NONE):
        if len(row) >= 3:
            refs[row[1]] = row[2]

    got = []
    with urllib.request.urlopen(f"{BASE}/{code}/audio/test.tar.gz") as r:
        with tarfile.open(fileobj=r, mode="r|gz") as tar:
            for m in tar:
                name = Path(m.name).name
                if not m.isfile() or name not in refs:
                    continue
                (out / name).write_bytes(tar.extractfile(m).read())
                got.append(name)
                if len(got) >= n:
                    break

    with refs_path.open("w", encoding="utf-8", newline="") as f:
        for name in got:
            f.write(f"{name}\t{refs[name]}\n")
    print(f"[{code}] {len(got)} utterances")


def main() -> None:
    ap = argparse.ArgumentParser()
    # 多取一些：含数字的句子打分时会被排除
    ap.add_argument("-n", type=int, default=120, help="utterances per language")
    ap.add_argument("--only", nargs="*", help="FLEURS codes to fetch (default: all)")
    args = ap.parse_args()
    codes = args.only or sorted({l.fleurs for l in LANGS.values() if l.fleurs and l.models})
    for code in codes:
        try:
            fetch(code, args.n)
        except Exception as e:  # 单个语言失败不影响其它
            print(f"[{code}] FAILED: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
