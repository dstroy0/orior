#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Tests for motion_read: each motion a recording holds found and read, a smooth one passing with nothing said and
# each way a motion goes wrong found once, by its type, on curves drawn here at sixty frames a second.
#
#   Usage:  python utils/test/test_motion_read.py

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import motion_read  # noqa: E402

FRAME = 1000.0 / 60.0


def clock(until, skip=None):
    """The frame times from 0 to `until` ms, those inside `skip`, a (from, to) in ms, left out as a late frame leaves
    them."""
    times = []
    at = 0.0
    while at <= until:
        if not skip or not (skip[0] < at < skip[1]):
            times.append(at)
        at += FRAME
    return times


def moving(start_value, end_value, start, duration, curve=motion_read.ease):
    """A value at rest at `start_value` until `start`, moving on `curve` for `duration` ms to `end_value`, at rest
    after."""

    def value(at):
        if at <= start:
            return start_value
        if at >= start + duration:
            return end_value
        return start_value + (end_value - start_value) * curve((at - start) / duration)

    return value


def recording(times, tracks, marks=()):
    """A recording as motion.js retains one: a frame each time, each track's value in it."""
    frames = [{"t": at, "values": {key: track(at) for key, track in tracks.items()}} for at in times]
    return {"frames": frames, "marks": [{"t": at, "label": label} for at, label in marks]}


def types(reading):
    return sorted({finding.type for finding in reading.findings})


class Ease(unittest.TestCase):
    def test_the_css_ease_curve(self):
        self.assertEqual(motion_read.ease(0.0), 0.0)
        self.assertEqual(motion_read.ease(1.0), 1.0)
        self.assertAlmostEqual(motion_read.ease(0.5), 0.8024033877399112, places=6)


class OneMotion(unittest.TestCase):
    def test_a_smooth_motion_is_found_once_and_passes(self):
        reading = motion_read.read(recording(clock(700), {"bar.top": moving(0, 100, 100, 300)}, [(90, "top in")]))
        self.assertEqual(len(reading.motions), 1)
        motion = reading.motions[0]
        self.assertEqual(motion.key, "bar.top")
        self.assertEqual(motion.step, "top in")
        self.assertLess(abs(motion.duration - 300), 2 * FRAME)
        self.assertEqual(reading.findings, [])

    def test_a_late_frame_inside_a_motion(self):
        reading = motion_read.read(recording(clock(700, skip=(200, 250)), {"bar.top": moving(0, 100, 100, 300)}))
        self.assertEqual(types(reading), ["late frame"])

    def test_late_frames_under_two_parts_are_said_once(self):
        tracks = {"bar.top": moving(0, 100, 100, 300), "tabs.top": moving(0, 30, 100, 300)}
        reading = motion_read.read(recording(clock(700, skip=(200, 250)), tracks, [(90, "top in")]))
        self.assertEqual(types(reading), ["late frame"])
        self.assertEqual(len(reading.findings), 1)
        self.assertEqual(sorted(reading.findings[0].keys), ["bar.top", "tabs.top"])

    def test_a_snap_on_a_late_frame_is_the_late_frame_s(self):
        rest = moving(35, 100, 150, 250)
        snapped = lambda at: 0.0 if at <= 100 else rest(at)  # noqa: E731
        reading = motion_read.read(recording(clock(700, skip=(100, 150)), {"bar.top": snapped}))
        self.assertEqual(types(reading), ["late frame"])

    def test_a_late_frame_while_nothing_moves_says_nothing(self):
        reading = motion_read.read(recording(clock(700, skip=(500, 560)), {"bar.top": moving(0, 100, 100, 300)}))
        self.assertEqual(reading.findings, [])

    def test_a_stall(self):
        smooth = moving(0, 100, 100, 300)
        stalled = lambda at: smooth(180.0) if 180.0 <= at < 230.0 else smooth(at)  # noqa: E731
        reading = motion_read.read(recording(clock(700), {"bar.top": stalled}))
        self.assertEqual(len(reading.motions), 1)
        self.assertEqual(types(reading), ["stall"])

    def test_a_slow_tail_on_the_layout_s_sixty_fourths_is_no_stall(self):
        fine = 1000.0 / 144.0
        times = [index * fine for index in range(int(700 / fine))]
        smooth = moving(0, 4, 100, 300)
        frames = [{"t": at, "values": {"editor.left": round(smooth(at) * 64) / 64}} for at in times]
        reading = motion_read.read({"frames": frames, "marks": []})
        self.assertEqual(len(reading.motions), 1)
        self.assertEqual(reading.findings, [])

    def test_a_hitch(self):
        times = clock(700)
        smooth = moving(0, 100, 100, 300)
        values = [smooth(at) for at in times]
        hitch = next(index for index, at in enumerate(times) if at > 230)
        values[hitch] = values[hitch - 1] + 0.25 * (values[hitch] - values[hitch - 1])
        frames = [{"t": at, "values": {"bar.top": value}} for at, value in zip(times, values)]
        reading = motion_read.read({"frames": frames, "marks": []})
        self.assertEqual(types(reading), ["hitch"])

    def test_a_start_snap(self):
        rest = moving(40, 100, 100, 300)
        snapped = lambda at: 0.0 if at <= 100 else rest(at)  # noqa: E731
        reading = motion_read.read(recording(clock(700), {"bar.top": snapped}))
        self.assertEqual(types(reading), ["start snap"])

    def test_an_end_snap(self):
        max = moving(0, 80, 100, 250)
        snapped = lambda at: 100.0 if at >= 100 + 250 + FRAME else max(at)  # noqa: E731
        reading = motion_read.read(recording(clock(700), {"bar.top": snapped}))
        self.assertEqual(types(reading), ["end snap"])

    def test_two_phases(self):
        first = moving(0, 50, 100, 150)
        second = moving(50, 100, 250, 150)
        both = lambda at: first(at) if at < 250 else second(at)  # noqa: E731
        reading = motion_read.read(recording(clock(700), {"bar.top": both}))
        self.assertEqual(len(reading.motions), 1)
        self.assertEqual(types(reading), ["two phases"])

    def test_an_overshoot(self):
        out = moving(0, 106, 100, 250)
        back = moving(106, 100, 350, 120)
        both = lambda at: out(at) if at < 350 else back(at)  # noqa: E731
        reading = motion_read.read(recording(clock(800), {"bar.top": both}))
        self.assertEqual(len(reading.motions), 1)
        self.assertIn("overshoot", types(reading))

    def test_a_track_absent_between_two_stretches_joins_neither(self):
        times = clock(2000)
        first = moving(0, 100, 100, 300)
        frames = []
        for at in times:
            values = {"bar.top": 5.0}
            if at < 500:
                values["work.area"] = first(at) * 1000
            elif at > 1500:
                values["work.area"] = 40000.0
            frames.append({"t": at, "values": values})
        reading = motion_read.read({"frames": frames, "marks": []})
        self.assertEqual([one.key for one in reading.motions], ["work.area"])
        self.assertLess(reading.motions[0].duration, 400)
        self.assertEqual(reading.findings, [])

    def test_a_motion_that_turns_back_is_a_reversal_and_no_hitch(self):
        out = moving(0, 60, 100, 150)
        back = moving(60, 40, 250, 60)
        on = moving(40, 100, 310, 200)
        track = lambda at: out(at) if at < 250 else back(at) if at < 310 else on(at)  # noqa: E731
        reading = motion_read.read(recording(clock(800), {"bar.top": track}))
        self.assertEqual(types(reading), ["reversal"])

    def test_one_frame_stepping_back_less_than_a_reversal_is_no_hitch(self):
        times = clock(700)
        smooth = moving(0, 100, 100, 300)
        values = [smooth(at) for at in times]
        back = next(index for index, at in enumerate(times) if at > 250)
        values[back] = values[back - 1] - 0.5
        frames = [{"t": at, "values": {"bar.top": value}} for at, value in zip(times, values)]
        reading = motion_read.read({"frames": frames, "marks": []})
        self.assertNotIn("hitch", types(reading))
        self.assertNotIn("two phases", types(reading))

    def test_a_still_track_and_a_shiver_are_no_motion(self):
        shiver = lambda at: 50.0 + (0.004 if int(at / FRAME) % 2 else 0.0)  # noqa: E731
        reading = motion_read.read(recording(clock(700), {"bar.top": lambda at: 12.0, "bar.left": shiver}))
        self.assertEqual(reading.motions, [])


class Pairs(unittest.TestCase):
    def test_in_and_out_on_one_curve_and_length_pass(self):
        inward = moving(0, 100, 100, 300)
        outward = moving(100, 0, 1000, 300)
        track = lambda at: inward(at) if at < 1000 else outward(at)  # noqa: E731
        reading = motion_read.read(recording(clock(1600), {"strip.left": track}, [(90, "left in"), (990, "left out")]))
        self.assertEqual(len(reading.motions), 2)
        self.assertEqual(reading.findings, [])

    def test_in_and_out_of_different_lengths(self):
        inward = moving(0, 100, 100, 300)
        outward = moving(100, 0, 1000, 500)
        track = lambda at: inward(at) if at < 1000 else outward(at)  # noqa: E731
        reading = motion_read.read(recording(clock(1800), {"strip.left": track}, [(90, "left in"), (990, "left out")]))
        self.assertEqual(types(reading), ["in and out differ"])

    def test_a_shared_track_s_motions_under_two_edges_are_no_pair(self):
        outward = moving(100, 0, 100, 220)
        inward = moving(0, 100, 1000, 300)
        track = lambda at: outward(at) if at < 1000 else inward(at)  # noqa: E731
        reading = motion_read.read(recording(clock(1600), {"work.area": lambda at: track(at) * 100}, [(90, "controls out"), (990, "left in")]))
        self.assertEqual(reading.findings, [])

    def test_a_step_repeated_differs(self):
        cycle = [moving(0, 100, 100, 300), moving(100, 0, 800, 300), moving(0, 100, 1500, 400), moving(100, 0, 2300, 400)]
        starts = [100, 800, 1500, 2300]

        def track(at):
            last = max(index for index, start in enumerate(starts) if at >= start or index == 0)
            return cycle[last](at)

        marks = [(90, "top in"), (790, "top out"), (1490, "top in"), (2290, "top out")]
        reading = motion_read.read(recording(clock(3000), {"bar.top": track}, marks))
        self.assertIn("repeat differs", types(reading))

    def test_a_repeat_slowed_by_late_frames_is_the_late_frames_s(self):
        first = moving(0, 100, 100, 360)
        second = moving(100, 0, 900, 300)
        third = moving(0, 100, 1600, 300)
        track = lambda at: first(at) if at < 900 else second(at) if at < 1600 else third(at)  # noqa: E731
        marks = [(90, "top in"), (890, "top out"), (1590, "top in")]
        reading = motion_read.read(recording(clock(2200, skip=(150, 200)), {"bar.top": track}, marks))
        self.assertEqual(types(reading), ["late frame"])

    def test_the_work_giving_way_and_the_controls_with_the_head_follow_no_rule(self):
        tracks = {"bar.top": moving(0, 30, 100, 300), "work.area": moving(0, 5000, 250, 150), "controls.top": moving(0, 30, 200, 300)}
        reading = motion_read.read(recording(clock(900), tracks, [(90, "top in")]))
        self.assertEqual(reading.findings, [])

    def test_two_parts_that_blend_or_follow_pass(self):
        tracks = {"bar.top": moving(0, 30, 100, 300), "strip.left": moving(0, 50, 100, 300), "statusbar.top": moving(0, 20, 420, 300)}
        reading = motion_read.read(recording(clock(900), tracks, [(90, "switch")]))
        self.assertEqual(reading.findings, [])

    def test_a_part_that_starts_in_another_s_last_hundredth_follows_it(self):
        tracks = {"bar.--reveal-top": moving(0, 1, 100, 300), "frame.--room-top": moving(0, 1, 100 + 292, 300)}
        reading = motion_read.read(recording(clock(900), tracks, [(90, "top in")]))
        self.assertEqual(reading.findings, [])

    def test_a_part_that_starts_while_another_is_mid_way(self):
        tracks = {"bar.top": moving(0, 30, 100, 300), "strip.left": moving(0, 50, 250, 300)}
        reading = motion_read.read(recording(clock(900), tracks, [(90, "switch")]))
        self.assertEqual(types(reading), ["starts mid-way"])


if __name__ == "__main__":
    unittest.main()
