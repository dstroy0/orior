// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's motions and text objects in the editor: by word, by line, to a character, to a bracket's pair, by paragraph, and
// the objects an operator takes.

import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

// The given text in the editor with Vim's keys on.
async function vimOn(marked) {
  const view = await given(marked);
  view.setVim(true);
  return view;
}

// Each key as Vim takes it: a string's characters one by one, or a list's keys as named.
const keys = (view, typed) => (Array.isArray(typed) ? typed : [...typed]).forEach((key) => view.vim.take(key));
const at = (view) => [view.primary().head.line, view.primary().head.col];

test("w, b and e step by word, a count taking as many steps, and 0, ^ and $ go along the line", async () => {
  const view = await vimOn("|one two three\n  four five");
  keys(view, "w");
  assert.deepEqual(at(view), [0, 4]);
  keys(view, "2w");
  assert.deepEqual(at(view), [1, 2], "on to the next line's first word");
  keys(view, "b");
  assert.deepEqual(at(view), [0, 8]);
  keys(view, "e");
  assert.deepEqual(at(view), [0, 12]);
  keys(view, "0");
  assert.deepEqual(at(view), [0, 0]);
  keys(view, "j^");
  assert.deepEqual(at(view), [1, 2]);
  keys(view, "$");
  assert.deepEqual(at(view), [1, 10]);
  view.setVim(false);
});

test("gg and G go to the first and last line, f and t to a character on the line, and ; again", async () => {
  const view = await vimOn("a line\n|banana bread\nlast");
  keys(view, "fa");
  assert.deepEqual(at(view), [1, 1]);
  keys(view, ";");
  assert.deepEqual(at(view), [1, 3]);
  keys(view, "tb");
  assert.deepEqual(at(view), [1, 6], "before the character");
  keys(view, "G");
  assert.deepEqual(at(view), [2, 0]);
  keys(view, "gg");
  assert.deepEqual(at(view), [0, 0]);
  view.setVim(false);
});

test("% goes to the bracket that pairs with the next on the line, and } and { to the empty line after and before", async () => {
  const view = await vimOn("|f(a, (b))\none\n\ntwo\n\nthree");
  keys(view, "%");
  assert.deepEqual(at(view), [0, 8]);
  keys(view, "%");
  assert.deepEqual(at(view), [0, 1]);
  keys(view, "}");
  assert.deepEqual(at(view), [2, 0]);
  keys(view, "}");
  assert.deepEqual(at(view), [4, 0]);
  keys(view, "{");
  assert.deepEqual(at(view), [2, 0]);
  view.setVim(false);
});

test("a text object: a word inside, a word with its space, and what a bracket or a quote holds", async () => {
  const view = await vimOn("one t|wo three");
  keys(view, "diw");
  assert.equal(view.doc.text(), "one  three");
  await given("one t|wo three");
  keys(view, "daw");
  assert.equal(view.doc.text(), "one three");
  await given("call(a|, b) + 1");
  keys(view, "di(");
  assert.equal(view.doc.text(), "call() + 1");
  await given('say "hel|lo there" now');
  keys(view, 'da"');
  assert.equal(view.doc.text(), "say now", "the space after the quote taken with it");
  await given('say "hel|lo"');
  keys(view, 'da"');
  assert.equal(view.doc.text(), "say", "the space before, where none comes after");
  await given('say "hel|lo there" now');
  keys(view, 'di"');
  assert.equal(view.doc.text(), 'say "" now');
  view.setVim(false);
});

test("the window is put back as the file found it", putBackAsFound);
