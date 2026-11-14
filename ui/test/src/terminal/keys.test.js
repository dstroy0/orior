// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a key sends a shell, as an xterm sends it.

import { keyText } from "../../../gui/src-js/terminal/keys.js";

const { test, assert } = window.__harness;

const key = (name, modifiers = {}) => ({ key: name, shiftKey: false, altKey: false, ctrlKey: false, metaKey: false, ...modifiers });

test("a character sends itself, and with Alt it is led by Escape", () => {
  assert.equal(keyText(key("a"), false), "a");
  assert.equal(keyText(key("é"), false), "é");
  assert.equal(keyText(key("a", { altKey: true }), false), "\x1ba");
  assert.equal(keyText(key("a", { metaKey: true }), false), null, "the system's own key is left alone");
});

test("Ctrl and a character send its control code, and Ctrl and space, slash and question mark theirs", () => {
  assert.equal(keyText(key("c", { ctrlKey: true }), false), "\x03");
  assert.equal(keyText(key("[", { ctrlKey: true }), false), "\x1b");
  assert.equal(keyText(key(" ", { ctrlKey: true }), false), "\x00");
  assert.equal(keyText(key("/", { ctrlKey: true }), false), "\x1f");
  assert.equal(keyText(key("1", { ctrlKey: true }), false), null);
});

test("the arrows send the cursor's codes, the application's form where it asks, and with a key pressed the retained form", () => {
  assert.equal(keyText(key("ArrowUp"), false), "\x1b[A");
  assert.equal(keyText(key("ArrowUp"), true), "\x1bOA");
  assert.equal(keyText(key("ArrowLeft", { ctrlKey: true }), false), "\x1b[1;5D");
  assert.equal(keyText(key("End", { shiftKey: true }), true), "\x1b[1;2F");
});

test("the named keys send theirs: Enter, Backspace, Shift+Tab, Ctrl+Backspace, the page keys and the function keys", () => {
  assert.equal(keyText(key("Enter"), false), "\r");
  assert.equal(keyText(key("Backspace"), false), "\x7f");
  assert.equal(keyText(key("Backspace", { ctrlKey: true }), false), "\x08");
  assert.equal(keyText(key("Tab", { shiftKey: true }), false), "\x1b[Z");
  assert.equal(keyText(key("PageDown"), false), "\x1b[6~");
  assert.equal(keyText(key("F5"), false), "\x1b[15~");
  assert.equal(keyText(key("Escape", { altKey: true }), false), "\x1b\x1b");
  assert.equal(keyText(key("Shift"), false), null);
});
