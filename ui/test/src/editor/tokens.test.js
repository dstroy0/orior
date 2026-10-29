// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tokens of a text's lines as orior's side hands them: each line's runs, a column then a name for each, drawn in its
// names' classes, colors worked out before a change put where their lines moved, those the change wrote left out, a line
// a change wrote showing the runs of the line it began on until its own come, a change of many lines taken whole, a
// text read a window at a time placing colors by the file's lines, the runs of the lines near the view alone kept, and a
// print's colors read in their classes.

import { classed, Highlight, partsOf } from "../../../gui/src-js/editor/tokens.js";

const { test, assert } = window.__harness;

const NAMES = ["", "comment", "identifier"];

const docOf = (lines) => ({ lines, get count() { return this.lines.length; }, line(at) { return this.lines[at]; } });

test("a mirror's lines show the runs orior's side hands, each name drawn in its classes", () => {
  const tokens = new Highlight(docOf(["/* a", "b"]), null);
  tokens.mirrors("tokens#runs", NAMES, [{ from: 0, runs: [[0, 1], [0, 2]] }]);
  try {
    assert.equal(tokens.classAt(0, 2), "t-comment");
    assert.equal(tokens.classAt(1, 0), "t-identifier");
    assert.deepEqual(tokens.runsOf(1), [[0, "t-identifier"]], "a line's runs read in their classes");
    assert.equal(tokens.cached(2), null, "a line whose runs have not come has none");
  } finally {
    tokens.letGo();
  }
});

test("colors worked out before a change land where their lines moved, a line it wrote left out", () => {
  const tokens = new Highlight(docOf(["a", "b", "x", "y", "z"]), null);
  tokens.mirrors("tokens#moved", NAMES, []);
  try {
    tokens.changed([{ from: { line: 0, col: 0 }, to: { line: 0, col: 1 }, text: "a\nb" }]);
    tokens.put(0, [{ from: 0, runs: [[0, 1], [0, 2], [0, 1]] }]);
    assert.equal(tokens.cached(0), null, "the line the change wrote takes none of the colors worked out before it");
    assert.equal(tokens.classAt(2, 0), "t-identifier", "a line below the change moves down with it");
    assert.equal(tokens.classAt(3, 0), "t-comment");
    tokens.put(1, [{ from: 0, runs: [[0, 2], [0, 2]] }]);
    assert.equal(tokens.classAt(0, 0), "t-identifier", "and colors worked out after it land as they come");
  } finally {
    tokens.letGo();
  }
});

test("a line a change wrote shows the runs of the line it began on until its own come", () => {
  const tokens = new Highlight(docOf(["/* a", "b"]), null);
  tokens.mirrors("tokens#carried", NAMES, [{ from: 0, runs: [[0, 1], [0, 2]] }]);
  try {
    tokens.changed([{ from: { line: 0, col: 4 }, to: { line: 0, col: 4 }, text: "c\nd" }]);
    assert.equal(tokens.classAt(0, 0), "t-comment", "the line the change began on keeps its runs");
    assert.equal(tokens.cached(1), null, "a line it added has none yet");
    assert.equal(tokens.classAt(2, 0), "t-identifier", "and the line after moves down with its own");
  } finally {
    tokens.letGo();
  }
});

test("a change of many lines is taken whole, the lines after it moved down past it", () => {
  const tokens = new Highlight(docOf(["a", "b"]), null);
  tokens.mirrors("tokens#many", NAMES, [{ from: 0, runs: [[0, 1], [0, 2]] }]);
  try {
    const count = 300000;
    tokens.changed([{ from: { line: 0, col: 1 }, to: { line: 0, col: 1 }, text: "\n".repeat(count - 1) }]);
    assert.equal(tokens.classAt(count, 0), "t-identifier", "the line after the change stands past every line it wrote");
    assert.equal(tokens.cached(1), null, "and a line it wrote has none yet");
  } finally {
    tokens.letGo();
  }
});

test("a text read a window at a time places colors by the file's lines, those outside the window left out", () => {
  const tokens = new Highlight(docOf(["a", "b", "c", "d"]), null, null, () => 100);
  tokens.mirrors("tokens#window", NAMES, [{ from: 99, runs: [[0, 1], [0, 2], [0, 1]] }]);
  try {
    assert.equal(tokens.cached(-1), null, "a line above the window is left out");
    assert.equal(tokens.classAt(0, 0), "t-identifier", "the file's line 100 is the window's first");
    assert.equal(tokens.classAt(1, 0), "t-comment");
    tokens.changed([{ from: { line: 0, col: 1 }, to: { line: 0, col: 1 }, text: "\n" }]);
    assert.equal(tokens.changes[0].edits[0].from.line, 100, "a change is counted from the file's first line");
    tokens.put(0, [{ from: 102, runs: [[0, 2]] }]);
    assert.equal(tokens.classAt(3, 0), "t-identifier", "colors worked out before it move down past the line it wrote");
  } finally {
    tokens.letGo();
  }
});

test("the page keeps the runs of the lines near its view alone, letting go of the rest as it asks for those it lacks", () => {
  const tokens = new Highlight(docOf(Array.from({ length: 1000 }, () => "a")), null);
  const colored = Array.from({ length: 1000 }, () => [0, 2]);
  tokens.mirrors("tokens#near", NAMES, [{ from: 0, runs: colored }]);
  try {
    tokens.shown(500, 509);
    assert.equal(tokens.cached(0) !== null && tokens.cached(999) !== null, true, "a view whose lines and those near it have runs asks for nothing");
    tokens.runs[505] = undefined;
    tokens.shown(500, 509);
    assert.equal(tokens.cached(0), null, "a line far above the view let go");
    assert.equal(tokens.cached(999), null, "and one far below");
    assert.equal(tokens.classAt(480, 0), "t-identifier", "a line two views' height above it kept");
    assert.equal(tokens.classAt(529, 0), "t-identifier", "and one two views' height below");
    assert.equal(tokens.cached(530), null, "past those, let go");
  } finally {
    tokens.letGo();
  }
});

test("a print's colors, asked of orior's side, are read in their classes by the line the page counts", () => {
  const tokens = new Highlight(docOf(["a", "b"]), null, null, () => 10);
  tokens.mirrors("tokens#print", NAMES, []);
  try {
    const given = tokens.classedFrom([{ from: 10, runs: [[0, 1], [0, 2]] }]);
    assert.deepEqual([given.get(0), given.get(1)], [[[0, "t-comment"]], [[0, "t-identifier"]]]);
  } finally {
    tokens.letGo();
  }
});

test("a line's runs as they came are read in their names' classes, and its text cut at them, a run that takes no letter left out", () => {
  assert.deepEqual(classed([0, 1, 4, 2], ["", "t-a", "t-b"]), [[0, "t-a"], [4, "t-b"]]);
  assert.deepEqual(partsOf("int x;", [[0, "t-keyword"], [3, ""], [3, ""], [4, "t-identifier"], [5, ""]]), [["t-keyword", "int"], ["", " "], ["t-identifier", "x"], ["", ";"]]);
  assert.deepEqual(partsOf("", [[0, ""]]), [], "an empty line has no parts");
});

test("a text with no grammar is one uncolored run a line", () => {
  const tokens = new Highlight(docOf(["anything"]), null);
  assert.deepEqual(tokens.runsOf(0), [[0, ""]]);
});
