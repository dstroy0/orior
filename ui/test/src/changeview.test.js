// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A change reviewed by its structure: the tree's change and a commit's shown as the declarations they
// touch, the pieces that call one another in a row, each caller before what it calls, a move as one
// piece, and a commit named with who made it.

import { invoke } from "../../gui/src-js/bridge.js";
import { changeShown, closeChange, rowsOf } from "../../gui/src-js/changeview.js";
import { editing } from "../../gui/src-js/edit.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const A_WAS = "def main():\n    helper()\n    return 0\n\n\ndef helper():\n    return 1\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n";
const A_NOW = "def main():\n    helper()\n    other()\n    return 0\n\n\ndef helper():\n    return 2\n";
const B_WAS = "def other():\n    return 3\n";
const B_NOW = "def other():\n    return moving()\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n";

test("a piece's rows mark the lines taken out and the lines put in", () => {
  assert.deepEqual(rowsOf({ was: "a\nb\nc", now: "a\nB\nc" }), [[" ", "a"], ["-", "b"], ["+", "B"], [" ", "c"]]);
  assert.deepEqual(rowsOf({ was: "", now: "x" }), [["+", "x"]]);
  assert.deepEqual(rowsOf({ was: "y", now: "" }), [["-", "y"]]);
});

test("the tree's change shows as the declarations it touches, each caller before what it calls, a move as one", async () => {
  const root = await invoke("root_get");
  await invoke("folder_create", { path: "shape" }).catch(() => {});
  await invoke("repo_init", { folder: `${root}/shape` });
  await invoke("file_write", { path: "shape/a.py", text: A_WAS });
  await invoke("file_write", { path: "shape/b.py", text: B_WAS });
  await invoke("git_commit", { message: "first", paths: ["shape/a.py", "shape/b.py"] });
  await invoke("file_write", { path: "shape/a.py", text: A_NOW });
  await invoke("file_write", { path: "shape/b.py", text: B_NOW });
  showView("edit");
  runCommand("review-structure");
  await ui.waitFor(() => changeShown()?.view.querySelector(".cv-piece"), 10000);
  const { view } = changeShown();
  assert.equal(view.querySelector(".cv-title").textContent, "Changes since the last commit", "no one is named for the tree's own change");
  const rows = [...view.querySelectorAll(".cv-group")].map((group) => [...group.querySelectorAll(".cv-name")].map((name) => name.textContent));
  assert.deepEqual(rows, [["main", "helper", "other", "moving"]], "one row, each caller before what it calls");
  assert.equal(view.querySelector(".cv-piece.moved .cv-how").textContent, "moved from shape/a.py");
  assert.ok(view.querySelector(".cv-piece.changed .cv-line.in") && view.querySelector(".cv-piece.changed .cv-line.out"), "lines taken out and put in marked");
  assert.deepEqual([...view.querySelector(".cv-piece .cv-calls").querySelectorAll(".cv-call")].map((one) => one.textContent), ["helper", "other"]);
  await ui.click(view.querySelector(".cv-name"));
  await ui.waitFor(() => !changeShown() && editing().active === "shape/a.py" && editing().editor.head().line === 0, 5000);
});

test("a commit is reviewed against its parent and named with who made it", async () => {
  const said = await invoke("git_commit", { message: "second", paths: ["shape/a.py", "shape/b.py"] });
  const id = said.match(/\[[^\]]*?([0-9a-f]{7,40})\]/)?.[1];
  assert.ok(id, said);
  runCommand("review-structure", [id]);
  await ui.waitFor(() => changeShown()?.view.querySelector(".cv-piece"), 10000);
  assert.match(changeShown().view.querySelector(".cv-title").textContent, new RegExp(`^${id.slice(0, 8)} by .+: second$`));
  assert.equal(changeShown().read.pieces.length, 4);
  closeChange();
});
