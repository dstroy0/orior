// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The File and Edit menus, each command run as its menu runs it and as a command line runs it, and what it does
// looked for.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { paneOpen } from "../../../gui/src-js/explorer.js";
import { forgetMacro, keptMacros, lastMacro, recording } from "../../../gui/src-js/macros.js";
import { calls, given, made, marked, press, putBackAsFound, ran, sheetShown, TREE } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("File, Open opens a file, the tree's own folder, a workspace's folders and a machine's tree, and asks for a repository", async () => {
  await press("open-file", "hello.py");
  assert.equal(editing().active, "hello.py");
  await press("open-folder", TREE());
  await ui.waitFor(() => made("root_set").length, 5000, "the folder opened");
  await invoke("file_write", { path: "made.code-workspace", text: JSON.stringify({ folders: [{ path: "." }] }) });
  await press("open-workspace", `${TREE()}/made.code-workspace`);
  await ui.waitFor(() => made("workspace_read").length, 5000, "the workspace read");
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args));
  await press("open-remote", "wsl:Ubuntu:/tmp");
  assert.equal(opened.at(-1)?.remote, "wsl:Ubuntu:/tmp", "a window opened on the machine's tree");
  await press("open-repository");
  await sheetShown("form");
});

test("File, Close Editor and Close All Editors close the tabs", async () => {
  await press("save-all");
  await openFile("hello.py");
  await press("close-editor");
  assert.notEqual(editing().active, "hello.py");
  await press("close-all");
  await ui.waitFor(() => !editing().open, 5000, "every tab closed");
});

test("Edit, Undo History shows the text's history", async () => {
  await given("a|");
  await press("undo-history");
  assert.ok(paneOpen("undo"));
});

test("Edit, Cut, Copy and Paste take the selection and put the clipboard's text", async () => {
  const clipped = [];
  calls.answer("clip_write", ({ text }) => clipped.push(text));
  await given("[ab]c");
  await press("copy");
  assert.equal(editing().editor.clip?.text, "ab", "copied");
  await given("[ab]c");
  await press("cut");
  assert.equal(editing().editor.doc.text(), "c", "cut");
  assert.deepEqual(clipped, ["ab", "ab"], "each put on the clipboard");
  calls.answer("clip_read", () => "XY");
  await given("c|");
  await press("paste");
  assert.equal(marked(), "cXY|");
  await press("compare-clipboard");
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the file beside the clipboard");
  closeDiff();
});

test("Edit, Macros records keys, plays them back and keeps them by name", async () => {
  const view = await given("|");
  await press("macro-record");
  assert.ok(recording(), "recording");
  view.focus();
  await ui.type("z");
  await press("macro-record");
  assert.ok(!recording() && lastMacro()?.length, "a macro recorded");
  await press("macro-play", "2");
  assert.equal(view.doc.text(), "zzz", "played twice more");
  await press("macro-keep", "made macro");
  try {
    assert.ok(keptMacros().some((one) => one.name === "made macro"));
  } finally {
    forgetMacro("made macro");
  }
});

test("Edit, Find in Files from the command line and from the menu", async () => {
  assert.equal(await ran("search", "greet"), "ran");
  await runCommand("search", ["greet"]);
  assert.ok(paneOpen("search"));
});

test("Edit, Extract Variable, Extract Constant, Inline Variable and Extract Function", async () => {
  await given("def f():\n    return [a + b] * 2\n");
  await press("extract-variable");
  assert.equal(editing().editor.doc.text(), "def f():\n    value = a + b\n    return value * 2\n");
  await given("x = [3]\ny = 4");
  await press("extract-constant");
  assert.equal(editing().editor.doc.text(), "VALUE = 3\n\nx = VALUE\ny = 4");
  await given("a = 1\nb = |a + 2");
  await press("inline-variable");
  assert.equal(editing().editor.doc.text(), "b = 1 + 2");
  await given("def f(a):\n    [b = a * 2\n    c = b + 1]\n    return c\n");
  await press("extract-function");
  await ui.waitFor(() => (editing().editor.doc.text().match(/\bdef /g) ?? []).length === 2, 5000, "a function made of the lines");
});

test("the window is put back as the file found it", putBackAsFound);
