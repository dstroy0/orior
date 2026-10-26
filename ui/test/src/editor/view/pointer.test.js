// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's pointer: the span a double press takes is the word under it, and a triple press the whole line with
// its break.

import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

const at = (line, col) => ({ line, col });

test("a word's span is the word under the place, a line's the line to the next line's start, else the place", async () => {
  const view = await given("one two three\nlast");
  assert.deepEqual(view.unitRange("word", at(0, 5)), [at(0, 4), at(0, 7)]);
  assert.deepEqual(view.unitRange("line", at(0, 5)), [at(0, 0), at(1, 0)]);
  assert.deepEqual(view.unitRange("line", at(1, 2)), [at(1, 0), at(1, 4)], "the last line to its end");
  assert.deepEqual(view.unitRange("char", at(0, 5)), [at(0, 5), at(0, 5)]);
});

test("the window is put back as the file found it", putBackAsFound);
