// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's typing and deleting: brackets and quotes paired and stepped over, a new line as deep as the one it
// leaves, and deleting a letter, a word, an indent's step, a pair, or the line break.

import { given, marked, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("an opening bracket types its pair, its closer is stepped over, and a selection is held between them", async () => {
  const view = await given("x = |");
  view.type("(");
  assert.equal(marked(), "x = (|)");
  view.type(")");
  assert.equal(marked(), "x = ()|");
  await given("[word]");
  view.type("(");
  assert.equal(marked(), "([word])");
  await given("it|");
  view.type("'");
  assert.equal(marked(), "it'|", "a quote after a word's letter is not paired");
});

test("a new line is as deep as the line it leaves, and a step deeper after a block opens", async () => {
  const view = await given("def f():|");
  view.newline();
  assert.equal(marked(), "def f():\n    |");
  await given("    x = 1|");
  view.newline();
  assert.equal(marked(), "    x = 1\n    |");
});

test("Backspace takes a letter, a whole step of indent, an empty pair, a word, or the line break above", async () => {
  const view = await given("ab|c");
  view.backspace();
  assert.equal(marked(), "a|c");
  await given("        |x");
  view.backspace();
  assert.equal(marked(), "    |x");
  await given("(|)");
  view.backspace();
  assert.equal(marked(), "|");
  await given("one two|");
  view.backspace(true);
  assert.equal(marked(), "one |");
  await given("a\n|b");
  view.backspace();
  assert.equal(marked(), "a|b");
});

test("Delete takes the letter after, a word, or the line break below, and a selection whole", async () => {
  const view = await given("|ab");
  view.deleteForward();
  assert.equal(marked(), "|b");
  await given("|one two");
  view.deleteForward(true);
  assert.equal(marked(), "| two");
  await given("a|\nb");
  view.deleteForward();
  assert.equal(marked(), "a|b");
  await given("x[yz]w");
  view.deleteForward();
  assert.equal(marked(), "x|w");
});

test("the lines the selections cover, as runs of neighbors, a selection ending at a line's start leaving it out", async () => {
  const view = await given("[a\nb\n]c\nd\n|e");
  assert.deepEqual(view.linesOf(), [0, 1, 4]);
  assert.deepEqual(view.blocks(), [[0, 1], [4, 4]]);
});

test("the window is put back as the file found it", putBackAsFound);
