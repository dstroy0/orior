// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's tabs and files: which side of a large file, read a slice at a time, is read next, and
// a zip in the tree opened as a folder, its files read, changed and saved back into it.

import { invoke } from "../../src/bridge.js";
import { chooseNames, editing, nextSlice } from "../../src/edit.js";
import { togglePane } from "../../src/sides.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const rowOf = (path) => document.getElementById("files").querySelector(`.node[data-key="${path}"]`);

test("a large file is read the way the view heads while it scrolls", () => {
  const both = { canAbove: true, canBelow: true, moving: true };
  assert.equal(nextSlice({ ...both, heading: -1, below: true }), "above");
  assert.equal(nextSlice({ ...both, heading: 1, below: false }), "below");
});

test("at rest a large file is read below and above in turn", () => {
  const both = { canAbove: true, canBelow: true, moving: false, heading: -1 };
  assert.equal(nextSlice({ ...both, below: true }), "below");
  assert.equal(nextSlice({ ...both, below: false }), "above");
});

test("a side read to its end is read no more, whichever way the view heads", () => {
  assert.equal(nextSlice({ canAbove: false, canBelow: true, moving: true, heading: -1, below: false }), "below");
  assert.equal(nextSlice({ canAbove: true, canBelow: false, moving: true, heading: 1, below: true }), "above");
});

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
