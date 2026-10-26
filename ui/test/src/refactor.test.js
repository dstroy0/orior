// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The refactorings the window makes itself in the file open: Extract Variable, Extract Constant and Inline Variable,
// each one change; the line a statement starts on; and the type the C family infers.

import { extractConstant, extractVariable, inferred, inlineVariable, statementStart } from "../../gui/src-js/refactor.js";
import { GIVEN, given, putBackAsFound } from "../menus.js";

const { test, assert } = window.__harness;

const docOf = (text) => {
  const lines = text.split("\n");
  return { count: lines.length, line: (at) => lines[at] };
};

test("a statement starts above each line that runs on to the next, and below a line that opens a block", () => {
  const doc = docOf("if ready:\n    total = (1 +\n        2 +\n        3)\nx = [\n    4,\n]");
  assert.equal(statementStart(doc, 3), 1);
  assert.equal(statementStart(doc, 1), 1, "a colon that ends a line opens a block");
  assert.equal(statementStart(doc, 5), 4);
  assert.equal(statementStart(doc, 6), 4);
  assert.equal(statementStart(docOf("y = items()\n    .first()"), 1), 0, "a line that starts with a dot runs on from above");
});

test("the C family infers a value's type with auto, and C with GNU C's __auto_type", () => {
  assert.equal(inferred("src/main.c"), "__auto_type");
  assert.equal(inferred("src/main.h"), "__auto_type");
  assert.equal(inferred("src/main.cpp"), "auto");
  assert.equal(inferred(null), "auto");
});

test("Extract Variable puts the expression into a variable above its statement, a number after a name the file uses", async () => {
  const view = await given("def f():\n    return [a + b] * 2\n");
  extractVariable(view, GIVEN);
  assert.equal(view.doc.text(), "def f():\n    value = a + b\n    return value * 2\n");
  await given("def f(value):\n    return [a + b] * value\n");
  extractVariable(view, GIVEN);
  assert.equal(view.doc.text(), "def f(value):\n    value2 = a + b\n    return value2 * value\n");
});

test("Extract Constant puts the value into a constant at the head of the file", async () => {
  const view = await given("x = [3]\ny = 4");
  extractConstant(view);
  assert.equal(view.doc.text(), "VALUE = 3\n\nx = VALUE\ny = 4");
});

test("Inline Variable puts the value where the name is used, in parentheses where it is more than one term, and takes the declaration out", async () => {
  const view = await given("a = 1\nb = |a + 2");
  inlineVariable(view);
  assert.equal(view.doc.text(), "b = 1 + 2");
  await given("a = x + 1\nb = |a * 2");
  inlineVariable(view);
  assert.equal(view.doc.text(), "b = (x + 1) * 2");
});

test("the window is put back as the file found it", putBackAsFound);
