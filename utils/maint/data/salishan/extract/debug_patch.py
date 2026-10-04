"""Write paper_sift_debug.py: paper_sift.py tracing the lines that open on the labels given."""
import io
import sys

LABELS = tuple(sys.argv[1:])
source = io.open("paper_sift.py", encoding="utf-8").read()
source = source.replace(
    "        verdict = second[number]\n        if interrupted is not None",
    "        verdict = second[number]\n        if text.lstrip().startswith(%r): print('SEEN', repr(text[:50]), verdict, example)\n"
    "        if interrupted is not None" % (LABELS,), 1)
source = source.replace(
    "        if opened:\n            flush()",
    "        if text.lstrip().startswith(%r): print('OPENED?', bool(opened))\n"
    "        if opened:\n            flush()" % (LABELS,), 1)
io.open("paper_sift_debug.py", "w", encoding="utf-8").write(source)
