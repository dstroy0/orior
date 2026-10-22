# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The browser's own trace of the window's frames, for what the page's own trace cannot see: the style, layout, paint,
raster and compositing each frame takes after its script, and the collector's pauses. A test starts it, and ends it
handing it to a query run, each thread's events summed by name a line each, the most time first:
`<thread> <name> <count> <total ms> <most ms>`."""

import collections
import subprocess
import threading
import time

# the categories traced: the timeline of each frame, its frames, and the collector
CATEGORIES = "devtools.timeline,disabled-by-default-devtools.timeline.frame,disabled-by-default-devtools.timeline.stack,v8.gc,cc,viz"
# the lines of the summary handed: the threads' busiest events, and the style passes and layouts scripts forced
LINES = 40
FORCED_LINES = 10


class Frames:
    """What the browser's trace has given so far, and whether it has ended."""

    def __init__(self):
        self.on = False
        self.events = []
        self.ended = False
        self.lock = threading.Lock()

    def heard(self, method, params):
        """An event of the page's connection the trace takes: its events as they come, and its end."""
        with self.lock:
            if not self.on:
                return
            if method == "Tracing.dataCollected":
                self.events.extend(params.get("value") or [])
            elif method == "Tracing.tracingComplete":
                self.ended = True

    def start(self, conn):
        with self.lock:
            self.on, self.events, self.ended = True, [], False
        conn.call("Tracing.start", timeout=10, categories=CATEGORIES, transferMode="ReportEvents")

    def end(self, conn, writer, qry, name):
        """The trace ended and its summary handed to the run at `qry` as the file `name` by `writer`."""
        conn.call("Tracing.end", timeout=10)
        until = time.monotonic() + 60
        while True:
            with self.lock:
                if self.ended:
                    self.on = False
                    events, self.events = self.events, []
                    break
            if time.monotonic() > until:
                raise RuntimeError("the browser's trace did not end in 60 s")
            time.sleep(0.05)
        text = summary(events)
        handing = subprocess.run([writer, "--filed", qry, name], input=text.encode(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if handing.returncode != 0:
            raise RuntimeError("%s --filed %s %s ended %d" % (writer, qry, name, handing.returncode))


def summary(events):
    """Each thread's complete events summed by name, the most time first, then the style passes and layouts a script
    forced, summed by the call that forced them."""
    threads = {}
    for event in events:
        if event.get("ph") == "M" and event.get("name") == "thread_name":
            threads[(event.get("pid", 0), event.get("tid", 0))] = str(event.get("args", {}).get("name", "")).replace(" ", "_")
    sums = collections.OrderedDict()
    forced = collections.OrderedDict()
    for event in events:
        if event.get("ph") != "X":
            continue
        thread = threads.get((event.get("pid", 0), event.get("tid", 0)), "thread")
        name = str(event.get("name", "")).replace(" ", "_")
        ms = float(event.get("dur") or 0) / 1000.0
        args = event.get("args") or {}
        touched = args.get("elementCount") or (args.get("beginData") or {}).get("dirtyObjects") or 0
        count, total, most, touched_all = sums.get((thread, name), (0, 0.0, 0.0, 0))
        sums[(thread, name)] = (count + 1, total + ms, max(most, ms), touched_all + int(touched))
        if name in ("UpdateLayoutTree", "Layout"):
            stack = (args.get("beginData") or {}).get("stackTrace") or []
            if stack:
                top = stack[0]
                at = "%s %s %s:%d" % (name, top.get("functionName") or "(anonymous)", str(top.get("url", "")).rsplit("/", 1)[-1], int(top.get("lineNumber") or 0) + 1)
                count_forced, total_forced = forced.get(at, (0, 0.0))
                forced[at] = (count_forced + 1, total_forced + ms)
    lines = []
    for (thread, name), (count, total, most, touched) in sorted(sums.items(), key=lambda one: -one[1][1])[:LINES]:
        lines.append("%s %s %d %.1f %.1f touched %d" % (thread, name, count, total, most, touched) if touched else "%s %s %d %.1f %.1f" % (thread, name, count, total, most))
    for at, (count, total) in sorted(forced.items(), key=lambda one: -one[1][1])[:FORCED_LINES]:
        lines.append("forced %s %d %.1f" % (at, count, total))
    return "".join(line + "\n" for line in lines)
