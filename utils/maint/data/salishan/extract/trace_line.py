"""Print the paper_sift.py lines executed while the main loop holds a source line with a given string.

usage: python trace_line.py <string> <stem> <authors> <language>
"""
import runpy
import sys

wanted = sys.argv[1]
sys.argv = ["paper_sift.py"] + sys.argv[2:]
hits = []


def tracer(frame, event, arg):
    if frame.f_code.co_filename.endswith("paper_sift.py"):
        text = frame.f_locals.get("text")
        if isinstance(text, str) and wanted in text and event == "line":
            hits.append(frame.f_lineno)
    return tracer


sys.settrace(tracer)
try:
    runpy.run_path("paper_sift.py", run_name="__main__")
finally:
    sys.settrace(None)
    collapsed = []
    for one in hits:
        if not collapsed or collapsed[-1] != one:
            collapsed.append(one)
    sys.stderr.write("LINES %s\n" % collapsed)
