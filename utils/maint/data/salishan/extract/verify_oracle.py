"""Check an oracle table against its text layer: every form looked up verbatim, every marked word looked up in the forms."""
import re
import sys
import unicodedata


def flat(text):
    return re.sub(r"\s+", " ", unicodedata.normalize("NFC", text))


table_path, text_path = sys.argv[1], sys.argv[2]
rows = open(table_path, encoding="utf-8").read().split("\n")
header = rows[0].split("\t")
body = [row for row in rows[1:] if row]
bad = [(number + 2, len(row.split("\t"))) for number, row in enumerate(body) if len(row.split("\t")) not in (4, 5)]
print("rows", len(body), "columns", header, "bad rows", bad)
layer = flat(open(text_path, encoding="utf-8").read())
forms = [row.split("\t")[3] for row in body]
kinds = {}
for row in body:
    kind = row.split("\t")[2]
    kinds[kind] = kinds.get(kind, 0) + 1
print("kinds", kinds)
missing = [(row.split("\t")[0], row.split("\t")[2], row.split("\t")[3][:70]) for row in body
           if flat(row.split("\t")[3]) not in layer]
print("forms not found verbatim:", len(missing), "of", len(forms))
for one in missing:
    print("   ", one)
labels = ("notation", "symbol note")
counted = [row.split("\t") for row in body if row.split("\t")[2] not in labels]
bare_layer = re.sub(r"\s+", "", layer)
stuck = [(cells[0], cells[2], cells[3][:70]) for cells in counted
         if re.sub(r"\s+", "", flat(cells[3])) not in bare_layer]
print("forms outside notation and symbol note:", len(counted),
      "not found verbatim:", sum(1 for cells in counted if flat(cells[3]) not in layer),
      "not found with all white space removed:", len(stuck))
for one in stuck:
    print("   ", one)
joined = "\n".join(flat(form) for form in forms)
marked = [word for word in layer.split(" ") if any(ord(ch) > 127 for ch in word)]
edge = "()[]{}‘’“”,.;:!?"
inner = [word for word in marked if any(ord(ch) > 127 for ch in word.strip(edge))]
print("words marked after edge punctuation is stripped", len(inner), "distinct", len(set(inner)),
      "not in forms:", len(set(word for word in inner if word.strip(edge) not in joined)))
absent = sorted(set(word for word in marked if word not in joined and word.strip(edge) not in joined))
print("marked words", len(marked), "distinct", len(set(marked)), "not in forms:", len(absent))
for word in absent:
    print("   ", word, " ".join("U+%04X" % ord(ch) for ch in word if ord(ch) > 127))
