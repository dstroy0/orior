# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The motions a recording of the window holds, read: each track a part's channel, as `bar.top` or
`strip.--reveal-left`, its value on every frame motion.js kept; each motion a stretch of a track that moves, a few
frames held still inside it kept as part of it; each motion checked on its own, and each pair of motions that should
match held to each other.

A motion's progress runs from 0 where it left its rest to 1 where it came to rest again. A smooth one leaves its rest
without a jump, runs up to one top speed and down from it, and comes to rest without one; every frame inside it comes
when the recording's frames come. What it is held to on its own:

    late frame      a frame inside the motion FRAME_LATE of the recording's frames long or more
    stall           frames inside it where it stands still
    hitch           one frame where it runs at under DIP of the speed on either side
    two phases      a stretch of frames where it slows to under DIP of the speed before and after, then speeds up again
    start snap      its first frame taking START_SNAP of the way or more
    end snap        its last frame taking END_SNAP of the way or more, and three times the frame before it
    overshoot       running past its rest by OVERSHOOT of the way or more and coming back
    reversal        going back by REVERSAL of the way or more before it has come to rest

And in pairs:

    in and out differ   a track's way to a rest and its way back: lengths apart by PAIR_LENGTH, or curves by PAIR_CURVE
    repeat differs      the same step's motion of a track each time the step is taken: by REPEAT_LENGTH or REPEAT_CURVE
    starts mid-way      two parts' motions, neither starting with the other, within TOGETHER frames, nor after it ends

Each finding's measure is how far past its limit it stands, 1 at the limit; the findings come most first."""

import statistics
from dataclasses import dataclass, field

FRAME_LATE = 1.5
HOLD = 6
DIP = 0.5
START_SNAP = 0.3
END_SNAP = 0.12
OVERSHOOT = 0.02
REVERSAL = 0.02
PAIR_LENGTH = 1.25
PAIR_CURVE = 0.06
REPEAT_LENGTH = 1.15
REPEAT_CURVE = 0.05
TOGETHER = 2
SHAPE_POINTS = 32

# A step of a track this small is no move, a pixel's no more than the layout's sixty-fourth of one, and a motion moves
# at least LEAST: in pixels, in a number from 0 to 1 as the claim's numbers and an opacity are, and in square pixels
# for an area.
STEP = {"px": 0.02, "number": 1e-5, "area": 1.0}
LEAST = {"px": 1.0, "number": 0.01, "area": 100.0}
# The least speed a person sees, in each unit a millisecond: half a pixel a frame at sixty frames a second, and its
# like for a number and an area. A frame held still, a hitch or two phases count only where the motion about them
# runs at this or faster; slower, the layout's sixty-fourths of a pixel hold a slow tail still between its steps.
VISIBLE = {"px": 0.03, "number": 0.0003, "area": 3.0}
# How far a motion has gone when it is done to the eye: a part that starts after it, within TOGETHER frames of this,
# follows it.
DONE = 0.99

# The edge each part comes in from; a part named in none is the work's, which every edge moves.
ZONES = {"bar": "top", "toolbar": "top", "tabs": "top", "mark": "top", "strip": "left", "statusbar": "bottom", "minimap": "right", "controls": "controls"}


def cubic_bezier(x1, y1, x2, y2, x):
    """The CSS cubic-bezier(x1, y1, x2, y2) timing curve at `x`, from 0 to 1."""
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    low, high = 0.0, 1.0
    for _ in range(60):
        middle = (low + high) / 2.0
        along = 3 * x1 * middle * (1 - middle) ** 2 + 3 * x2 * middle**2 * (1 - middle) + middle**3
        if along < x:
            low = middle
        else:
            high = middle
    at = (low + high) / 2.0
    return 3 * y1 * at * (1 - at) ** 2 + 3 * y2 * at**2 * (1 - at) + at**3


def ease(x):
    """CSS's `ease`, which the claim's numbers run on."""
    return cubic_bezier(0.25, 0.1, 0.25, 1.0, x)


def unit_of(key):
    channel = key.split(".", 1)[1] if "." in key else key
    if channel.startswith("--") or channel == "opacity":
        return "number"
    if channel == "area":
        return "area"
    return "px"


@dataclass
class Motion:
    """A stretch of a track that moves: its frames from the rest it left to the frame it came to rest on."""

    key: str
    step: str
    times: list
    values: list

    @property
    def part(self):
        return self.key.split(".", 1)[0]

    @property
    def start(self):
        return self.times[0]

    @property
    def end(self):
        return self.times[-1]

    @property
    def duration(self):
        return self.times[-1] - self.times[0]

    @property
    def rising(self):
        return self.values[-1] > self.values[0]

    @property
    def done(self):
        """When it has gone DONE of its way."""
        return next((at for at, part in zip(self.times, self.progress) if part >= DONE), self.end)

    @property
    def progress(self):
        span = self.values[-1] - self.values[0]
        return [(value - self.values[0]) / span for value in self.values]

    def shape(self, points=SHAPE_POINTS):
        """Its progress at `points` even times from its start to its end."""
        progress = self.progress
        out = []
        for index in range(points):
            at = self.start + self.duration * index / (points - 1)
            later = next((one for one, time in enumerate(self.times) if time >= at), len(self.times) - 1)
            if later == 0 or self.times[later] == at:
                out.append(progress[later])
                continue
            before = later - 1
            part = (at - self.times[before]) / (self.times[later] - self.times[before])
            out.append(progress[before] + (progress[later] - progress[before]) * part)
        return out


@dataclass
class Finding:
    kind: str
    part: str
    step: str
    start: float
    end: float
    measure: float
    said: str
    keys: list = field(default_factory=list)
    motions: list = field(default_factory=list)
    frames: list = field(default_factory=list)


@dataclass
class Reading:
    frame_ms: float
    motions: list
    findings: list


def step_at(marks, at):
    """The label of the last mark at or before `at`, or ""."""
    label = ""
    for mark in marks:
        if mark["t"] <= at:
            label = mark["label"]
    return label


def motions_of(key, times, values, marks):
    """Each motion of one track."""
    unit = unit_of(key)
    moves = [False] + [abs(values[index] - values[index - 1]) > STEP[unit] for index in range(1, len(values))]
    found = []
    index = 1
    while index < len(values):
        if not moves[index]:
            index += 1
            continue
        first = last = index
        held = 0
        ahead = index + 1
        while ahead < len(values):
            if moves[ahead]:
                last = ahead
                held = 0
            else:
                held += 1
                if held > HOLD:
                    break
            ahead += 1
        if abs(values[last] - values[first - 1]) >= LEAST[unit]:
            found.append(Motion(key, step_at(marks, times[first - 1]), times[first - 1 : last + 1], values[first - 1 : last + 1]))
        index = last + 1
    return found


def finding(kind, motion, measure, said, frames=()):
    return Finding(kind, motion.part, motion.step, motion.start, motion.end, measure, said, [motion.key], [motion], list(frames))


def late_in(motion, frame_ms):
    """Whether a frame inside the motion came late: its snaps, its length and its curve are the late frames' then."""
    return any(later - earlier >= FRAME_LATE * frame_ms for earlier, later in zip(motion.times, motion.times[1:]))


def checked(motion, frame_ms):
    """What one motion is held to on its own."""
    out = []
    progress = motion.progress
    steps = [(progress[index] - progress[index - 1], motion.times[index] - motion.times[index - 1]) for index in range(1, len(progress))]
    still = STEP[unit_of(motion.key)]
    moved = [abs(motion.values[index] - motion.values[index - 1]) > still for index in range(1, len(motion.values))]
    late = [index for index, (_, lapse) in enumerate(steps) if lapse >= FRAME_LATE * frame_ms]
    if late:
        worst = max(steps[index][1] for index in late)
        out.append(finding("late frame", motion, worst / (FRAME_LATE * frame_ms), "%d late frame%s, the longest %.0f ms against %.1f ms" % (len(late), "" if len(late) == 1 else "s", worst, frame_ms), [motion.times[index + 1] for index in late]))
    unit = unit_of(motion.key)
    pace = [abs(motion.values[index + 1] - motion.values[index]) / max(steps[index][1], 1e-9) for index in range(len(steps))]

    def seen_about(index):
        """Whether the motion about a held step runs fast enough to be seen: the nearest moving steps either side."""
        before = next((pace[at] for at in range(index - 1, -1, -1) if moved[at]), 0.0)
        after = next((pace[at] for at in range(index + 1, len(steps)) if moved[at]), 0.0)
        return (before + after) / 2 >= VISIBLE[unit]

    stalled = [index for index in range(1, len(steps) - 1) if not moved[index] and seen_about(index)]
    if stalled:
        out.append(finding("stall", motion, len(stalled), "stands still %d frame%s inside its motion" % (len(stalled), "" if len(stalled) == 1 else "s"), [motion.times[index + 1] for index in stalled]))
    if steps and steps[0][0] >= START_SNAP and 0 not in late:
        out.append(finding("start snap", motion, steps[0][0] / START_SNAP, "its first frame takes %.0f%% of the way" % (100 * steps[0][0]), [motion.times[1]]))
    if len(steps) > 1 and steps[-1][0] >= END_SNAP and steps[-1][0] >= 3 * abs(steps[-2][0]) and len(steps) - 1 not in late:
        out.append(finding("end snap", motion, steps[-1][0] / END_SNAP, "its last frame takes %.0f%% of the way" % (100 * steps[-1][0]), [motion.end]))
    top = max(progress)
    if top >= 1 + OVERSHOOT:
        out.append(finding("overshoot", motion, (top - 1) / OVERSHOOT, "runs %.1f%% past its rest and comes back" % (100 * (top - 1)), [motion.times[progress.index(top)]]))
    peak = progress.index(top)
    back = [index for index in range(peak) if steps[index - 1][0] <= -REVERSAL] if peak else []
    if back:
        worst = min(steps[index - 1][0] for index in back)
        out.append(finding("reversal", motion, -worst / REVERSAL, "goes back %.0f%% of the way before it comes to rest" % (-100 * worst), [motion.times[index] for index in back]))
    out += speed_checked(motion, steps, moved, peak)
    return out


def speed_checked(motion, steps, moved, peak):
    """A hitch or two phases in the motion's speed, read over its steps from the second to the one before its last,
    up to its farthest point and up to its first step back, which the reversal says, a step held still or the step
    after it left out, as the stall says them."""
    back = next((index for index, (part, _) in enumerate(steps) if part < 0), len(steps))
    kept = []
    for index in range(1, min(len(steps) - 1, max(peak, 1), back)):
        if moved[index] and moved[index - 1]:
            kept.append((index, steps[index][0] / steps[index][1]))
    if len(kept) < 3:
        return []
    speeds = [speed for _, speed in kept]
    low = [at for at in range(1, len(speeds) - 1) if speeds[at] < DIP * min(max(speeds[:at]), max(speeds[at + 1 :]))]
    out = []
    runs = []
    for at in low:
        if runs and at == runs[-1][-1] + 1:
            runs[-1].append(at)
        else:
            runs.append([at])
    seen = VISIBLE[unit_of(motion.key)] / max(abs(motion.values[-1] - motion.values[0]), 1e-9)
    for run in runs:
        around = min(max(speeds[: run[0]]), max(speeds[run[-1] + 1 :]))
        deepest = min(speeds[at] for at in run)
        if around < seen:
            continue
        frames = [motion.times[kept[at][0] + 1] for at in run]
        if len(run) == 1:
            out.append(finding("hitch", motion, around * DIP / max(deepest, 1e-9), "one frame runs at %.0f%% of the speed either side of it" % (100 * deepest / around), frames))
        else:
            out.append(finding("two phases", motion, len(run), "slows over %d frames to %.0f%% of its speed, then speeds up again" % (len(run), 100 * deepest / around), frames))
    return out


def curve_apart(one, other):
    """How far apart two motions' curves stand: the root mean square of their progress at even times."""
    first, second = one.shape(), other.shape()
    return (sum((a - b) ** 2 for a, b in zip(first, second)) / len(first)) ** 0.5


def edge_of(step):
    """The edge a step comes in or goes out at, `top` of `top in` and of `top out`; the step itself where it names
    neither, so two such steps pair only with each other."""
    words = step.rsplit(" ", 1)
    return words[0] if len(words) == 2 and words[1] in ("in", "out") else step


def paired(motions, frame_ms):
    """A track's way in against its way out, and the same step's motion of a track each time the step is taken; a
    motion a late frame came in left out, as the late frame says it."""
    out = []
    tracks = {}
    for motion in motions:
        if not late_in(motion, frame_ms):
            tracks.setdefault(motion.key, []).append(motion)
    for key, own in tracks.items():
        own.sort(key=lambda one: one.start)
        index = 0
        while index + 1 < len(own):
            one, other = own[index], own[index + 1]
            if one.rising == other.rising or edge_of(one.step) != edge_of(other.step):
                index += 1
                continue
            lengths = max(one.duration, other.duration) / max(min(one.duration, other.duration), 1e-9)
            apart = curve_apart(one, other)
            if lengths >= PAIR_LENGTH or apart >= PAIR_CURVE:
                out.append(Finding("in and out differ", one.part, "%s / %s" % (one.step, other.step), one.start, other.end, max(lengths / PAIR_LENGTH, apart / PAIR_CURVE), "%.0f ms one way and %.0f ms back, their curves %.3f apart" % (one.duration, other.duration, apart), [key], [one, other]))
            index += 2
        steps = {}
        for motion in own:
            steps.setdefault((motion.step, motion.rising), []).append(motion)
        for (step, _), taken in steps.items():
            if len(taken) < 2:
                continue
            lengths = max(one.duration for one in taken) / max(min(one.duration for one in taken), 1e-9)
            apart = max(curve_apart(taken[0], one) for one in taken[1:])
            if lengths >= REPEAT_LENGTH or apart >= REPEAT_CURVE:
                out.append(Finding("repeat differs", taken[0].part, step, taken[0].start, taken[-1].end, max(lengths / REPEAT_LENGTH, apart / REPEAT_CURVE), "taken %d times: %s ms, curves up to %.3f apart" % (len(taken), ", ".join("%.0f" % one.duration for one in taken), apart), [key], list(taken)))
    return out


def edges_apart(one, other):
    """Whether two parts come in from different edges, the head and the controls apart, which come together. The
    work's own parts come in from none: they give way to whatever comes, as it reaches them."""
    zones = {ZONES.get(one.part), ZONES.get(other.part)}
    return None not in zones and len(zones) == 2 and zones != {"top", "controls"}


def overlapped(motions, frame_ms):
    """Two edges' parts' motions where one starts while the other is mid-way: neither together nor one after the
    other."""
    out = []
    worst = {}
    ordered = sorted(motions, key=lambda one: one.start)
    for at, one in enumerate(ordered):
        for other in ordered[at + 1 :]:
            if other.start > one.end:
                break
            if not edges_apart(one, other):
                continue
            offset = other.start - one.start
            if offset <= TOGETHER * frame_ms or other.start >= one.done - TOGETHER * frame_ms:
                continue
            measure = offset / (TOGETHER * frame_ms)
            pair = (one.part, other.part, one.step)
            if pair not in worst or measure > worst[pair].measure:
                worst[pair] = Finding("starts mid-way", other.part, one.step, one.start, other.end, measure, "%s starts %.0f ms into %s's %.0f ms" % (other.key, offset, one.key, one.duration), [one.key, other.key], [one, other])
    out += worst.values()
    return out


def collapsed(findings):
    """One finding for each kind, part and step, the worst, with every track it was found on; late frames once for
    each step, as they are the frames' and not a part's."""
    kept = {}
    for one in findings:
        if one.kind == "late frame":
            one.part = "frames"
        key = (one.kind, one.part, one.step)
        if key not in kept:
            kept[key] = one
            continue
        held = kept[key]
        keys = held.keys + [name for name in one.keys if name not in held.keys]
        if one.measure > held.measure:
            one.keys = keys
            kept[key] = one
        else:
            held.keys = keys
    return sorted(kept.values(), key=lambda one: -one.measure)


def read(recording):
    """A recording's motions and findings."""
    frames = recording["frames"]
    marks = sorted(recording.get("marks", []), key=lambda mark: mark["t"])
    times = [frame["t"] for frame in frames]
    lapses = [later - earlier for earlier, later in zip(times, times[1:])]
    frame_ms = statistics.median(lapses) if lapses else 1000.0 / 60.0
    keys = sorted({key for frame in frames for key in frame["values"]})
    motions = []
    for key in keys:
        # a track runs over the frames that hold its channel, one after another; a frame without it breaks it, as a
        # clip a script takes off leaves none to read until it cuts one again
        stretches = [[]]
        for frame in frames:
            value = frame["values"].get(key)
            if isinstance(value, (int, float)):
                stretches[-1].append((frame["t"], value))
            elif stretches[-1]:
                stretches.append([])
        for kept in stretches:
            if len(kept) > 1:
                motions += motions_of(key, [at for at, _ in kept], [value for _, value in kept], marks)
    motions.sort(key=lambda one: (one.start, one.key))
    findings = []
    for motion in motions:
        findings += checked(motion, frame_ms)
    findings += paired(motions, frame_ms)
    findings += overlapped(motions, frame_ms)
    return Reading(frame_ms, motions, collapsed(findings))
