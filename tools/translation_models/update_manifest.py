"""生成 resources/translation_models.json：本地翻译模型的随程序发布清单。

数据来自 Mozilla Remote Settings（Firefox Translations 的模型索引）。运行时不联网查索引，
清单固化在仓库里；Mozilla 发布新模型后重新运行本脚本并人工检查差异。

    python tools/translation_models/update_manifest.py                 # 联网获取
    python tools/translation_models/update_manifest.py --input rec.json  # 用已下载的索引

只用标准库。
"""
import argparse
import json
import re
import sys
import urllib.request
from datetime import date
from pathlib import Path

INDEX_URL = ("https://firefox.settings.services.mozilla.com/v1/buckets/main/"
             "collections/translations-models/records")
BASE_URL = "https://firefox-settings-attachments.cdn.mozilla.net/"
FILE_TYPES = {"model", "lex", "vocab", "srcvocab", "trgvocab"}
OUTPUT = Path(__file__).resolve().parents[2] / "resources" / "translation_models.json"


def version_key(version):
    return [int(p) if p.isdigit() else 0 for p in version.split(".")]


def load_records(path):
    if path:
        raw = Path(path).read_bytes()
    else:
        with urllib.request.urlopen(INDEX_URL, timeout=60) as resp:
            raw = resp.read()
    return json.loads(raw)["data"]


def build(records):
    best = {}
    for r in records:
        if r.get("fileType") not in FILE_TYPES:
            continue
        att = r.get("attachment") or {}
        # 约三分之二的记录没有顶层 hash，以 attachment.hash 为准
        sha = att.get("hash") or r.get("hash")
        if not (r.get("fromLang") and r.get("toLang") and r.get("version")
                and r.get("name") and sha and att.get("location") and "size" in att):
            continue
        if not re.fullmatch(r"[0-9a-f]{64}", sha):
            continue
        key = f'{r["fromLang"]}>{r["toLang"]}'
        entry = best.setdefault(key, {"version": r["version"], "files": []})
        if version_key(r["version"]) > version_key(entry["version"]):
            entry["version"], entry["files"] = r["version"], []
        if r["version"] == entry["version"]:
            entry["files"].append({"type": r["fileType"], "name": r["name"], "sha256": sha,
                                   "size": att["size"], "location": att["location"]})
    for entry in best.values():
        entry["files"].sort(key=lambda f: (f["type"], f["name"]))
    return dict(sorted(best.items()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", help="已下载的索引 JSON；不给则联网获取")
    args = ap.parse_args()

    directions = build(load_records(args.input))
    if not directions:
        sys.exit("没有解析出任何方向，不写文件")

    doc = {"generated": date.today().isoformat(), "baseUrl": BASE_URL, "directions": directions}
    OUTPUT.write_text(json.dumps(doc, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(directions)} 个方向 -> {OUTPUT}")


if __name__ == "__main__":
    main()
