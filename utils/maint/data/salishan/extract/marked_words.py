import sys
import unicodedata

path = sys.argv[1]
text = unicodedata.normalize("NFC", open(path, encoding="utf-8").read())
seen = {}
for word in text.split():
    if any(ord(ch) > 127 for ch in word):
        seen[word] = seen.get(word, 0) + 1
for word, count in seen.items():
    names = " ".join("U+%04X" % ord(ch) for ch in word if ord(ch) > 127)
    print("%d\t%s\t%s" % (count, word, names))
print("distinct", len(seen), "total", sum(seen.values()))
