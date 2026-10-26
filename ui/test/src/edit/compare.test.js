// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Compare: two files of the tree side by side, a file's text as its tab holds it, a revision beside another or the
// file, a commit's change to a file, a tab given its file's text again from the tree, and Local History's snapshot
// shown and taken back to.

import { invoke } from "../../../gui/src-js/bridge.js";
import { compareCommit, compareFiles, openTouched, reloadFromDisk, revertSnapshot, showSnapshot, textNow } from "../../../gui/src-js/edit/compare.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { putBackAsFound, told } from "../../menus.js";

const { test, assert, ui, calls } = window.__harness;

const view = () => document.querySelector(".diff-view");
const sides = () => view()?.querySelector(".diff-sides").textContent;

// The files compared_<name>.txt written with `texts`, and the first open in the editor.
async function written(texts) {
  const paths = Object.keys(texts).map((name) => `compared_${name}.txt`);
  for (const [name, text] of Object.entries(texts)) {
    await invoke("file_write", { path: `compared_${name}.txt`, text });
  }
  showView("edit");
  await openFile(paths[0]);
  await ui.waitFor(() => editing().active === paths[0] && editing().editor?.s, 5000);
  return paths;
}

test("two files side by side, the first chosen on the left, each as its tab holds it where one is open", async () => {
  const [left, right] = await written({ a: "one\ntwo\n", b: "one\n2\n" });
  editing().editor.goTo(0, 3);
  editing().editor.type("!");
  assert.equal(await textNow(left), "one!\ntwo\n", "the tab's text, not saved");
  assert.equal(await textNow(right), "one\n2\n", "the tree's, where no tab holds it");
  await compareFiles(left, right);
  await ui.waitFor(() => view(), 3000);
  assert.equal(view().querySelector(".diff-name").textContent, `${left} · ${right}`);
  assert.equal(sides(), `${left}, then ${right}`);
  assert.equal(view().querySelectorAll(".diff-row.changed").length, 2);
  closeDiff();
});

test("a revision beside another, the older left, a commit's change to a file, and a revision that does not read said", async () => {
  const [path] = await written({ revisions: "now\n" });
  calls.answer("file_at", ({ id }) => `text at ${id}\n`);
  await compareCommit(path, { id: "bbbbbbb2222", when: 20, author: "Writer" }, { id: "aaaaaaa1111", when: 10 });
  await ui.waitFor(() => sides() === "aaaaaaa, then bbbbbbb by Writer", 3000, "the older on the left");
  await openTouched({ id: "ccccccc3333", parents: ["ddddddd4444"], author: "" }, { path, state: "A" });
  await ui.waitFor(() => sides() === "ddddddd, then ccccccc", 3000, "a file the commit added");
  assert.ok(view().querySelectorAll(".diff-row.added").length >= 1);
  calls.answer("file_at", () => {
    throw new Error("no such revision");
  });
  await compareCommit(path, { id: "eeeeeee5555", when: 1 });
  assert.ok(told(/no such revision/));
  closeDiff();
});

test("a tab is given its file's text again from the tree, and its changes are no longer marked", async () => {
  const [path] = await written({ reloaded: "first\n" });
  editing().editor.type("x");
  assert.ok(editing().activeChanged);
  await invoke("file_write", { path, text: "from the tree\n" });
  await reloadFromDisk([path]);
  assert.equal(editing().editor.s.doc.text(), "from the tree\n");
  assert.ok(!editing().activeChanged);
});

test("Local History's snapshot is shown beside the file, and the file taken back to it as one step", async () => {
  const [path] = await written({ snapshot: "as it stands\n" });
  calls.answer("history_read", () => "as it was saved\n");
  await showSnapshot(path, Date.UTC(2026, 0, 2, 3, 4, 5));
  await ui.waitFor(() => /^saved .+, then as it stands$/.test(sides() ?? ""), 3000);
  await revertSnapshot(path, Date.UTC(2026, 0, 2, 3, 4, 5));
  assert.equal(view(), null, "the view closed");
  assert.equal(editing().editor.s.doc.text(), "as it was saved\n");
  editing().editor.s.doc.undo();
  assert.equal(editing().editor.s.doc.text(), "as it stands\n", "one step undo takes back");
});

test("the window is put back as the file found it", putBackAsFound);
