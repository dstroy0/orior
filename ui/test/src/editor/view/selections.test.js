// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's selections: set in order with those that overlap merged, the primary retained, copied apart from the
// editor's own, and moved by a character, a pair of surrogates whole, a word, and with the anchor retained.

import { given, marked, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

const caret = (line, col) => ({ anchor: { line, col }, head: { line, col }, goal: null });

test("selections are set in order, those that overlap or touch merged, and the one chosen is the primary", async () => {
  const view = await given("abcdefgh");
  const chosen = { anchor: { line: 0, col: 1 }, head: { line: 0, col: 3 }, goal: null };
  view.select([{ anchor: { line: 0, col: 5 }, head: { line: 0, col: 7 }, goal: null }, { anchor: { line: 0, col: 2 }, head: { line: 0, col: 4 }, goal: null }, chosen], chosen);
  assert.equal(marked(), "a[bcd]e[fg]h", "the first two merged, in order");
  assert.deepEqual([view.primary().anchor.col, view.primary().head.col], [1, 4], "the merged one holds the primary");
  const copy = view.copySelections();
  copy[0].head.col = 0;
  assert.equal(view.s.selections[0].head.col, 4, "a copy changes nothing of the editor's");
  view.select([caret(0, 99)]);
  assert.equal(marked(), "abcdefgh|", "a place past the line's end is clamped to it");
});

test("a character left and right steps over a pair of surrogates whole and across line ends", async () => {
  const view = await given("a😀b\nc");
  assert.deepEqual(view.charRight({ line: 0, col: 1 }), { line: 0, col: 3 });
  assert.deepEqual(view.charLeft({ line: 0, col: 3 }), { line: 0, col: 1 });
  assert.deepEqual(view.charRight({ line: 0, col: 4 }), { line: 1, col: 0 });
  assert.deepEqual(view.charLeft({ line: 1, col: 0 }), { line: 0, col: 4 });
  assert.deepEqual(view.charLeft({ line: 0, col: 0 }), { line: 0, col: 0 });
});

test("a word left and right stops at the ends of words and of runs of signs, past white space", async () => {
  const view = await given("one, two  three");
  assert.deepEqual(view.wordRight({ line: 0, col: 0 }), { line: 0, col: 3 });
  assert.deepEqual(view.wordRight({ line: 0, col: 3 }), { line: 0, col: 4 }, "the comma a run of its own");
  assert.deepEqual(view.wordRight({ line: 0, col: 4 }), { line: 0, col: 8 });
  assert.deepEqual(view.wordLeft({ line: 0, col: 15 }), { line: 0, col: 10 });
  assert.deepEqual(view.wordLeft({ line: 0, col: 10 }), { line: 0, col: 5 });
});

test("moving every selection by a step, the anchor retained where the move extends", async () => {
  const view = await given("|one two\n|three four");
  view.moveBy((sel) => view.wordRight(sel.head), false);
  assert.equal(marked(), "one| two\nthree| four");
  view.moveBy((sel) => view.wordRight(sel.head), true);
  assert.equal(marked(), "one[ two]\nthree[ four]");
});

test("the window is put back as the file found it", putBackAsFound);
