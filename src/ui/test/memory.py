#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Watches the memory of every command of orior's menus, walked as walk.py walks them on its scratch
# tree of dummy files, each pressed once under V8's sampling heap profiler: what the page still holds
# once the command is closed and put back and the heap collected, by module and by function; the
# heap's growth; each process's; and the bytes each call of the tree's side took and kept, which the
# window counts as its allocator gives them. Nothing is timed or filmed, the profiler taking the
# page's time. memory.md says it, the most held first, beside memory.json.
#
#   Usage:  python -I src/ui/test/memory.py [--only <command,...>] [--menus <title,...>] [--skip <command,...>]
#                                            [--port 9222] [--wait 8] [--sampling 64]

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from walk import Walker, options  # noqa: E402

if __name__ == "__main__":
    Walker(options("Watch the memory each command of orior's menus leaves held, by module, function and call.", memory_only=True)).run()
