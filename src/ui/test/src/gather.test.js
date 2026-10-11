// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The task's code in one place: lines gathered from several files into a task, shown side by side
// over the editor, each the live text of its file, found again where lines come above it, and kept
// with the task.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { boardShown, closeBoard, gatherLines, linesHere, tasks } from "../../src/gather.js";
import { runCommand } from "../../src/menubar.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const ONE = "import os\n\n\ndef first():\n    a = 1\n    return a\n\n\ndef second():\n    return 2\n";
const TWO = "x = 1\ny = 2\nz = 3\n";

const tabNamed = (name) => [...document.querySelectorAll("#tabs .tab")].find((one) => one.textContent.includes(name));
const gathered = () => tasks().tasks[tasks().current].map((one) => [one.path, one.from, one.to]);

test("Gather Lines takes the declaration the cursor stands in, or the lines selected", async () => {
  localStorage.removeItem(`orior.gather.${document.getElementById("tree-path").textContent}`);
  await invoke("file_write", { path: "gather_one.py", text: ONE });
  await invoke("file_write", { path: "gather_two.py", text: TWO });
  showView("edit");
  await openFile("gather_one.py");
  await ui.waitFor(() => editing().active === "gather_one.py", 5000);
  const { editor } = editing();
  editor.goTo(4, 2);
  assert.deepEqual(linesHere(editor), [3, 5], "first(), its blank lines at the end left off");
  editor.goTo(9, 0);
  assert.deepEqual(linesHere(editor), [8, 9]);
  editor.goTo(4, 2);
  runCommand("gather-lines");
  await openFile("gather_two.py");
  await ui.waitFor(() => editing().active === "gather_two.py", 5000);
  editing().editor.select([{ anchor: { line: 1, col: 0 }, head: { line: 2, col: 5 }, goal: null }]);
  gatherLines();
  assert.deepEqual(gathered(), [["gather_one.py", 3, 5], ["gather_two.py", 1, 2]]);
});

test("Gathered Lines shows the pieces side by side, each the live text of its file, taking edits", async () => {
  runCommand("gathered-lines");
  await ui.waitFor(() => boardShown()?.editors.length === 2, 5000);
  const board = boardShown().board;
  const pieces = [...board.querySelectorAll(".gather-piece")];
  assert.equal(pieces.length, 2);
  const across = getComputedStyle(board.querySelector(".gather-pieces")).gridTemplateColumns.split(" ").length;
  if (across > 1) {
    const [a, b] = pieces.map((one) => one.getBoundingClientRect());
    assert.ok(Math.abs(a.top - b.top) < 2 && b.left > a.right - 1, "side by side");
  }
  const [one, two] = boardShown().editors;
  assert.equal(one.s.doc.line(one.head().line), "def first():");
  assert.equal(two.s.doc.line(two.head().line), "y = 2");
  assert.equal(two.s.readOnly, false, "a file a tab holds takes edits");
  two.goTo(1, 0);
  two.focus();
  await ui.type("# ");
  await ui.waitFor(() => tabNamed("gather_two.py")?.querySelector(".dirty"), 3000);
  assert.equal(editing().editor.s.doc.line(1), "# y = 2", "the tab has the edit");
});

test("a piece is found again by its first line where lines come above it", async () => {
  closeBoard();
  await openFile("gather_one.py");
  await ui.waitFor(() => editing().active === "gather_one.py", 5000);
  const { editor } = editing();
  editor.goTo(0, 0);
  editor.focus();
  await ui.type("import sys\n");
  runCommand("gathered-lines");
  await ui.waitFor(() => boardShown()?.editors.length === 2, 5000);
  assert.deepEqual(gathered()[0], ["gather_one.py", 4, 6]);
  const [one] = boardShown().editors;
  assert.equal(one.s.doc.line(one.head().line), "def first():");
});

test("a task is named, keeps its own pieces, and a piece's name opens its file at its lines", async () => {
  const board = () => boardShown().board;
  board().querySelector(".gather-name").value = "Other";
  await ui.click([...board().querySelectorAll(".gather-bar button")].find((one) => one.textContent === "New Task"));
  await ui.waitFor(() => board().querySelector(".gather-empty"), 2000);
  assert.equal(tasks().current, "Other");
  assert.deepEqual(gathered(), []);
  const select = board().querySelector(".gather-task");
  select.value = "Task 1";
  select.dispatchEvent(new Event("change"));
  await ui.waitFor(() => boardShown()?.editors.length === 2, 5000);
  assert.deepEqual(Object.keys(tasks().tasks), ["Task 1", "Other"]);
  await ui.click(board().querySelector(".gather-go"));
  await ui.waitFor(() => !boardShown() && editing().active === "gather_one.py" && editing().editor.head().line === 4, 5000);
});
