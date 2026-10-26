// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The language server's intel: rename, quick fixes, parameter info and quick documentation.

import { invoke } from "../../gui/src-js/bridge.js";
import { closeMenu } from "../../gui/src-js/menu.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { showView } from "../../gui/src-js/views.js";
import { asFound, press, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("Edit, Rename, Quick Fix, Parameter Info and Quick Documentation ask the language server, here clangd", async () => {
  await invoke("file_write", { path: "served.c", text: "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)\n}\n" });
  showView("edit");
  await openFile("served.c");
  await ui.waitFor(() => editing().active === "served.c" && editing().editor?.s, 5000, "served.c open");
  const view = editing().editor;
  await ui.waitFor(async () => {
    view.goTo(3, 16);
    await press("parameter-info");
    return document.querySelector(".ed-signature:not([hidden])")?.textContent.includes("add");
  }, 30000, "clangd to give the signature");
  view.goTo(0, 5);
  await press("quick-doc");
  await ui.waitFor(() => !view.hover.el.hidden && /add/.test(view.hover.el.textContent), 10000, "the documentation of add");
  view.hover.el.hidden = true;
  view.goTo(0, 5);
  await press("rename");
  const field = await ui.waitFor(() => document.querySelector(".ed-rename"), 10000, "the rename field");
  assert.equal(field.value, "add");
  field.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));
  // a + b selected in add: clangd offers to put it in a variable of its own.
  await ui.waitFor(async () => {
    view.select([{ anchor: { line: 0, col: 31 }, head: { line: 0, col: 36 }, goal: null }]);
    await press("quick-fix");
    return document.querySelector("body > .menu");
  }, 15000, "clangd's refactorings of the selection");
  assert.match(document.querySelector("body > .menu").textContent, /xtract/, "an extraction offered");
  closeMenu(false);
}, { timeout: 90000 });

test("the quick fixes' list, from the command line: the fix of that title applied, here clangd's", async () => {
  asFound();
  const text = "int add(int a, int b) { return a + b; }\n";
  await invoke("file_write", { path: "served.c", text });
  showView("edit");
  await openFile("served.c");
  await ui.waitFor(() => editing().active === "served.c" && editing().editor?.s, 5000, "served.c open");
  const view = editing().editor;
  // a + b selected in add: clangd offers to put it in a variable of its own.
  const title = await ui.waitFor(async () => {
    view.select([{ anchor: { line: 0, col: 31 }, head: { line: 0, col: 36 }, goal: null }]);
    await press("quick-fix");
    const found = [...document.querySelectorAll("body > .menu > .menu-item")].map((button) => button.item).find((item) => /xtract/.test(item?.label ?? ""));
    closeMenu(false);
    return found?.command === "quick-fix-apply" ? found.args[0] : null;
  }, 30000, "clangd's extraction offered");
  await press("quick-fix-apply", ...title.split(" "));
  await ui.waitFor(() => editing().editor.doc.text() !== text && /return \w+;/.test(editing().editor.doc.text()), 10000, "a + b put in a variable of its own").catch((error) => {
    throw new Error(`${error.message}; the text: ${JSON.stringify(editing().editor.doc.text())}`);
  });
}, { timeout: 90000 });

test("the window is put back as the file found it", putBackAsFound);
