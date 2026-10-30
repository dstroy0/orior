# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The report of a motion recording, written into its folder: report.md with the findings ranked, the most first,
each with a picture under pictures/; every motion found; and the long animation frames with their scripts.

A finding on one motion is pictured as its progress over its own time from 0 to 1, a dot each frame, beside CSS's
`ease` in gray, the frames the finding names marked. A pair the reading paired with each other is pictured on one scale:
a track's way in and way out, or a step's repeats, each over its own time from 0 to 1; two parts' motions in one step
over the step's time in milliseconds."""

import os

from PIL import Image, ImageDraw, ImageFont

import motion_read

WIDTH = 560
HEIGHT = 280
MARGIN = 40
# The most findings pictured.
PICTURED = 60
GROUND = (18, 20, 36)
GRID = (52, 56, 84)
TEXT = (200, 204, 224)
EASE = (120, 124, 150)
MARKED = (236, 92, 92)
COLORS = [(96, 180, 255), (120, 220, 150), (240, 190, 90), (200, 140, 255)]
PAIRED = ("in and out differ", "repeat differs")


def plane(title):
    """A picture's ground, its frame and its title; and the point of a place on it, x and y from 0 to 1, y from -0.1
    to 1.15 to let an overshoot and a reversal show."""
    picture = Image.new("RGB", (WIDTH, HEIGHT), GROUND)
    draw = ImageDraw.Draw(picture)
    font = ImageFont.load_default()
    left, top, right, bottom = MARGIN, MARGIN, WIDTH - MARGIN // 2, HEIGHT - MARGIN // 2

    def point(x, y):
        return left + (right - left) * x, bottom - (bottom - top) * (y + 0.1) / 1.25

    for value in (0.0, 0.5, 1.0):
        draw.line([point(0, value), point(1, value)], fill=GRID)
        draw.text((4, point(0, value)[1] - 6), "%.1f" % value, fill=TEXT, font=font)
    draw.rectangle([left, top, right, bottom], outline=GRID)
    draw.text((left, 8), title[:88], fill=TEXT, font=font)
    return picture, draw, font, point


def curve(draw, point, places, color, dots=True):
    points = [point(x, y) for x, y in places]
    if len(points) > 1:
        draw.line(points, fill=color, width=2)
    if dots:
        for x, y in points:
            draw.ellipse([x - 2, y - 2, x + 2, y + 2], fill=color)


def own_places(motion):
    """A motion's progress over its own time from 0 to 1."""
    length = motion.duration or 1.0
    return [((at - motion.start) / length, progress) for at, progress in zip(motion.times, motion.progress)]


def pictured(finding, path):
    """The picture of one finding, written to `path`."""
    picture, draw, font, point = plane("%s: %s, %s" % (finding.kind, finding.part, finding.step))
    if finding.kind == "starts mid-way":
        first = min(one.start for one in finding.motions)
        length = (max(one.end for one in finding.motions) - first) or 1.0
        for index, motion in enumerate(finding.motions):
            places = [((at - first) / length, progress) for at, progress in zip(motion.times, motion.progress)]
            curve(draw, point, places, COLORS[index % len(COLORS)])
            draw.text((MARGIN + 4, MARGIN + 4 + 12 * index), "%s, from %.0f ms" % (motion.key, motion.start - first), fill=COLORS[index % len(COLORS)], font=font)
        draw.text((WIDTH - 150, HEIGHT - 14), "0 to %.0f ms" % length, fill=TEXT, font=font)
    elif finding.kind in PAIRED:
        for index, motion in enumerate(finding.motions):
            curve(draw, point, own_places(motion), COLORS[index % len(COLORS)])
            draw.text((MARGIN + 4, MARGIN + 4 + 12 * index), "%s %s, %.0f ms" % (motion.key, motion.step, motion.duration), fill=COLORS[index % len(COLORS)], font=font)
    else:
        motion = finding.motions[0]
        curve(draw, point, [(index / 64.0, motion_read.ease(index / 64.0)) for index in range(65)], EASE, dots=False)
        curve(draw, point, own_places(motion), COLORS[0])
        length = motion.duration or 1.0
        for at in finding.frames:
            if at in motion.times:
                x, y = point((at - motion.start) / length, motion.progress[motion.times.index(at)])
                draw.ellipse([x - 5, y - 5, x + 5, y + 5], outline=MARKED, width=2)
        draw.text((MARGIN + 4, MARGIN + 4), "%s, %.0f ms, %d frames; ease in grey" % (motion.key, motion.duration, len(motion.times) - 1), fill=COLORS[0], font=font)
    picture.save(path)


def cell(text):
    return str(text).replace("|", "\\|")


def written(reading, recording, folder):
    """report.md and the pictures written into `folder`; the report's path."""
    pictures = os.path.join(folder, "pictures")
    os.makedirs(pictures, exist_ok=True)
    frames = recording.get("frames", [])
    first = frames[0]["t"] if frames else 0.0
    size = recording.get("size", {})
    kinds = {}
    for one in reading.findings:
        kinds[one.kind] = kinds.get(one.kind, 0) + 1
    lines = [
        "# Motion",
        "",
        "%d frames at %.1f ms a frame, the page %sx%s at %s device pixels to one; %d motions, %d findings%s."
        % (len(frames), reading.frame_ms, size.get("width", "?"), size.get("height", "?"), size.get("scale", "?"), len(reading.motions), len(reading.findings), (": " + ", ".join("%d %s" % (count, kind) for kind, count in sorted(kinds.items(), key=lambda one: -one[1]))) if kinds else ""),
        "",
    ]
    if recording.get("lateRead") is False:
        lines += ["The page keeps no long animation frames: the window's web view does not give them.", ""]
    lines += ["## Findings, the most first", "", "| # | kind | part | step | at ms | measure | what | tracks | picture |", "| ---: | --- | --- | --- | ---: | ---: | --- | --- | --- |"]
    for rank, one in enumerate(reading.findings, 1):
        picture = ""
        if rank <= PICTURED:
            name = "%03d_%s_%s.png" % (rank, one.kind.replace(" ", "_").replace("-", "_"), one.part)
            pictured(one, os.path.join(pictures, name))
            picture = "[picture](pictures/%s)" % name
        tracks = ", ".join(one.keys[:4]) + (" and %d more" % (len(one.keys) - 4) if len(one.keys) > 4 else "")
        lines.append("| %d | %s | %s | %s | %.0f | %.2f | %s | %s | %s |" % (rank, one.kind, cell(one.part), cell(one.step), one.start - first, one.measure, cell(one.said), cell(tracks), picture))
    if not reading.findings:
        lines.append("| | nothing found | | | | | | | |")
    lines += ["", "## Motions", "", "| track | step | from ms | ms | frames | from | to |", "| --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    for motion in reading.motions:
        lines.append("| %s | %s | %.0f | %.0f | %d | %.3f | %.3f |" % (cell(motion.key), cell(motion.step), motion.start - first, motion.duration, len(motion.times) - 1, motion.values[0], motion.values[-1]))
    late = sorted(recording.get("late", []), key=lambda one: -one.get("ms", 0))[:15]
    if late:
        lines += ["", "## Long animation frames, the longest first", "", "| from ms | ms | blocking ms | scripts |", "| ---: | ---: | ---: | --- |"]
        for one in late:
            scripts = "; ".join("%s %s %s ms" % (script.get("fn") or script.get("invoker") or "", script.get("at", ""), script.get("ms", "")) for script in one.get("scripts", [])[:4])
            lines.append("| %.0f | %.0f | %.0f | %s |" % (one["t"] - first, one["ms"], one.get("blocking", 0), cell(scripts)))
    marks = recording.get("marks", [])
    if marks:
        lines += ["", "## Steps", "", "| from ms | step |", "| ---: | --- |"]
        lines += ["| %.0f | %s |" % (mark["t"] - first, cell(mark["label"])) for mark in marks]
    path = os.path.join(folder, "report.md")
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    return path
