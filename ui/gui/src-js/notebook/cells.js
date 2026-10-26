// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells as nodes of the page: the bar under a cell that adds one after it, a cell's node, and what its
// menu does: its kind changed, moved, its outputs cleared, deleted.

import { blurCell, cellFile, cellOf, drawNotebook, element, focusCell, state } from "../notebook.js";
import { drawMarks, drawOutputs } from "./draw.js";
import { debugCell, runCell } from "./kernel.js";

// The bar under a cell that adds a cell after it, of code or of Markdown.
export function addBar(book, after) {
  const add = (kind) => {
    const made = cellOf(book, { cell_type: kind, source: "", metadata: {} });
    const index = after ? book.cells.indexOf(after) + 1 : book.cells.length;
    book.cells.splice(index, 0, made);
    book.changed = true;
    state.hooks.changed();
    drawNotebook(book).then(() => focusCell(book, made));
  };
  const code = element("button", { type: "button", textContent: "+ Code" });
  code.addEventListener("click", () => add("code"));
  const text = element("button", { type: "button", textContent: "+ Markdown" });
  text.addEventListener("click", () => add("markdown"));
  return element("div", { className: "nb-add" }, code, text);
}

export function cellNode(book, cell) {
  const run = element("button", { type: "button", className: "nb-run", textContent: "▶", title: "Run the cell (Shift+Enter)" });
  run.addEventListener("click", () => runCell(book, cell));
  const menu = element("button", { type: "button", className: "nb-cell-menu", textContent: "⋯", title: "The cell's menu" });
  menu.addEventListener("click", (event) => cellMenu(book, cell, event));
  const gutter = element("div", { className: "nb-gutter" }, run, element("span", { className: "nb-count" }), element("span", { className: "nb-stale", textContent: "!", hidden: true }));
  const source = element("div", { className: "nb-source" });
  source.addEventListener("mousedown", (event) => {
    if (book.focused !== cell && !event.target.closest("input, select, a")) {
      event.preventDefault();
      focusCell(book, cell);
    }
  });
  const main = element("div", { className: "nb-main" }, source, element("div", { className: "nb-forms" }), element("div", { className: "nb-outputs", hidden: true }));
  cell.node = element("div", { className: `nb-cell ${cell.kind}` }, gutter, main, menu);
  cell.node.dataset.id = cell.id;
  return cell.node;
}

function cellMenu(book, cell, event) {
  const items = [
    { label: "Run", run: () => runCell(book, cell) },
    { label: "Debug", disabled: !cellFile(book, cell), run: () => debugCell(book, cell) },
    { label: cell.kind === "code" ? "Change to Markdown" : "Change to Code", run: () => changeKind(book, cell) },
    { label: "Move Up", disabled: book.cells.indexOf(cell) === 0, run: () => moveCell(book, cell, -1) },
    { label: "Move Down", disabled: book.cells.indexOf(cell) === book.cells.length - 1, run: () => moveCell(book, cell, 1) },
    { label: "Clear Outputs", run: () => clearOutputs(book, cell) },
    { label: "Delete", run: () => deleteCell(book, cell) },
  ];
  state.hooks.menu(items, event);
}

function changeKind(book, cell) {
  if (book.focused === cell) {
    blurCell(book);
  }
  cell.kind = cell.kind === "code" ? "markdown" : "code";
  cell.session.setLanguage(cell.kind === "code" ? book.language : state.hooks.languageOf("cell.md"));
  cell.outputs = [];
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}

function moveCell(book, cell, by) {
  const index = book.cells.indexOf(cell);
  book.cells.splice(index, 1);
  book.cells.splice(index + by, 0, cell);
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}

function clearOutputs(book, cell) {
  cell.outputs = [];
  book.changed = true;
  state.hooks.changed();
  drawOutputs(book, cell);
  drawMarks(book);
}

function deleteCell(book, cell) {
  if (book.focused === cell) {
    blurCell(book);
  }
  book.cells = book.cells.filter((one) => one !== cell);
  if (cell.runs.length) {
    cell.gone = true;
    book.gone.push(cell);
  }
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}
