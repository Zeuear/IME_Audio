# @brief 生成 resources/data/pinyin_table.txt：汉字 → 无声调拼音（每字取最常用读音）。
#
# 用途：热词按读音模糊匹配（HotwordSelector）。数据来自 pypinyin（MIT，内含 mozillazg/pinyin-data，
# 同样 MIT，源自 Unicode Unihan）。每行 "拼音 汉字串"，按拼音归并，比 "字,拼音" 逐行存小一半以上。
# 多音字：第一读音写在普通行，其余读音写在以 "+" 开头的行（汉字串同样），人名里的生僻读音（隗 wěi、缪 miào 等）靠它。
# 用法：uv run --with pypinyin python tools/gen_pinyin_table.py
import sys
from collections import defaultdict
from pathlib import Path

from pypinyin import Style, pinyin
from pypinyin.pinyin_dict import pinyin_dict

by_syllable = defaultdict(list)
alternates = defaultdict(list)
for cp in sorted(pinyin_dict):
    ch = chr(cp)
    # 只收常用区：基本区和扩展 A，人名生僻字（郗、仉、隗…）都在里面
    if not (0x3400 <= cp <= 0x4DBF or 0x4E00 <= cp <= 0x9FFF):
        continue
    readings = []
    for r in pinyin(ch, style=Style.NORMAL, heteronym=True)[0]:
        if r and r != ch and r.isascii() and r not in readings:
            readings.append(r)
    if not readings:
        continue
    by_syllable[readings[0]].append(ch)
    for extra in readings[1:]:
        alternates[extra].append(ch)

out = Path(__file__).resolve().parents[1] / "resources" / "data" / "pinyin_table.txt"
out.parent.mkdir(parents=True, exist_ok=True)
with out.open("w", encoding="utf-8", newline="\n") as f:
    for syl in sorted(by_syllable):
        f.write(f"{syl} {''.join(by_syllable[syl])}\n")
    for syl in sorted(alternates):
        f.write(f"+{syl} {''.join(alternates[syl])}\n")
print(out, sum(len(v) for v in by_syllable.values()), "chars,", sum(len(v) for v in alternates.values()), "alt readings,", len(by_syllable), "syllables,", out.stat().st_size, "bytes")
