// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's tabs and files: which side of a large file, read a slice at a time, is read next.

import { nextSlice } from "../../src/edit.js";

const { test, assert } = window.__harness;

test("a large file is read the way the view heads while it scrolls", () => {
  const both = { canAbove: true, canBelow: true, moving: true };
  assert.equal(nextSlice({ ...both, heading: -1, below: true }), "above");
  assert.equal(nextSlice({ ...both, heading: 1, below: false }), "below");
});

test("at rest a large file is read below and above in turn", () => {
  const both = { canAbove: true, canBelow: true, moving: false, heading: -1 };
  assert.equal(nextSlice({ ...both, below: true }), "below");
  assert.equal(nextSlice({ ...both, below: false }), "above");
});

test("a side read to its end is read no more, whichever way the view heads", () => {
  assert.equal(nextSlice({ canAbove: false, canBelow: true, moving: true, heading: -1, below: false }), "below");
  assert.equal(nextSlice({ canAbove: true, canBelow: false, moving: true, heading: 1, below: true }), "above");
});
