// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's tabs and files: a zip in the tree opened as a folder, its files read, changed and saved
// back into it, and a large text stored on the window's side, its changes following it there and its
// gutter's marks worked out there, the tabs opened again as the tree starts, and a tab back at its
// saved text, however it got back, marked saved; and the changes not saved retained as a window closes.

import { editing, openFile, setVimKeys, vimKeys } from "../../gui/src-js/edit.js";
import { invoke } from "../../gui/src-js/bridge.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("a tab back at its saved text shows no change, by a key taken back, an input method's character or Vim's undo", async () => {
  await invoke("file_write", { path: "saved.txt", text: "first line\nsecond line\n" });
  showView("edit");
  await openFile("saved.txt");
  await ui.waitFor(() => editing().active === "saved.txt" && editing().editor?.s, 5000);
  const view = editing().editor;
  const changed = () => editing().activeChanged;
  view.goTo(0, 5);
  view.focus();
  await ui.type("x");
  assert.ok(changed(), "a character typed is a change");
  await ui.key("Backspace");
  assert.ok(!changed(), "taken back, none");
  // An input method's character, composed and then taken back.
  const input = view.input;
  input.dispatchEvent(new CompositionEvent("compositionstart", { data: "" }));
  input.value = "e";
  input.dispatchEvent(new InputEvent("input", { data: "e", isComposing: true }));
  input.dispatchEvent(new CompositionEvent("compositionend", { data: "é" }));
  assert.equal(view.doc.line(0), "firstx line".replace("x", "é"), "the character written");
  assert.ok(changed(), "a change");
  await ui.key("Backspace");
  assert.ok(!changed(), "the character taken back, none");
  input.dispatchEvent(new CompositionEvent("compositionstart", { data: "" }));
  input.dispatchEvent(new CompositionEvent("compositionend", { data: "" }));
  assert.ok(!changed(), "a composition left with nothing writes nothing");
  // Vim's undo.
  const vim = vimKeys();
  try {
    setVimKeys(true);
    view.goTo(1, 0);
    view.focus();
    await ui.key("Escape");
    await ui.key("x");
    assert.equal(view.doc.line(1), "econd line");
    assert.ok(changed(), "Vim's x is a change");
    await ui.key("u");
    assert.equal(view.doc.line(1), "second line");
    assert.ok(!changed(), "Vim's undo back to the saved text, none");
  } finally {
    setVimKeys(vim);
  }
});
