// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A LaTeX file's pages in the live preview: made as it is typed, the last pages kept while the next
// are made, the errors of a make that failed listed over them, and the text tied to the pages both
// ways by SyncTeX.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { setPreview } from "../../gui/src-js/preview.js";
import { boxesOf, lastMade, lineAt } from "../../gui/src-js/texpreview.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("preview");
const pages = () => pane().querySelectorAll(".tex-page");

const BOXES = [
  { page: 1, file: "a.tex", x: 100, y: 100, w: 300, h: 12, lines: [[100, 4], [180, 5]] },
  { page: 1, file: "a.tex", x: 100, y: 120, w: 300, h: 12, lines: [[100, 5]] },
  { page: 2, file: "a.tex", x: 100, y: 100, w: 300, h: 12, lines: [[100, 9]] },
  { page: 2, file: "b.tex", x: 100, y: 140, w: 300, h: 12, lines: [[100, 2]] },
];

test("a line's boxes are those it was set in, else those of the nearest line before it that was set", () => {
  assert.deepEqual(boxesOf(BOXES, "a.tex", 4).map((box) => box.y), [100, 120], "line 4 from 0 is SyncTeX's 5");
  assert.deepEqual(boxesOf(BOXES, "a.tex", 6).map((box) => [box.page, box.y]), [[1, 100], [1, 120]], "a line set nowhere takes the one before it");
  assert.deepEqual(boxesOf(BOXES, "a.tex", 0), []);
  assert.deepEqual(boxesOf(BOXES, "b.tex", 1).map((box) => box.page), [2]);
});

test("a point on a page gives the line set there", () => {
  assert.deepEqual(lineAt(BOXES, 1, 200, 105), { file: "a.tex", line: 4 }, "past where line 5's part starts");
  assert.deepEqual(lineAt(BOXES, 1, 120, 105), { file: "a.tex", line: 3 });
  assert.deepEqual(lineAt(BOXES, 2, 150, 147), { file: "b.tex", line: 1 }, "a box of another file");
  assert.deepEqual(lineAt(BOXES, 2, 150, 400), { file: "b.tex", line: 1 }, "below every box, the nearest");
  assert.equal(lineAt(BOXES, 3, 0, 0), null);
});

const TEXT = "\\documentclass{article}\n\\begin{document}\nThe first page.\n\\newpage\nThe second page.\n\\end{document}\n";

test("a LaTeX file shows as its pages beside its text, the cursor's line marked on its page", async () => {
  await invoke("file_write", { path: "paper.tex", text: TEXT });
  showView("edit");
  await openFile("paper.tex");
  await ui.waitFor(() => editing().active === "paper.tex", 5000);
  setPreview("beside");
  await ui.waitFor(() => pages().length === 2, 60000).catch((error) => {
    throw new Error(`${error.message}: ${JSON.stringify(lastMade()?.errors ?? null)}`);
  });
  assert.equal(pane().dataset.kind, "latex");
  const { editor } = editing();
  editor.goTo(4, 3);
  await ui.waitFor(() => pane().querySelector('.tex-page[data-page="2"] .tex-here'), 3000);
  assert.equal(pane().querySelector('.tex-page[data-page="1"] .tex-here'), null);
});

test("a key typed is made again, the last pages kept while it is", async () => {
  const before = pane().querySelector(".tex-pages");
  const { editor } = editing();
  editor.goTo(4, 3);
  editor.focus();
  await ui.type(" more");
  await ui.waitFor(() => pane().dataset.making === "true", 5000);
  assert.equal(pages().length, 2, "the last pages stay while the next are made");
  await ui.waitFor(() => lastMade()?.id === editing().editor.s.doc.id && !pane().dataset.making, 60000);
  assert.notEqual(pane().querySelector(".tex-pages"), before, "drawn anew");
});

test("a press on a page takes the editor to the line set there", async () => {
  const box = lastMade().boxes.find((one) => one.page === 2 && one.file === "paper.tex" && one.lines.some(([, line]) => line === 5));
  assert.ok(box, `a box of line 5 on page 2: ${JSON.stringify(lastMade().boxes)}`);
  const page = pane().querySelector('.tex-page[data-page="2"]');
  page.scrollIntoView({ block: "center" });
  await ui.frames(2);
  const rect = page.getBoundingClientRect();
  const view = page.viewBox.baseVal;
  const { editor } = editing();
  editor.goTo(0, 0);
  const [across] = box.lines.find(([, line]) => line === 5);
  await ui.click({ x: rect.left + ((across + 2) / view.width) * rect.width, y: rect.top + ((box.y + box.h / 2) / view.height) * rect.height });
  await ui.waitFor(() => editing().editor.head().line === 4, 2000).catch((error) => {
    throw new Error(`${error.message}: the editor stands at line ${editing().editor.head().line}`);
  });
});

test("a make that fails keeps the last pages and lists its errors over them, each taking the editor to its line", async () => {
  const { editor } = editing();
  editor.goTo(2, 0);
  editor.focus();
  await ui.type("\\nosuchcommand ");
  await ui.waitFor(() => pane().querySelector(".tex-error"), 60000);
  assert.equal(pages().length, 2, "the last pages kept");
  assert.match(pane().querySelector(".tex-error").textContent, /^3: Undefined control sequence/);
  editor.goTo(0, 0);
  await ui.click(pane().querySelector(".tex-error"));
  assert.equal(editing().editor.head().line, 2);
  setPreview("off");
});
