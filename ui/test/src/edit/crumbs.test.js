// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The diagnostics stepped through, and the problems fed to the Problems pane.

import { given, marked, press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Go, Next Problem and Previous Problem step through the diagnostics", async () => {
  const view = await given("a|b\ncd\nef");
  // The checks that run as the text rests come first; the diagnostics are set after them.
  await ui.rest(800);
  view.s.diagnostics = [
    { from: { line: 2, col: 0 }, to: { line: 2, col: 1 }, severity: 1, message: "second" },
    { from: { line: 1, col: 1 }, to: { line: 1, col: 2 }, severity: 2, message: "first" },
  ];
  try {
    await press("next-problem");
    assert.equal(marked(), "ab\nc|d\nef");
    await press("next-problem");
    assert.equal(marked(), "ab\ncd\n|ef");
    await press("previous-problem");
    assert.equal(marked(), "ab\nc|d\nef");
  } finally {
    view.s.diagnostics = [];
  }
});

test("the window is put back as the file found it", putBackAsFound);
