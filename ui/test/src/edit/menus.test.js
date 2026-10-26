// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tree's menus, the tabs' and the editor's, each item from the command line.

import { allCommands, hasCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { toggleTerminal } from "../../../gui/src-js/terminal.js";
import { showView } from "../../../gui/src-js/views.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { showPane } from "../../../gui/src-js/explorer.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { asFound, calls, HOME, itemsUnder, made, menuOver, press, putBackAsFound, rowOf, tabOfName } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("every item of the tree's menus, the tabs' and the editor's runs a command the command line runs", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await ui.waitFor(() => rowOf("hello.py") && rowOf("src") && rowOf("bundle.zip"), 10000, "the tree's rows");
  await openFile("hello.py");
  const named = new Set(allCommands().map((item) => item.command));
  const wrong = [];
  // Each found as its menu opens: the tree draws its rows again as it changes.
  for (const [where, target] of [["a file", () => rowOf("hello.py")], ["a folder", () => rowOf("src")], ["an archive", () => rowOf("bundle.zip")], ["a tab", () => tabOfName("hello.py")], ["the editor", () => editing().editor.host]]) {
    const items = itemsUnder(await menuOver(target));
    assert.ok(items.length, `${where} has a menu`);
    for (const item of items) {
      if (!item.command || !named.has(item.command) || !hasCommand(item.command)) {
        wrong.push(`${where}: ${item.label} runs ${item.command ?? "no command"}`);
      }
    }
  }
  assert.deepEqual(wrong, []);
});

test("the tree's file menu, from the command line: open, copy, cut and paste, the path, the terminal and comparing", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await ui.waitFor(() => rowOf("src"), 10000, "the tree");
  const wasOpen = rowOf("src").getAttribute("aria-expanded") === "true";
  await press("file-open", "src");
  await ui.waitFor(() => (rowOf("src").getAttribute("aria-expanded") === "true") !== wasOpen, 3000, "the folder opened or closed");
  await press("file-open", "src");
  await press("file-open", "README.md");
  assert.equal(editing().active, "README.md");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("file-copy-path", "hello.py");
  assert.deepEqual(copied, ["hello.py"]);
  await press("file-copy", "hello.py");
  await press("file-paste", "src");
  await ui.waitFor(async () => (await invoke("tree_list", { dir: "src" })).some((entry) => /^hello.*\.py$/.test(entry.name)), 5000, "hello.py pasted into src");
  await invoke("file_write", { path: "made_cut.txt", text: "cut\n" });
  await press("file-cut", "made_cut.txt");
  await press("file-paste", "src");
  await ui.waitFor(async () => (await invoke("tree_list", { dir: "src" })).some((entry) => entry.name === "made_cut.txt") && !(await invoke("tree_list", { dir: "" })).some((entry) => entry.name === "made_cut.txt"), 5000, "made_cut.txt moved into src");
  await press("file-terminal", "src");
  await ui.waitFor(() => /src/.test(document.getElementById("term-views").textContent), 10000, "the terminal in src");
  toggleTerminal(false);
  await press("file-select-compare", "hello.py");
  await press("file-compare", "README.md");
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "hello.py beside README.md");
  closeDiff();
  calls.answer("plugin:webview|create_webview_window", () => null);
  await press("file-open-window", "README.md");
  await ui.waitFor(() => made("plugin:webview|create_webview_window").length, 5000, "the page's window asked for");
  assert.ok(made("view_window").some((one) => one.args.path === "README.md"), "the file asked to open in a window of its own");
  assert.ok(made("plugin:webview|create_webview_window").some((one) => /README\.md$/.test(one.args.options.url)), "its window loads it from the view scheme");
}, { timeout: 60000 });

test("the tree's file menu, from the command line: an archive's names and a folder let go", async () => {
  await press("file-names", "bundle.zip", "windows-1252");
  assert.equal(JSON.parse(localStorage.getItem("orior.archive.names"))["bundle.zip"], "windows-1252");
  await press("file-names", "bundle.zip");
  assert.equal(JSON.parse(localStorage.getItem("orior.archive.names"))["bundle.zip"], undefined, "as the archive says again");
  const name = await invoke("tree_mount", { path: HOME() });
  assert.ok((await invoke("tree_mounts")).some((one) => one.name === name));
  await press("file-remove", name);
  assert.ok(!(await invoke("tree_mounts")).some((one) => one.name === name), "the folder let go");
});

test("a tab's menu, from the command line: close, close others, split, a new window, groups, marks and a notebook", async () => {
  // A tab with changes not saved asks before it closes; every one is saved first.
  asFound();
  await press("save-all");
  showView("edit");
  await openFile("hello.py");
  await openFile("README.md");
  await press("tab-close", "hello.py");
  assert.ok(!tabOfName("hello.py"), "hello.py's tab closed");
  await openFile("hello.py");
  await press("tab-close-others", "hello.py");
  assert.deepEqual([...document.querySelectorAll("#tabs .tab")].map((tab) => tab.title), ["hello.py"]);
  const desk = document.querySelector("#mode-edit .desk");
  await press("tab-split-right", "hello.py");
  assert.equal(desk.dataset.split, "right");
  await press("unsplit");
  await press("tab-split-down", "hello.py");
  assert.equal(desk.dataset.split, "down");
  await press("unsplit");
  await press("tab-group", "hello.py", "Made Group");
  await ui.waitFor(() => [...document.querySelectorAll(".tab-group-name")].some((one) => one.textContent === "Made Group"), 3000, "the group");
  await press("tab-ungroup", "hello.py");
  assert.ok(![...document.querySelectorAll(".tab-group-name")].some((one) => one.textContent === "Made Group"), "the group gone");
  await press("tab-mark", "hello.py");
  assert.ok(editing().markItems().some((item) => item.label.endsWith("hello.py")), "marked");
  await press("tab-mark", "hello.py");
  assert.ok(!editing().markItems().some((item) => item.label?.endsWith("hello.py")), "and let go");
  await invoke("file_write", { path: "notebook_cells.py", text: "# %%\nx = 1\n\n# %%\nprint(x)\n" });
  await openFile("notebook_cells.py");
  await press("tab-open-as", "notebook_cells.py", "notebook");
  await ui.waitFor(async () => (await menuOver(() => tabOfName("notebook_cells.py"))).some((item) => item.label === "Open as Text"), 5000, "the file open as a notebook");
  await press("tab-open-as", "notebook_cells.py", "text");
  await ui.waitFor(async () => (await menuOver(() => tabOfName("notebook_cells.py"))).some((item) => item.label === "Open as Notebook"), 5000, "and as its text again");
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args.words));
  await press("tab-new-window", "hello.py");
  assert.deepEqual(opened.at(-1), ["file", "open-file", "hello.py"]);
  await ui.waitFor(() => !tabOfName("hello.py"), 3000, "the tab gone to the new window");
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
