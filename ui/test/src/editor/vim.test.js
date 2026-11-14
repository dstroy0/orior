// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's keys in the editor: its modes and what the status line shows of them, which keys it takes and which it leaves
// the editor, the caret as a block, and Escape dropping what was typed so far.

import { given, putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

const event = (key, modifiers = {}) => {
  let prevented = false;
  return { key, ctrlKey: false, altKey: false, metaKey: false, shiftKey: false, ...modifiers, preventDefault: () => (prevented = true), prevented: () => prevented };
};

test("Vim starts in normal mode, its caret a block, and the status line shows the mode and the keys typed so far", async () => {
  const view = await given("|text");
  view.setVim(true);
  try {
    assert.equal(view.vim.mode, "normal");
    assert.equal(view.vim.block(), true);
    assert.equal(view.vim.label(), "NORMAL");
    view.vim.take("2");
    view.vim.take("d");
    assert.equal(view.vim.label(), "NORMAL 2d");
    view.vim.take("Esc");
    assert.equal(view.vim.label(), "NORMAL", "Escape drops the keys typed");
    view.vim.take("i");
    assert.deepEqual([view.vim.mode, view.vim.label(), view.vim.block()], ["insert", "INSERT", false]);
    assert.equal(view.host.dataset.vim, "insert");
  } finally {
    view.setVim(false);
  }
});

test("in insert mode Vim takes Escape alone; in normal mode it takes characters and leaves the editor the keys Vim gives nothing to", async () => {
  const view = await given("|text");
  view.setVim(true);
  try {
    const save = event("s", { ctrlKey: true });
    assert.equal(view.vim.key(save), false, "Ctrl+S is the editor's");
    const character = event("l");
    assert.equal(view.vim.key(character), true);
    assert.ok(character.prevented());
    assert.equal(view.vim.key(event("ArrowLeft")), true, "an arrow is a motion");
    view.vim.take("i");
    assert.equal(view.vim.key(event("a")), false, "typing in insert mode is the editor's");
    const escape = event("Escape");
    assert.equal(view.vim.key(escape), true);
    assert.equal(view.vim.mode, "normal");
  } finally {
    view.setVim(false);
  }
  assert.equal(view.host.dataset.vim, undefined, "turned off, the editor holds no mode");
});

test("the window is put back as the file found it", putBackAsFound);
