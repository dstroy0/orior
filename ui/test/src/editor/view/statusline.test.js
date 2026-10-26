// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's place on the page: the word before the cursor that completion replaces, and the rectangle a place
// stands in, a line's height below the line above it.

import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("the prefix is the word before the primary cursor, and empty after a sign", async () => {
  const view = await given("let total = sub|");
  const word = view.prefix();
  assert.equal(word.text, "sub");
  assert.deepEqual(word.from, { line: 0, col: 12 });
  await given("call(|");
  assert.equal(view.prefix().text, "");
});

test("a place on the next line stands a line's height lower, at the same left for the same col", async () => {
  const view = await given("abc\nabc");
  const upper = view.rectOf({ line: 0, col: 2 });
  const lower = view.rectOf({ line: 1, col: 2 });
  assert.equal(lower.left, upper.left);
  assert.equal(lower.top, upper.bottom);
  assert.ok(upper.bottom > upper.top);
});

test("the window is put back as the file found it", putBackAsFound);
