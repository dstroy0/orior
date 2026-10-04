import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.stdout.reconfigure(encoding="utf-8")
import gen  # noqa: E402

paper = gen.Paper("ICSNL56_Zenk_final")
start = paper.find(r"^sinaa_694_side 1$")
TIME = re.compile(r"^\d:\d\d:\d\d\.\d$")
sets, expect, pre = [], 1, []
for number in range(start + 1, paper.last + 1):
    text = paper.text(number)
    if not text or paper.lines[number][2] or re.fullmatch(r"\d{3}", text):
        continue
    head = re.match(r"^(\d{1,3}) (.*)$", text)
    if head and int(head.group(1)) == expect:
        sets.append([expect, number, []])
        expect += 1
        continue
    (sets[-1][2] if sets else pre).append((number, text))
print("sets", len(sets), "last", sets[-1][0])
for label, number, body in sets:
    plain = [one for one in body if not one[1].startswith("(") and not TIME.match(one[1])]
    if len(plain) % 2 or any(one[1].startswith("(") and not one[1].rstrip().endswith(")") for one in body):
        print("--", label, paper.text(number))
        for one in body:
            print("   ", one[0], one[1][:150])
