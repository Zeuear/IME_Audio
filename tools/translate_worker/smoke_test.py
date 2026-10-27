"""CI 冒烟测试：下载一个真实模型，让 worker 真正翻译一句话。

只检查启动时的 READY 不够：缺了 BLAS 的 worker 能正常启动和加载模型，
要到第一次矩阵乘法才 abort（Windows 上表现为 0xC0000409）。

用法：python smoke_test.py <worker 可执行文件> <translation_models.json> <工作目录>
config.yml 的内容必须与 TranslationModelStore::generateBergamotYaml 保持一致。
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import urllib.request

DIRECTION = "zh-Hans>en"
SOURCE_TEXT = "你好世界"


def download(url, dest, sha256, size):
    if dest.exists() and dest.stat().st_size == size and _sha256(dest) == sha256:
        return
    with urllib.request.urlopen(url, timeout=120) as resp, open(dest, "wb") as out:
        while chunk := resp.read(1 << 20):
            out.write(chunk)
    actual = _sha256(dest)
    if actual != sha256:
        raise SystemExit(f"SHA256 mismatch for {dest.name}: expected {sha256}, got {actual}")


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def write_config(model_dir, files):
    path_of = {f["type"]: (model_dir / f["name"]).resolve().as_posix() for f in files}
    shared = path_of.get("vocab")
    src_vocab = shared or path_of["srcvocab"]
    trg_vocab = shared or path_of["trgvocab"]
    config = model_dir / "config.yml"
    config.write_text(
        f"models: [{path_of['model']}]\n"
        f"vocabs: [{src_vocab}, {trg_vocab}]\n"
        f"shortlist: [{path_of['lex']}, false]\n"
        "beam-size: 1\nnormalize: 1.0\nword-penalty: 0\nmax-length-break: 128\n"
        "mini-batch-words: 1024\nworkspace: 128\nmax-length-factor: 2.0\nskip-cost: true\n"
        "cpu-threads: 0\nquiet: true\nquiet-translation: true\n"
        "gemm-precision: int8shiftAlphaAll\nalignment: soft\nssplit-mode: paragraph\n",
        encoding="utf-8")
    return config


def main():
    # Windows 控制台默认 cp1252，打印中文会抛 UnicodeEncodeError，把测试误判为失败
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    worker, manifest_path, work_dir = sys.argv[1:4]
    manifest = json.loads(pathlib.Path(manifest_path).read_text(encoding="utf-8"))
    direction = manifest["directions"][DIRECTION]
    model_dir = pathlib.Path(work_dir) / DIRECTION.replace(">", "-")
    model_dir.mkdir(parents=True, exist_ok=True)
    for f in direction["files"]:
        download(manifest["baseUrl"] + f["location"], model_dir / f["name"], f["sha256"], f["size"])
    config = write_config(model_dir, direction["files"])

    src, trg = DIRECTION.split(">")
    # 按行协议发送：加载 → 翻译 → 退出；用字节写入，避免任何编码或 BOM 转换
    request = f"L\t{src}\t{trg}\t{config.as_posix()}\nT\t1\t{src}\t{trg}\t{SOURCE_TEXT}\nQ\n".encode("utf-8")
    proc = subprocess.run([worker], input=request, capture_output=True, timeout=300)
    out = proc.stdout.decode("utf-8", "replace")
    err = proc.stderr.decode("utf-8", "replace")
    print(f"exit code: {proc.returncode} (0x{proc.returncode & 0xFFFFFFFF:08X})")
    print("--- stdout ---\n" + out + "--- stderr ---\n" + err[-4000:])

    result = next((line.split("\t") for line in out.splitlines() if line.startswith("R\t1\t")), None)
    if proc.returncode != 0 or result is None or len(result) < 4 or not result[3].strip():
        raise SystemExit("worker did not return a translation")
    print(f"translated {SOURCE_TEXT!r} -> {result[3]!r}")


if __name__ == "__main__":
    main()
