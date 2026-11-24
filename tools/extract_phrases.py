#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
提取每一段开头的词组，生成 CSV。

用法：
    python3 extract_phrases.py [输入文件] [输出文件]
    默认输入 input.txt，默认输出 output.csv

规则：
- 每一段 = 以空行分隔的段落；如果全文没有空行，则每行算一段
- 取每一段开头到第一个空格之间的文字
- 输出格式（Tab 分隔，和示例一致）：
      1               你好              all
      2               早上好            all
  即：序号、空列、词组、空列、all
"""

import csv
import sys


def split_paragraphs(text):
    """按空行分段；没有空行时按行分段。"""
    paragraphs = [p.strip() for p in text.split("\n\n") if p.strip()]
    if len(paragraphs) <= 1:
        paragraphs = [line.strip() for line in text.splitlines() if line.strip()]
    return paragraphs


def first_word(paragraph):
    """取段落开头到第一个空格之间的文字（全角空格也算空格）。"""
    return paragraph.split(" ", 1)[0].strip()


def main():
    input_path = sys.argv[1] if len(sys.argv) > 1 else "input.txt"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "output.csv"

    with open(input_path, "r", encoding="utf-8-sig") as f:
        text = f.read()

    words = [first_word(p) for p in split_paragraphs(text)]

    with open(output_path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.writer(f, delimiter="\t")
        for i, word in enumerate(words, 1):
            writer.writerow([i, "", word, "", "all"])

    print(f"共提取 {len(words)} 个词组，已写入 {output_path}")


if __name__ == "__main__":
    main()
