// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's operators and commands in the editor: deleting, changing, yanking and putting, joining, case, replacing,
// undo and redo, the lines opened, indenting, and visual mode.

import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

async function vimOn(marked) {
  const view = await given(marked);
  view.setVim(true);
  return view;
}

const keys = (view, typed) => (Array.isArray(typed) ? typed : [...typed]).forEach((key) => view.vim.take(key));

test("x, D and dd take out a letter, the rest of the line and the line, and u and Ctrl+R take them back and bring them again", async () => {
  const view = await vimOn("|abc def\nnext\nlast");
  keys(view, "2x");
  assert.equal(view.doc.text(), "c def\nnext\nlast");
  keys(view, "lD");
  assert.equal(view.doc.text(), "c\nnext\nlast");
  keys(view, "jdd");
  assert.equal(view.doc.text(), "c\nlast");
  keys(view, "u");
  assert.equal(view.doc.text(), "c\nnext\nlast");
  keys(view, ["C-r"]);
  assert.equal(view.doc.text(), "c\nlast");
  view.setVim(false);
});

test("yy yanks a line and p puts it below, P above, and a register named keeps its own", async () => {
  const view = await vimOn("|one\ntwo");
  keys(view, "yyjp");
  assert.equal(view.doc.text(), "one\ntwo\none");
  keys(view, '"ayykP');
  assert.equal(view.doc.text(), "one\none\ntwo\none");
  keys(view, 'G"ap');
  assert.equal(view.doc.text(), "one\none\ntwo\none\none");
  view.setVim(false);
});

test("J joins lines with a space, ~ turns a letter's case, and r replaces one", async () => {
  const view = await vimOn("|first\n   second");
  keys(view, "J");
  assert.equal(view.doc.text(), "first second");
  keys(view, "0~~");
  assert.equal(view.doc.text(), "FIrst second");
  keys(view, "rX");
  assert.equal(view.doc.text(), "FIXst second");
  view.setVim(false);
});

test("o opens a line below in insert mode, Escape goes back, and . does the change again", async () => {
  const view = await vimOn("|a b c d");
  keys(view, "dw");
  assert.equal(view.doc.text(), "b c d");
  keys(view, ".");
  assert.equal(view.doc.text(), "c d");
  keys(view, "o");
  assert.equal(view.vim.mode, "insert");
  view.type("new");
  view.vim.leaveInsert();
  assert.equal(view.vim.mode, "normal");
  assert.equal(view.doc.text(), "c d\nnew");
  view.setVim(false);
});

test(">> indents a line by a step and << takes it back, and cw changes a word into insert mode", async () => {
  const view = await vimOn("|word rest");
  keys(view, ">>");
  assert.equal(view.doc.text(), `${view.unit()}word rest`);
  keys(view, "<<");
  assert.equal(view.doc.text(), "word rest");
  keys(view, "cw");
  assert.equal(view.vim.mode, "insert");
  assert.equal(view.doc.text(), " rest");
  view.setVim(false);
});

test("visual mode chooses from the cursor as it moves, and an operator acts on what is chosen; V chooses whole lines", async () => {
  const view = await vimOn("|one two\nthree\nfour");
  keys(view, "vee");
  assert.equal(view.vim.mode, "visual");
  keys(view, "d");
  assert.equal(view.doc.text(), "\nthree\nfour");
  assert.equal(view.vim.mode, "normal");
  keys(view, "jVjd");
  assert.equal(view.doc.text(), "");
  view.setVim(false);
});

test("the window is put back as the file found it", putBackAsFound);
