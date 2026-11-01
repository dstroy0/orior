// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the editor draws in its gutter: the mark of a line added, changed, or with lines taken out above or below it,
// and Line History's date on the first line of each run a commit left; and in its text, a row written again into the
// node it is kept in, and each bracket orior's side names with its depth in the color of that depth.

import { classOf } from "../../../../gui/src-js/editor/grammar.js";
import { given, putBackAsFound } from "../../../menus.js";

const { test, assert, ui } = window.__harness;

test("a line added or changed has its bar, and lines taken out a wedge above, or below the last line", async () => {
  const view = await given("a\nb\nc");
  const kept = view.s.changes;
  try {
    view.setChanges(view.s, { added: new Set([0]), changed: new Set([1]), removed: new Set([2, 3]) });
    assert.match(view.changeMark(0), /ed-change added/);
    assert.match(view.changeMark(1), /ed-change changed/);
    assert.match(view.changeMark(2), /ed-change removed" data-change="2"/);
    assert.match(view.changeMark(2), /ed-change removed below" data-change="3"/);
    view.setChanges(view.s, null);
    assert.equal(view.changeMark(0), "", "no marks where none are set");
  } finally {
    view.setChanges(view.s, kept);
  }
});

test("a commit's date stands on the first line of the run it left, and a line no commit holds has no mark", async () => {
  const view = await given("a\nb\nc");
  const kept = view.s.history;
  const when = Date.UTC(2026, 0, 15, 12) / 1000;
  const commit = { id: "0123456789abcdef", when, author: "Someone", subject: "The subject" };
  try {
    view.setLineHistory(view.s, { lines: [0, 0, -1], commits: [commit] });
    const date = new Date(when * 1000);
    const day = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
    const first = view.historyMark(0);
    assert.match(first, /class="ed-history first"/);
    assert.ok(first.includes(`>${day}`), "the date shown on the run's first line");
    assert.ok(first.includes("01234567") && first.includes("The subject"), "the id and subject under the pointer");
    assert.match(view.historyMark(1), /class="ed-history" [^>]*><\/span>/, "the run's next line marked, no date shown");
    assert.equal(view.historyMark(2), "");
  } finally {
    view.setLineHistory(view.s, kept);
  }
});

test("a row written again is put into the node it is kept in, its spans given the classes and the text its markup draws, those it kept kept", async () => {
  const view = await given("total = 1\ncount = 'two' + 3\n");
  await ui.waitFor(() => view.s.highlight.runsOf(1).some(([, cls]) => cls), 5000, "the given file's colors");
  const node = document.createElement("div");
  node.innerHTML = view.rowHtml(0);
  const keptSpans = [...node.children];
  assert.ok(view.rowPatched(node, view.rows().rowOf(1)), "a row of colored text is put in place");
  const drawn = document.createElement("div");
  drawn.innerHTML = view.rowHtml(1);
  const spans = (host) => [...host.children].map((span) => [span.className, span.textContent]);
  assert.equal(node.textContent, drawn.textContent);
  assert.deepEqual(spans(node), spans(drawn));
  assert.ok(keptSpans.length > 0 && keptSpans.every((span, at) => node.children[at] === span || at >= node.children.length), "the spans the node had stay");
});

test("a row put into a node that kept a hint and a fold's mark keeps neither's width nor its fold in the spans it gives the row", async () => {
  const view = await given("total = 1\ncount = 'two' + 3\n");
  await ui.waitFor(() => view.s.highlight.runsOf(1).some(([, cls]) => cls), 5000, "the given file's colors");
  const node = document.createElement("div");
  node.innerHTML = '<span class="ed-hint" style="width:40px">value:</span><span class="ed-folded" data-fold="3">⋯</span>';
  assert.ok(view.rowPatched(node, view.rows().rowOf(1)), "the row put in place");
  const drawn = document.createElement("div");
  drawn.innerHTML = view.rowHtml(1);
  assert.equal(node.innerHTML, drawn.innerHTML);
});

test("a wrapped line's row written again is put into the node it is kept in, its part's width and spans as its markup draws them, the spans it kept kept", async () => {
  const view = await given(`${"total = 'two' + 12345; ".repeat(40)}\ncount = 3\n`);
  await ui.waitFor(() => view.s.highlight.runsOf(0).some(([, cls]) => cls), 5000, "the given file's colors");
  const was = view.wrapOn;
  try {
    view.setWrap(true);
    const rows = view.rows();
    const first = rows.rowOf(0);
    assert.ok(rows.lastRowOf(0) >= first + 2, "the long line wraps");
    const node = document.createElement("div");
    node.innerHTML = view.rowCell(first + 1);
    const keptSpans = [...node.querySelectorAll("span")];
    assert.ok(view.rowPatched(node, first + 2), "a wrapped line's row is put in place");
    const drawn = document.createElement("div");
    drawn.innerHTML = view.rowCell(first + 2);
    const shape = (host) => [host.textContent, ...[...host.querySelectorAll("*")].map((one) => [one.tagName, one.className, one.style.width, one.children.length ? "" : one.textContent])];
    assert.deepEqual(shape(node), shape(drawn));
    assert.ok(keptSpans.some((span) => node.contains(span)), "the spans the node had stay");
    const whole = document.createElement("div");
    whole.innerHTML = view.rowCell(first);
    assert.ok(view.rowPatched(whole, rows.rowOf(1)), "a row of a line that does not wrap put into a node a wrapped row kept");
    drawn.innerHTML = view.rowCell(rows.rowOf(1));
    assert.deepEqual(shape(whole), shape(drawn));
  } finally {
    view.setWrap(was);
  }
});

test("a wrapped line longer than a row not wrapped is drawn across draws each of its cols in its rows, to the line's end", async () => {
  const end = "the line's end";
  const view = await given(`${"total = 'two' + 12345; ".repeat(500)}${end}\ncount = 3\n`);
  const was = view.wrapOn;
  try {
    view.setWrap(true);
    const rows = view.rows();
    const drawn = document.createElement("div");
    let line = "";
    for (let row = rows.rowOf(0); row <= rows.lastRowOf(0); row += 1) {
      drawn.innerHTML = view.rowCell(row);
      line += drawn.textContent;
    }
    assert.ok(line === view.s.doc.line(0), `the rows' text the line's: ${line.length} of its ${view.s.doc.line(0).length} letters, ending ${line.slice(-20)}`);
    const node = document.createElement("div");
    node.innerHTML = view.rowCell(rows.rowOf(0));
    assert.ok(view.rowPatched(node, rows.lastRowOf(0)), "the line's last row put in place");
    assert.ok(node.textContent.endsWith(end), `the last row ends the line: ${node.textContent.slice(-40)}`);
  } finally {
    view.setWrap(was);
  }
});

test("a bracket named with the depth its pair stands at takes that depth's color, each depth its own, where bracket pairs are colored", async () => {
  const view = await given("");
  const was = view.bracketsOn;
  const colorOf = (depth) => {
    const span = document.createElement("span");
    span.className = classOf(`delimiter.bracket.depth.open.${depth}`);
    view.host.append(span);
    const color = getComputedStyle(span).color;
    span.remove();
    return color;
  };
  try {
    view.setBrackets(true);
    const colors = [0, 1, 2, 3, 4, 5, 6, 7, 10, 11].map(colorOf);
    assert.equal(new Set(colors).size, colors.length, `each depth a color of its own: ${colors.join(", ")}`);
    view.setBrackets(false);
    assert.equal(new Set([0, 1, 2, 3].map(colorOf)).size, 1, "no depth's color where bracket pairs are not colored");
  } finally {
    view.setBrackets(was);
  }
});

test("the window is put back as the file found it", putBackAsFound);
