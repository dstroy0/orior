// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells as nodes of the page: the bar that adds a cell of code or Markdown after one, a cell's node with
// its run button, count and out-of-date mark, and its menu: its type changed, moved, its outputs cleared, deleted.

import { addBar } from "../../../gui/src-js/notebook/cells.js";
import { code, markdown, openedNotebook } from "../../notebooks.js";
import { itemsOfMenu, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

const CELLS = [code("a = 1"), markdown("words"), code("print(a)", { outputs: [{ output_type: "stream", name: "stdout", text: "1\n" }], execution_count: 1 })];

// The cell's menu opened, and its item `label` chosen.
async function chosen(cell, label) {
  cell.node.querySelector(".nb-cell-menu").click();
  await ui.waitFor(() => itemsOfMenu().some((item) => item.label === label), 3000, `the cell's menu with ${label}`);
  [...document.querySelectorAll("body > .menu .menu-item")].find((button) => button.item?.label === label).click();
}

test("a cell's node holds its run button, its count, its out-of-date mark hidden, and its menu, under its id", async () => {
  const book = await openedNotebook("nodes", CELLS);
  const [first, words] = book.cells;
  const node = first.node;
  assert.equal(node.dataset.id, first.id);
  assert.ok(node.classList.contains("nb-cell") && node.classList.contains("code"));
  assert.ok(words.node.classList.contains("markdown"));
  assert.equal(node.querySelector(".nb-run").textContent, "▶");
  assert.ok(node.querySelector(".nb-stale").hidden);
  assert.equal(book.cells[2].node.querySelector(".nb-count").textContent, "[1]");
});

test("the bar under a cell adds one of code or Markdown after it, which takes the keys", async () => {
  const book = await openedNotebook("added", CELLS);
  const bar = addBar(book, book.cells[0]);
  [...bar.querySelectorAll("button")].find((one) => one.textContent === "+ Markdown").click();
  await ui.waitFor(() => book.cells.length === 4 && book.focused === book.cells[1], 3000, "the cell added and focused");
  assert.equal(book.cells[1].kind, "markdown");
  assert.ok(book.changed);
  addBar(book, null).querySelector("button").click();
  await ui.waitFor(() => book.cells.length === 5, 3000);
  assert.equal(book.cells.at(-1).kind, "code", "with no cell given, at the end");
});

test("a cell's menu changes its kind, moves it, clears its outputs and deletes it, a deleted cell that ran kept as gone", async () => {
  const book = await openedNotebook("menu", CELLS);
  const [first, , last] = book.cells;
  await chosen(first, "Move Down");
  await ui.waitFor(() => book.cells[1] === first, 3000, "moved down");
  await chosen(last, "Clear Outputs");
  await ui.waitFor(() => last.outputs.length === 0 && last.node.querySelector(".nb-outputs").hidden, 3000, "its outputs cleared");
  await chosen(last, "Change to Markdown");
  await ui.waitFor(() => last.kind === "markdown", 3000);
  last.runs = [1];
  await chosen(last, "Delete");
  await ui.waitFor(() => !book.cells.includes(last), 3000);
  assert.ok(book.gone.includes(last) && last.gone, "a cell that ran is kept as gone");
});

test("the window is put back as the file found it", putBackAsFound);
