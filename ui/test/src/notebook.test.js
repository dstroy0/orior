// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook as cells: a .ipynb opened as a model of code and Markdown cells, changes marked and written back as the
// file keeps them, a cell out of date where a cell it stands on changed since it ran, and the file a Python cell is
// debugged in.

import { cellFile, cellFileOf, cellName, isNotebook, markNotebookSaved, notebookDirty, notebookFile, notebookOfCellFile, staleBy } from "../../gui/src-js/notebook.js";
import { code, markdown, openedNotebook } from "../notebooks.js";
import { putBackAsFound } from "../menus.js";

const { test, assert } = window.__harness;

const CELLS = [code("x = 1"), markdown("# Title"), code("print(x)", { execution_count: 2, outputs: [{ output_type: "stream", name: "stdout", text: "1\n" }] })];

test("a .ipynb opens as a notebook, its cells of code and Markdown drawn, each output and count as the file kept them", async () => {
  assert.ok(isNotebook("a/b.IPYNB") && !isNotebook("a.py"));
  const book = await openedNotebook("model", CELLS);
  assert.deepEqual(book.cells.map((cell) => cell.kind), ["code", "markdown", "code"]);
  assert.equal(book.languageName, "python");
  assert.equal(book.cells[2].count, 2);
  assert.equal(book.cells[2].outputs.length, 1);
  assert.ok(book.cells.every((cell) => /^[0-9a-f]{8}$/.test(cell.id)), "a cell the file gives no id takes one");
  assert.equal(book.cells[1].node.querySelector(".nb-source h1")?.textContent, "Title", "Markdown shows as it reads");
  assert.equal(book.cells[2].node.querySelector(".nb-outputs").textContent.trim(), "1");
});

test("a cell changed marks the notebook, and the file written back holds the cells as they stand", async () => {
  const book = await openedNotebook("written", CELLS);
  assert.ok(!notebookDirty(book));
  const cell = book.cells[0];
  cell.session.doc.change([{ from: { line: 0, col: 4 }, to: { line: 0, col: 5 }, text: "2" }], "type", cell.session.selections);
  assert.ok(notebookDirty(book));
  const file = notebookFile(book);
  assert.equal(file.cells[0].source, "x = 2");
  assert.ok(!("outputs" in file.cells[1]) && !("execution_count" in file.cells[1]), "a Markdown cell keeps no outputs");
  assert.deepEqual([file.cells[2].execution_count, file.cells[2].outputs.length], [2, 1]);
  assert.deepEqual(file.cells.map((one) => one.id), book.cells.map((one) => one.id), "each cell's id written, as nbformat 4.5 keeps them");
  markNotebookSaved(book);
  assert.ok(!notebookDirty(book));
});

test("a cell is out of date where one that ran before it was changed, ran again or was deleted since", async () => {
  const book = await openedNotebook("stale", CELLS);
  const [first, , last] = book.cells;
  try {
    first.runs = [1];
    last.runs = [2];
    assert.deepEqual(staleBy(book, last), []);
    first.edited = 3;
    assert.deepEqual(staleBy(book, last), [first], "changed since");
    first.edited = 0;
    first.runs = [1, 3];
    assert.deepEqual(staleBy(book, last), [first], "run again since");
    first.runs = [1];
    first.gone = true;
    assert.deepEqual(staleBy(book, last), [first], "deleted");
    assert.deepEqual(staleBy(book, first), [], "a cell that stands on none that ran before it");
    assert.equal(cellName(book, first), "cell 1");
    assert.equal(cellName(book, { id: "elsewhere" }), "a cell since deleted");
  } finally {
    Object.assign(first, { runs: [], edited: 0, gone: undefined });
    last.runs = [];
  }
});

test("a Python code cell is debugged in a file of its own, found again by its session and by its path", async () => {
  const book = await openedNotebook("debugged", CELLS);
  const [first, words] = book.cells;
  const file = cellFile(book, first);
  assert.equal(file, `build/orior/cells/notebook_debugged.ipynb.${first.id}.py`);
  assert.equal(cellFileOf(first.session), file);
  assert.equal(notebookOfCellFile(file), "notebook_debugged.ipynb");
  assert.equal(cellFile(book, words), null, "a Markdown cell has none");
});

test("the window is put back as the file found it", putBackAsFound);
