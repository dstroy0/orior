// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A selection drawn as one rounded outline over the rows it covers.

import { selectionPath } from "../../../gui/src-js/editor/shape.js";

const { test, assert } = window.__harness;

const shapes = (path) => path.split("Z").filter(Boolean).length;
const corners = (path) => (path.match(/Q/g) ?? []).length;

test("one row's span is a rectangle of four rounded corners", () => {
  const path = selectionPath([[0, 10, 50]], 20, 3);
  assert.equal(shapes(path), 1);
  assert.equal(corners(path), 4);
  assert.match(path, /^M10 3Q10 0 13 0/, "the top left corner rounded by the radius");
});

test("rows that overlap make one outline, a longer row under a shorter making two more corners", () => {
  const path = selectionPath([[0, 30, 80], [1, 10, 80]], 20, 3);
  assert.equal(shapes(path), 1);
  assert.equal(corners(path), 6);
});

test("rows that do not overlap, or that do not follow one another, are shapes of their own, and an empty span draws nothing", () => {
  assert.equal(shapes(selectionPath([[0, 60, 80], [1, 10, 40]], 20, 3)), 2);
  assert.equal(shapes(selectionPath([[0, 10, 80], [2, 10, 80]], 20, 3)), 2);
  assert.equal(selectionPath([[0, 10, 10]], 20, 3), "");
});

test("a corner is rounded by less than the radius where a side is too short for it", () => {
  const path = selectionPath([[0, 0, 4]], 20, 10);
  assert.match(path, /^M0 2Q0 0 2 0/, "half the four pixels across");
});
