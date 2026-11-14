// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells drawn: the fields a cell's #@param lines make and their values written back, each type of
// output, a long one cut to its ends, the counts and out-of-date marks, and the map beside the cells.

import { LONG } from "../../../gui/src-js/notebook.js";
import { drawMarks, drawOutputs, formsOf } from "../../../gui/src-js/notebook/draw.js";
import { code, markdown, openedNotebook } from "../../notebooks.js";
import { putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

const PNG = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";

test("a #@param line is a field: text, a number, a box, a list of choices, or a slider with its bounds", () => {
  const fields = formsOf(['name = "x"  #@param {type:"string"}', "count = 3  #@param", "on = True #@param", 'pick = "b" #@param ["a", "b", 3]', "level = 5 #@param {type:\"slider\", min:0, max:10, step:1}", "plain = 1"].join("\n"));
  assert.deepEqual(fields.map((one) => [one.line, one.name, one.type]), [[0, "name", "string"], [1, "count", "number"], [2, "on", "boolean"], [3, "pick", "choice"], [4, "level", "slider"]]);
  assert.deepEqual(fields[3].choices, ["a", "b", "3"]);
  assert.deepEqual(fields[4].bounds, { min: "0", max: "10", step: "1" });
});

test("a cell's fields are drawn under it, and a choice made is written into its line", async () => {
  const book = await openedNotebook("forms", [code('mode = "fast"  #@param ["fast", "slow"]\nrun = True  #@param'), markdown("x")]);
  const cell = book.cells[0];
  const forms = cell.node.querySelector(".nb-forms");
  const select = forms.querySelector("select");
  assert.equal(select.value, "fast");
  assert.ok(forms.querySelector('input[type="checkbox"]').checked);
  select.value = "slow";
  select.dispatchEvent(new Event("change"));
  await ui.waitFor(() => cell.session.doc.line(0).startsWith('mode = "slow"'), 3000, "the line written");
  assert.match(cell.session.doc.line(0), /^mode = "slow"\s+#@param/);
});

test("each type of output is drawn: a stream, an error's traceback, an image, HTML in a frame, Markdown, and a long text cut", async () => {
  const book = await openedNotebook("outputs", [code("x"), markdown("y")]);
  const cell = book.cells[0];
  const long = Array.from({ length: LONG + 10 }, (_, at) => `line ${at}`).join("\n");
  cell.outputs = [
    { output_type: "stream", name: "stderr", text: "warned\n" },
    { output_type: "error", ename: "ValueError", evalue: "bad", traceback: ["Traceback", "ValueError: bad"] },
    { output_type: "display_data", data: { "image/png": PNG, "text/plain": "<Figure>" } },
    { output_type: "display_data", data: { "text/html": "<b>bold</b>" } },
    { output_type: "display_data", data: { "text/markdown": "*said*" } },
    { output_type: "execute_result", data: { "text/plain": long } },
  ];
  drawOutputs(book, cell);
  const host = cell.node.querySelector(".nb-outputs");
  assert.ok(!host.hidden);
  assert.equal(host.querySelector(".nb-stream.stderr").textContent, "warned\n");
  assert.equal(host.querySelector(".nb-error").textContent, "Traceback\nValueError: bad");
  assert.ok(host.querySelector("img.nb-image").src.startsWith("data:image/png;base64,"), "the image, the richest form");
  assert.equal(host.querySelector("iframe.nb-html").getAttribute("sandbox"), "allow-scripts", "HTML with no reach into the window");
  assert.equal(host.querySelector(".nb-markdown em")?.textContent, "said");
  const more = host.querySelector(".nb-more");
  assert.equal(more.textContent, `Show all ${LONG + 10} lines`);
  more.click();
  assert.ok(!host.querySelector(".nb-more") && host.textContent.includes(`line ${LONG}`), "the whole a press away");
  cell.outputs = [];
  drawOutputs(book, cell);
  assert.ok(host.hidden, "no outputs, none displayed");
});

test("a cell's count shows its run, a running cell [*], and one out of date its mark naming the cells it is on", async () => {
  const book = await openedNotebook("marks", [code("a = 1"), code("b = a")]);
  const [first, second] = book.cells;
  try {
    second.count = 4;
    first.runs = [1];
    second.runs = [2];
    first.edited = 3;
    first.running = true;
    drawMarks(book);
    assert.equal(first.node.querySelector(".nb-count").textContent, "[*]");
    assert.equal(second.node.querySelector(".nb-count").textContent, "[4]");
    const mark = second.node.querySelector(".nb-stale");
    assert.ok(!mark.hidden && second.node.classList.contains("stale"));
    assert.match(mark.title, /^Out of date: it is on cell 1, changed, run again or deleted since this cell ran$/);
    assert.equal(document.querySelectorAll("#notebook .nb-map .nb-map-cell").length, 2, "a block of the map for each cell");
    assert.ok(document.querySelector("#notebook .nb-map .nb-map-cell.stale"));
  } finally {
    Object.assign(first, { runs: [], edited: 0, running: false });
    Object.assign(second, { runs: [], count: null });
    drawMarks(book);
  }
});

test("the window is put back as the file found it", putBackAsFound);
