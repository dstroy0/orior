// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What folds, read from indentation; the rows a text shows once regions fold and long lines wrap; where a long
// line wraps; and the spans of hidden lines merged.

import { hiddenSpans, indentOf, joinSpans, openersOf, opens, regionEnd, regions, Rows, wrapBreaks } from "../../../gui/src-js/editor/folding.js";

const { test, assert } = window.__harness;

const docOf = (text) => {
  const lines = text.split("\n");
  return { count: lines.length, line: (at) => lines[at] };
};
const CODE = docOf("def a():\n    x = 1\n\n    if x:\n        y = 2\n    return x\ndef b():\n    pass");

test("a line's indent counts spaces, and a tab to its next stop; a blank line has none", () => {
  assert.equal(indentOf("    x", 4), 4);
  assert.equal(indentOf("  \tx", 4), 4);
  assert.equal(indentOf("\t\tx", 4), 8);
  assert.equal(indentOf("   ", 4), -1);
});

test("a line opens a region when the next line not blank is deeper, and the region runs to the last line deeper than it", () => {
  assert.equal(opens(CODE, 0, 4), true);
  assert.equal(opens(CODE, 1, 4), false);
  assert.equal(regionEnd(CODE, 0, 4), 5, "the blank line inside is passed over");
  assert.equal(regionEnd(CODE, 3, 4), 4);
  assert.equal(regionEnd(CODE, 1, 4), -1);
  assert.deepEqual([...regions(CODE, 4)], [[3, 4], [0, 5], [6, 7]]);
});

test("the lines that open the regions around a line, the outermost first, a blank line inside what follows it", () => {
  assert.deepEqual(openersOf(CODE, 4, 4, 10), [0, 3]);
  assert.deepEqual(openersOf(CODE, 4, 4, 1), [3], "the innermost alone");
  assert.deepEqual(openersOf(CODE, 2, 4, 10), [0]);
  assert.deepEqual(openersOf(CODE, 6, 4, 10), []);
});

test("rows: a folded region's lines take its opening line's row, and a wrapped line takes a row more for each break", () => {
  const rows = new Rows(6, [[1, 2]], (line) => (line === 4 ? [10, 20] : null));
  assert.equal(rows.size, 6, "six lines, two hidden, one in three rows");
  assert.deepEqual([0, 1, 2, 3, 4].map((row) => rows.lineOf(row)), [0, 3, 4, 4, 4]);
  assert.equal(rows.rowOf(2), 0, "hidden, on the row of the line that opens the fold");
  assert.equal(rows.rowOf(5), 5);
  assert.deepEqual([rows.startOf(2), rows.startOf(3), rows.startOf(4)], [0, 10, 20]);
  assert.deepEqual([rows.endOf(2), rows.endOf(3), rows.endOf(4)], [10, 20, -1]);
  assert.equal(rows.rowAt(4, 15), 3);
  assert.equal(rows.rowAt(4, 20), 4, "a col where a row starts is on that row");
  assert.equal(rows.lastRowOf(4), 4);
  const plain = new Rows(3, []);
  assert.deepEqual([plain.size, plain.lineOf(2), plain.rowOf(1), plain.endOf(0)], [3, 2, 1, -1]);
});

test("a long line wraps after the last space that fits, or where the columns run out where none does", () => {
  assert.equal(wrapBreaks("short", 10, 4), null);
  assert.deepEqual(wrapBreaks("one two three four", 10, 4), [8], "\"three four\" fills the second row's ten columns");
  assert.deepEqual(wrapBreaks("one two three fours", 10, 4), [8, 14]);
  assert.deepEqual(wrapBreaks("abcdefghijklmnop", 6, 4), [6, 12]);
  assert.deepEqual(wrapBreaks("\t\t\tx", 8, 4), [2], "a tab counts to its stop");
});

test("the folded regions' hidden lines, and two lists of spans, merged and in order", () => {
  assert.deepEqual(hiddenSpans(new Map([[0, 5], [3, 4], [8, 9]])), [[1, 5], [9, 9]]);
  assert.deepEqual(joinSpans([[5, 6]], [[1, 2], [3, 4], [9, 8]]), [[1, 6]], "touching spans join, an empty one is dropped");
});
