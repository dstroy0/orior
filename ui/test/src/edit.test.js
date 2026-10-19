// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's tabs and files: a zip in the tree opened as a folder, its files read, changed and saved
// back into it, and a large text held on the Rust side, its changes following it there and its
// gutter's marks worked out there, the tabs opened again as the tree starts, and a tab back at its
// saved text, however it got back, marked saved; and the changes not saved kept as a window closes.

import { invoke } from "../../gui/src-js/bridge.js";
import { chooseNames, editing, openFile, restoreSession, setVimKeys, treeRowShown, vimKeys } from "../../gui/src-js/edit.js";
import { lineChanges } from "../../gui/src-js/editor/diff.js";
import { togglePane } from "../../gui/src-js/sides.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const rowOf = (path) => treeRowShown(path);

test("a zip in the tree opens as a folder, and a file in it opens, is changed and saved back into it", async () => {
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await ui.waitFor(() => rowOf("bundle.zip")?.checkVisibility(), 10000);
  const zip = rowOf("bundle.zip");
  assert.equal(zip.dataset.archive, "true");
  assert.ok(zip.classList.contains("dir"), "it opens as a folder");
  assert.ok(zip.querySelector(".file-icon, svg, img, .icon"), "with its file's icon");
  await ui.click(zip);
  await ui.waitFor(() => rowOf("bundle.zip/docs"), 5000);
  await ui.click(rowOf("bundle.zip/docs"));
  await ui.waitFor(() => rowOf("bundle.zip/docs/read.md"), 5000);
  await ui.click(rowOf("bundle.zip/docs/read.md"));
  await ui.waitFor(() => editing().active === "bundle.zip/docs/read.md", 5000);
  assert.match(editing().editor.s.doc.line(2), /hello from the bundle/);
  editing().editor.focus();
  await ui.key("Ctrl+End");
  await ui.type("changed in orior");
  await ui.key("Ctrl+S");
  await ui.waitFor(async () => /changed in orior/.test((await invoke("file_read", { path: "bundle.zip/docs/read.md" })).text ?? ""), 5000);
  const opened = await invoke("file_read", { path: "bundle.zip/docs/read.md" });
  assert.match(opened.text, /^# Inside a zip\n\nhello from the bundle\nchanged in orior/);
});

test("names the zip does not say are UTF-8 read in the encoding the reader chooses", async () => {
  const nameOf = () => [...document.querySelectorAll('#files .node[data-key^="bundle.zip/"] .name')].map((one) => one.textContent);
  assert.ok(nameOf().includes("ô·û{.txt"), `read in the DOS page as the zip says: ${nameOf()}`);
  await chooseNames("bundle.zip", "shift_jis");
  try {
    await ui.waitFor(() => nameOf().includes("日本.txt"), 3000);
  } finally {
    await chooseNames("bundle.zip", "");
  }
  await ui.click(rowOf("bundle.zip"));
  await ui.waitFor(() => !rowOf("bundle.zip/docs"), 3000);
  togglePane(false);
});

test("a large text is held on the Rust side, each change follows it there, and its gutter's marks come from there as the page's own diff gives them", async () => {
  const lines = Array.from({ length: 20000 }, (_, line) => `value_${line} = compute(${line}) + ${line % 7}`);
  const root = await invoke("root_get");
  await invoke("folder_create", { path: "held" }).catch(() => {});
  await invoke("repo_init", { folder: `${root}/held` });
  await invoke("file_write", { path: "held/held.py", text: `${lines.join("\n")}\n` });
  await invoke("git_commit", { message: "a large file held", paths: ["held/held.py"] });
  showView("edit");
  await openFile("held/held.py");
  await ui.waitFor(() => editing().active === "held/held.py" && editing().editor?.s?.doc.held?.ready, 10000);
  const view = editing().editor;
  const s = view.s;
  const held = s.doc.held;
  view.goTo(5000, 0);
  view.focus();
  await ui.type("changed_");
  view.goTo(12000, 0);
  await ui.key("Shift+Down");
  await ui.key("Delete");
  view.goTo(15000, 0);
  await ui.type("put in\n");
  // Each change has gone to the Rust side, once the calls about the text are answered, and the text
  // there is the page's, line for line and unit for unit.
  await new Promise((rest) => setTimeout(rest, 50));
  await held.chain;
  const shape = await invoke("document_edit", { key: held.key, changes: [] });
  assert.deepEqual(shape, { lines: s.doc.count, units: s.doc.lines.reduce((sum, line) => sum + line.length, 0) });
  const head = (await invoke("file_head", { path: "held/held.py" })).split(/\r?\n/);
  const own = lineChanges(head, s.doc.lines);
  await ui.waitFor(() => s.changes && s.changes.hunks.length === own.hunks.length, 5000);
  assert.deepEqual([...s.changes.changed].sort((a, b) => a - b), [...own.changed].sort((a, b) => a - b), "changed");
  assert.deepEqual([...s.changes.added].sort((a, b) => a - b), [...own.added].sort((a, b) => a - b), "added");
  assert.deepEqual([...s.changes.removed].sort((a, b) => a - b), [...own.removed].sort((a, b) => a - b), "taken out");
  assert.deepEqual(s.changes.hunks, own.hunks);
  for (const hunk of own.hunks) {
    assert.deepEqual(s.changesThen.slice(hunk.then[0], hunk.then[1]), head.slice(hunk.then[0], hunk.then[1]), "the lines a change spans as the commit holds them");
  }
  // Closed, its tab's text is let go on the Rust side. The tab strip is drawn again as the marks
  // come: each press is on the close button as it stands.
  const closer = () => document.querySelector(`#tabs .tab .close[title="held/held.py"]`);
  for (let press = 0; press < 2 && closer(); press += 1) {
    await ui.waitFor(() => closer()?.checkVisibility(), 3000);
    await ui.click(closer());
  }
  await ui.waitFor(() => !document.querySelector(`#tabs .tab .close[title="held/held.py"]`), 3000);
  let gone = false;
  for (let tries = 0; tries < 30 && !gone; tries += 1) {
    gone = await invoke("document_edit", { key: held.key, changes: [] }).then(() => false, () => true);
    if (!gone) {
      await new Promise((rest) => setTimeout(rest, 100));
    }
  }
  assert.ok(gone, "no text is held under its key");
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
    assert.equal(document.activeElement, field, "the field keeps the keys");
    field.blur();
    await restoreSession();
    assert.ok(editorNode.contains(document.activeElement), "the editor takes the keys nothing has");
  } finally {
    field.remove();
  }
});

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
  assert.ok(changed(), "a letter typed is a change");
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

test("the changes not saved are kept at once as the window closes, and let go once the text is back as saved", async () => {
  await invoke("file_write", { path: "closing.txt", text: "kept as typed\n" });
  showView("edit");
  await openFile("closing.txt");
  await ui.waitFor(() => editing().active === "closing.txt" && editing().editor?.s, 5000);
  const view = editing().editor;
  const backup = () => Object.keys(localStorage).filter((key) => key.startsWith("orior.backup.") && key.endsWith("\nclosing.txt")).map((key) => localStorage.getItem(key))[0] ?? null;
  view.goTo(0, 0);
  view.focus();
  await ui.type("Q");
  window.dispatchEvent(new Event("pagehide"));
  assert.equal(backup(), "Qkept as typed\n", "kept the moment the window closes, not a moment after typing rests");
  await ui.key("Backspace");
  window.dispatchEvent(new Event("pagehide"));
  assert.equal(backup(), null, "back as saved, nothing kept");
});
