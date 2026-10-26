// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lines of a text that differ from an older text of it: added, changed, where lines were taken out, and each
// change as a hunk of both texts.

import { lineChanges } from "../../../gui/src-js/editor/diff.js";

const { test, assert } = window.__harness;

const sorted = (set) => [...set].sort((a, b) => a - b);

test("the same text has no changes", () => {
  const marks = lineChanges(["a", "b"], ["a", "b"]);
  assert.deepEqual([sorted(marks.added), sorted(marks.changed), sorted(marks.removed), marks.hunks], [[], [], [], []]);
});

test("lines put in where none came out are added, and each change is a hunk of both texts", () => {
  const marks = lineChanges(["a", "b", "c"], ["a", "x", "y", "b", "c"]);
  assert.deepEqual(sorted(marks.added), [1, 2]);
  assert.deepEqual(sorted(marks.changed), []);
  assert.deepEqual(marks.hunks, [{ now: [1, 3], then: [1, 1] }]);
});

test("lines put in where some came out are changed, and lines only taken out mark the line below where they were", () => {
  const changed = lineChanges(["a", "b", "c"], ["a", "B", "c"]);
  assert.deepEqual(sorted(changed.changed), [1]);
  assert.deepEqual(changed.hunks, [{ now: [1, 2], then: [1, 2] }]);
  const removed = lineChanges(["a", "b", "c", "d"], ["a", "d"]);
  assert.deepEqual(sorted(removed.removed), [1]);
  assert.deepEqual(removed.hunks, [{ now: [1, 1], then: [1, 3] }]);
  const atEnd = lineChanges(["a", "b"], ["a"]);
  assert.deepEqual(sorted(atEnd.removed), [1], "below the last line");
});

test("changes apart from one another are hunks of their own, in order", () => {
  const marks = lineChanges(["a", "b", "c", "d", "e"], ["A", "b", "c", "d", "E", "f"]);
  assert.deepEqual(marks.hunks, [{ now: [0, 1], then: [0, 1] }, { now: [4, 6], then: [4, 5] }]);
  assert.deepEqual(sorted(marks.changed), [0, 4, 5]);
});

test("texts too far apart to mark give null", () => {
  const then = Array.from({ length: 2000 }, (_, at) => `then ${at}`);
  const now = Array.from({ length: 2000 }, (_, at) => `now ${at}`);
  assert.equal(lineChanges(then, now), null);
});
