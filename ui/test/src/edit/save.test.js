// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Saving and formatting a tab's file, and running, validating and verifying it.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { calls, GIVEN, given, made, press, putBackAsFound, ran, told, TREE } from "../../menus.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("File, Save and Save All write the files changed", async () => {
  const view = await given("saved by Save|");
  await press("save");
  assert.equal((await invoke("file_read", { path: GIVEN })).text, "saved by Save");
  await openFile("hello.py");
  const hello = editing().editor;
  hello.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "# saved by Save All\n" }], null);
  await given("and this|");
  await press("save-all");
  assert.equal((await invoke("file_read", { path: GIVEN })).text, "and this");
  assert.match((await invoke("file_read", { path: "hello.py" })).text, /^# saved by Save All\n/);
  await openFile("hello.py");
  editing().editor.change([{ from: { line: 0, col: 0 }, to: { line: 1, col: 0 }, text: "" }], null);
  await press("save");
  assert.ok(view);
});

test("Edit, Format Document formats the file with its language's formatter", async () => {
  // The command line's own format takes the files to format; the menu's formats the tab shown.
  assert.match(await ran("format"), /exit (code|status): 2/, "the command line asks for the files");
  calls.answer("format_languages", () => ["python"]);
  calls.answer("format_text", () => "x = 1\n");
  await given("x=1\n|");
  await runCommand("format");
  await ui.waitFor(() => editing().editor.doc.text() === "x = 1\n", 5000, "the text formatted");
});

test("Run, Run File runs the file shown, from the window and from the command line", async () => {
  assert.equal(await ran("run-file", `${TREE()}/hello.py`), "ran", "Python ran hello.py");
  showView("edit");
  await openFile("hello.py");
  const before = made("run_file_line").length;
  await runCommand("run-file");
  await ui.waitFor(() => made("run_file_line").length > before, 5000, "the file's run asked for");
  await ui.waitFor(() => /hello, orior/.test(document.getElementById("term-views").textContent), 15000, "its output in the terminal");
}, { timeout: 60000 });

test("Run, Validate says where no tool plugin validates the file, from the window and the command line", async () => {
  assert.match(await ran("validate", `${TREE()}/hello.py`), /ended/, "the command line refuses a file no tool validates");
  showView("edit");
  await openFile("hello.py");
  await runCommand("validate");
  assert.ok(told(/No tool plugin validates Python/));
});

test("Run, Verify Disassembly reads a C module alone from the window, and the command line names the toolchain it lacks", async () => {
  // the command line's loop asks for Ghidra and the compiler first, and says which it lacks on a machine without them
  assert.match(await ran("disasm-verify", `${TREE()}/hello.py`), /ended|Ghidra|clang/, "the command line's answer names what stops the loop");
  showView("edit");
  await openFile("hello.py");
  await runCommand("disasm-verify");
  assert.ok(told(/Verify Disassembly reads a C module/), "a file that is no C module is said to be none");
});

test("the window is put back as the file found it", putBackAsFound);
