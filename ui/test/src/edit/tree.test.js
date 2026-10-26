// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tree: folders and files added to the window, and archives opened as folders.

import { chooseNames, treeRowShown } from "../../../gui/src-js/edit/tree.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing } from "../../../gui/src-js/edit.js";
import { calls, HOME, made, press, putBackAsFound, TREE } from "../../menus.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("File, Add Folder, Add File, New Window and Open Folder in New Window", async () => {
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args));
  await press("add-folder", HOME());
  assert.ok(made("tree_mount").some((one) => one.args.path === HOME()), "the folder mounted beside the tree");
  await invoke("file_write_any", { path: `${HOME()}/added.txt`, text: "added\n" });
  await press("add-file", `${HOME()}/added.txt`);
  await ui.waitFor(() => editing().active?.startsWith("@"), 5000, "the file opened beside the tree");
  await press("new-window");
  assert.deepEqual(opened.at(-1)?.words, [], "a new window");
  await press("open-folder-window", TREE());
  assert.equal(opened.at(-1)?.root, TREE(), "a new window on the folder");
});

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

test("the window is put back as the file found it", putBackAsFound);
