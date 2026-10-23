// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Files dropped on the window from outside it: the folder each goes into by where it is dropped, the
// copy made behind with its bar on the status bar, and nothing opened. The window's drop event is
// sent as the system sends it, and the file comes from orior's home, outside the tree.

import { invoke, listen } from "../../gui/src-js/bridge.js";
import { folderAt } from "../../gui/src-js/drop.js";
import { editing } from "../../gui/src-js/edit.js";
import { togglePane } from "../../gui/src-js/sides.js";
import { status } from "../../gui/src-js/status.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const rowOf = (path) => document.getElementById("files").querySelector(`.node[data-key="${path}"]`);
const middle = (node) => {
  const box = node.getBoundingClientRect();
  return { x: box.left + box.width / 2, y: box.top + box.height / 2 };
};
const under = ({ x, y }) => {
  const found = document.elementFromPoint(x, y);
  return found ? `${found.tagName.toLowerCase()}.${[...found.classList].join(".")}` : "nothing";
};

test("a drop goes into the folder it is dropped on, the folder of a file it is dropped on, or the tree's top", async () => {
  showView("edit");
  togglePane(true, { take: false, pin: true });
  // The pane has slid in where each row is what stands at its own middle.
  const standing = (path) => rowOf(path)?.contains(document.elementFromPoint(middle(rowOf(path)).x, middle(rowOf(path)).y));
  await ui.waitFor(() => standing("src") && standing("hello.py"), 10000);
  const folder = middle(rowOf("src"));
  assert.equal(folderAt(folder.x, folder.y), "src", `on the folder: ${under(folder)} there`);
  const file = middle(rowOf("hello.py"));
  assert.equal(folderAt(file.x, file.y), "", `on a file at the top: ${under(file)} there`);
  const editor = middle(document.getElementById("editor"));
  assert.equal(folderAt(editor.x, editor.y), "");
});

test("a file dropped on a folder is copied into it behind the window, its bar on the status bar as it goes, and is not opened", async () => {
  const home = await invoke("home_folder");
  const source = `${home}/dropped/large.txt`;
  await invoke("file_write_any", { path: source, text: "a line of the dropped file\n".repeat(400000) });
  const active = editing().active;
  const folder = middle(rowOf("src"));
  // Each step of the copy as the window hears it, and whether its bar stood on the status bar once the window's own
  // listener had taken the step: this listener may run before it, so the bar is read a task later.
  const heard = [];
  let hearing = true;
  await listen("bring-progress", ({ payload }) => hearing && setTimeout(() => heard.push({ ...payload, shown: status.reads.has(payload.path) }), 0));
  await invoke("plugin:event|emit", { event: "tauri://drag-drop", payload: { paths: [source], position: { x: folder.x * devicePixelRatio, y: folder.y * devicePixelRatio } } });
  await ui.waitFor(() => document.getElementById("status-said").textContent.includes("Copied src/large.txt into the tree"), 10000);
  hearing = false;
  const going = heard.filter((one) => one.path === "src/large.txt" && one.done < one.size);
  assert.ok(going.length && going.every((one) => one.shown), `its bar stood while it was copied: ${JSON.stringify(heard)}`);
  assert.ok(!status.reads.has("src/large.txt"), "and went once it was whole");
  const copied = await invoke("file_read", { path: "src/large.txt" });
  assert.equal(copied.size, "a line of the dropped file\n".length * 400000, "copied whole");
  assert.equal(editing().active, active, "nothing dropped is opened");
  assert.ok(!document.querySelector(`#tabs .tab .close[title="src/large.txt"]`), "no tab holds it");
});
