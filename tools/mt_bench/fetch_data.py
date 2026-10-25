# @brief 下载翻译基准需要的全部文件：FLORES-200 devtest、Bergamot（Firefox 翻译）模型、Opus-MT(CTranslate2) 对照模型。
#
# 只用标准库，用 asr_bench 的 venv 运行：  python -I fetch_data.py
# 产物都在本目录下（flores/、models/），已被 .gitignore 排除。已存在的文件会跳过。
import io
import json
import tarfile
import urllib.request
from pathlib import Path

HERE = Path(__file__).parent

FLORES_URL = "https://dl.fbaipublicfiles.com/nllb/flores200_dataset.tar.gz"
FLORES_LANGS = ["eng_Latn", "zho_Hans", "spa_Latn", "fra_Latn", "deu_Latn", "jpn_Jpan", "kor_Hang", "rus_Cyrl"]

REGISTRY = "https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/translations-models/records"
CDN = "https://firefox-settings-attachments.cdn.mozilla.net/"
BERGAMOT_PAIRS = [("en", "zh-Hans"), ("zh-Hans", "en"), ("en", "es"), ("es", "en"), ("en", "fr"), ("fr", "en"),
                  ("en", "de"), ("de", "en"), ("en", "ja"), ("ja", "en"), ("en", "ko"), ("ko", "en"),
                  ("en", "ru"), ("ru", "en")]

# Opus-MT 只有部分方向有 CTranslate2 转换版（gaudi/opus-mt-<pair>-ctranslate2）
OPUS_PAIRS = ["en-zh", "zh-en", "ja-en", "ko-en", "es-en", "de-en"]


def flores() -> None:
    out = HERE / "flores"
    if out.exists() and len(list(out.glob("*.devtest"))) == len(FLORES_LANGS):
        return
    out.mkdir(exist_ok=True)
    data = urllib.request.urlopen(FLORES_URL).read()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for m in tar:
            name = Path(m.name).name
            if m.name.endswith(".devtest") and name.split(".")[0] in FLORES_LANGS:
                (out / name).write_bytes(tar.extractfile(m).read())
    print("FLORES:", len(list(out.glob("*.devtest"))), "languages")


def bergamot() -> None:
    recs = json.load(urllib.request.urlopen(REGISTRY))["data"]
    for src, tgt in BERGAMOT_PAIRS:
        cand = [r for r in recs if r["fromLang"] == src and r["toLang"] == tgt]
        ver = max({r["version"] for r in cand if r["fileType"] == "model"})   # 取最新版本
        d = HERE / "models" / "bergamot" / f"{src}-{tgt}"
        d.mkdir(parents=True, exist_ok=True)
        total = 0
        for r in (r for r in cand if r["version"] == ver):
            dest = d / r["name"]
            if not dest.exists():
                dest.write_bytes(urllib.request.urlopen(CDN + r["attachment"]["location"]).read())
            total += dest.stat().st_size
        print(f"Bergamot {src}->{tgt} v{ver}: {total / 1e6:.1f} MB", flush=True)


def opus() -> None:
    for pair in OPUS_PAIRS:
        d = HERE / "models" / "opus" / pair
        d.mkdir(parents=True, exist_ok=True)
        for f in ("model.bin", "shared_vocabulary.json", "source.spm", "target.spm", "config.json"):
            dest = d / f
            if dest.exists() and dest.stat().st_size > 0:
                continue
            with urllib.request.urlopen(f"https://huggingface.co/gaudi/opus-mt-{pair}-ctranslate2/resolve/main/{f}") as r, \
                    dest.open("wb") as o:
                while chunk := r.read(1 << 20):
                    o.write(chunk)
        print(f"Opus-MT {pair}: {(d / 'model.bin').stat().st_size / 1e6:.0f} MB", flush=True)


if __name__ == "__main__":
    flores()
    bergamot()
    opus()
