#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Tests for motion_report: a reading written as report.md, a picture for each finding and a row for each motion,
# into build/harness/motion_report_test/.
#
#   Usage:  python utils/test/test_motion_report.py

import os
import shutil
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

import motion_read  # noqa: E402
import motion_report  # noqa: E402
from test_motion_read import clock, moving, recording  # noqa: E402
from utils.tools import findroot  # noqa: E402

FOLDER = findroot.at("build", "harness", "motion_report_test")


class Written(unittest.TestCase):
    def setUp(self):
        shutil.rmtree(FOLDER, ignore_errors=True)

    def test_each_finding_pictured_and_each_motion_listed(self):
        smooth = moving(0, 100, 100, 300)
        held = lambda at: smooth(180.0) if 180.0 <= at < 230.0 else smooth(at)  # noqa: E731
        inward = moving(0, 100, 100, 300)
        outward = moving(100, 0, 1000, 500)
        tracks = {"bar.top": held, "strip.left": lambda at: inward(at) if at < 1000 else outward(at), "minimap.left": moving(0, 40, 160, 300)}
        taken = recording(clock(1800), tracks, [(90, "left in"), (990, "left out")])
        reading = motion_read.read(taken)
        path = motion_report.written(reading, taken, FOLDER)
        with open(path, encoding="utf-8") as kept:
            text = kept.read()
        for one in reading.findings:
            self.assertIn("| %s |" % one.kind, text)
        self.assertEqual(len(os.listdir(os.path.join(FOLDER, "pictures"))), len(reading.findings))
        self.assertEqual(text.count("\n| bar.top |") + text.count("\n| strip.left |") + text.count("\n| minimap.left |"), len(reading.motions))
        self.assertGreaterEqual(len(reading.findings), 3)


if __name__ == "__main__":
    unittest.main()
