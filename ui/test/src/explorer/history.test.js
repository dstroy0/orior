// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer's menu, the commits', Local History's and Review's, each item from the command line.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { outlinePinned, panesNow, showPane } from "../../../gui/src-js/explorer.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { asFound, made, press, putBackAsFound, rowOf } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the explorer's menu, from the command line: a pane shown and hidden, the tree read again, its folders closed, and the Outline pinned", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await press("pane-show", "todo", "on");
  assert.ok(panesNow().todo.shown, "TODO shown");
  await press("pane-show", "todo", "off");
  assert.ok(!panesNow().todo.shown, "and hidden");
  const read = made("tree_list").length;
  await press("explorer-refresh");
  await ui.waitFor(() => made("tree_list").length > read, 5000, "the tree read again");
  if (rowOf("src")?.getAttribute("aria-expanded") !== "true") {
    await press("file-open", "src");
  }
  await ui.waitFor(() => rowOf("src")?.getAttribute("aria-expanded") === "true", 3000, "src open");
  await press("explorer-collapse");
  await ui.waitFor(() => rowOf("src")?.getAttribute("aria-expanded") === "false", 3000, "src closed");
  await press("outline-pin", "hello.py");
  assert.equal(outlinePinned(), "hello.py");
  await press("outline-pin", "none");
  assert.equal(outlinePinned(), null);
  asFound();
});

test("Local History's menu, from the command line: a save's difference shown, and the file taken back to it", async () => {
  asFound();
  showView("edit");
  await invoke("file_write", { path: "history_file.txt", text: "first\n" });
  await openFile("history_file.txt");
  const view = editing().editor;
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text: "second\n" }], null);
  await press("save");
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text: "third\n" }], null);
  await press("save");
  const saves = await ui.waitFor(async () => {
    const found = await invoke("history_list", { path: "history_file.txt" });
    return found.length >= 2 && found;
  }, 5000, "the saves kept");
  const second = saves.find((one) => one.size === "second\n".length) ?? saves.at(-1);
  await press("history-difference", "history_file.txt", String(second.at));
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the save beside the file");
  closeDiff();
  await press("history-revert", "history_file.txt", String(second.at));
  await ui.waitFor(async () => (await invoke("file_read", { path: "history_file.txt" })).text === "second\n" || editing().editor.doc.text() === "second\n", 5000, "the file taken back");
  asFound();
});

test("the window is put back as the file found it", putBackAsFound);
