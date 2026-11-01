// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells as branches on the window's side: each cell of the file a branch colored in its kind's language,
// its session the mirror of it, a cell the reader adds a branch of no text that what is written in it follows, and a
// cell deleted letting its branch go.

import { invoke } from "../../../gui/src-js/bridge.js";
import { addBar } from "../../../gui/src-js/notebook/cells.js";
import { code, markdown, openedNotebook } from "../../notebooks.js";
import { putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

// The shape of the branch under `key` on the window's side, `{ lines, units }`, or null where there is none.
const shapeOf = (key) => invoke("document_edit", { key, changes: [] }).catch(() => null);

test("each cell of a notebook opened is a branch colored in its kind's language, its session the mirror of it", async () => {
  const book = await openedNotebook("mirror", [code("# a comment\nx = 1"), markdown("# A heading")]);
  const [first, words] = book.cells;
  await ui.waitFor(() => first.session.highlight.key && words.session.highlight.key, 10000, "both cells mirrored");
  assert.notEqual(first.session.highlight.key, words.session.highlight.key, "each cell a branch of its own");
  await ui.waitFor(() => first.session.highlight.runsOf(0).some(([, name]) => name.includes("t-comment")), 10000, "the code cell's comment colored");
  const doc = first.session.doc;
  assert.deepEqual(await shapeOf(first.session.highlight.key), { lines: doc.count, units: doc.lines.reduce((sum, line) => sum + line.length, 0) }, "the branch is the cell's text");
});

test("a cell the reader adds is a branch of no text, and what is written in it follows it there", async () => {
  const book = await openedNotebook("mirror_added", [code("a = 1")]);
  addBar(book, null).querySelector("button").click();
  await ui.waitFor(() => book.cells.length === 2 && book.cells[1].session.doc.mirrored, 3000, "the cell added and mirrored");
  const session = book.cells[1].session;
  session.doc.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "y = 2" }], "test", session.selections);
  await ui.waitFor(async () => (await shapeOf(session.doc.mirrored.key))?.units === 5, 5000, "the written text on the window's side");
});

test("a cell deleted lets its branch go", async () => {
  const book = await openedNotebook("mirror_deleted", [code("a = 1"), code("b = 2")]);
  const last = book.cells[1];
  await ui.waitFor(() => last.session.doc.mirrored, 10000, "the cell mirrored");
  const key = last.session.doc.mirrored.key;
  last.node.querySelector(".nb-cell-menu").click();
  await ui.waitFor(() => [...document.querySelectorAll("body > .menu .menu-item")].some((button) => button.item?.label === "Delete"), 3000);
  [...document.querySelectorAll("body > .menu .menu-item")].find((button) => button.item?.label === "Delete").click();
  await ui.waitFor(() => !book.cells.includes(last), 3000);
  await ui.waitFor(async () => (await shapeOf(key)) === null, 5000, "no branch under its key");
});

test("the window is put back as the file found it", putBackAsFound);
