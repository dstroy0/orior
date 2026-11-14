// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Places: Back and Forward through where the cursor was, and what each tree retains.

import { restoreSession } from "../../../gui/src-js/edit/places.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { GIVEN, given, press, putBackAsFound } from "../../menus.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("Go, Go to File opens the file at its line and column, and Back and Forward step through where the cursor was", async () => {
  await given("a|b\ncd");
  await press("file", "hello.py:2:5");
  assert.equal(editing().active, "hello.py");
  assert.deepEqual(editing().editor.head(), { line: 1, col: 4 });
  await press("back");
  await ui.waitFor(() => editing().active === GIVEN, 3000, "back to the given file");
  await press("forward");
  await ui.waitFor(() => editing().active === "hello.py", 3000, "forward to hello.py");
  await press("last-editor");
  await ui.waitFor(() => editing().active === GIVEN, 3000, "the editor used before");
});

test("the tabs opened again as the tree starts leave the keys in the field the reader is typing in, and take them where nothing has them", async () => {
  showView("edit");
  await openFile("README.md");
  await ui.waitFor(() => editing().active === "README.md", 5000);
  const editorNode = document.getElementById("editor");
  const field = document.createElement("input");
  document.body.append(field);
  try {
    field.focus();
    await restoreSession();
    assert.equal(document.activeElement, field, "the field retains the keys");
    field.blur();
    await restoreSession();
    assert.ok(editorNode.contains(document.activeElement), "the editor takes the keys nothing has");
  } finally {
    field.remove();
  }
});

test("the changes not saved are retained at once as the window closes, and let go once the text is back as saved", async () => {
  await invoke("file_write", { path: "closing.txt", text: "retained as typed\n" });
  showView("edit");
  await openFile("closing.txt");
  await ui.waitFor(() => editing().active === "closing.txt" && editing().editor?.s, 5000);
  const view = editing().editor;
  const backup = () => Object.keys(localStorage).filter((key) => key.startsWith("orior.backup.") && key.endsWith("\nclosing.txt")).map((key) => localStorage.getItem(key))[0] ?? null;
  view.goTo(0, 0);
  view.focus();
  await ui.type("Q");
  window.dispatchEvent(new Event("pagehide"));
  assert.equal(backup(), "Qkept as typed\n", "retained the moment the window closes, not a moment after typing rests");
  await ui.key("Backspace");
  window.dispatchEvent(new Event("pagehide"));
  assert.equal(backup(), null, "back as saved, nothing retained");
});

test("the window is put back as the file found it", putBackAsFound);
