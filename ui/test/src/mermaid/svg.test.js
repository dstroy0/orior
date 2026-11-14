// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What every diagram is drawn with: text made safe, a label's lines and width, a label drawn, a line of a
// diagram's text with its comment cut, and the SVG that holds a drawing.

import { escape, lineOf, linesOf, svg, textAt, widest, width } from "../../../gui/src-js/mermaid/svg.js";

const { test, assert } = window.__harness;

test("text is made safe for SVG, each of & < > and \" written as its entity", () => {
  assert.equal(escape('a & b < c > d "e"'), "a &amp; b &lt; c &gt; d &quot;e&quot;");
  assert.equal(escape(42), "42");
});

test("a label's lines start at each <br> and each \\n, and its width is its widest line's", () => {
  assert.deepEqual(linesOf("one<br>two<br/>three<BR />four\\nfive"), ["one", "two", "three", "four", "five"]);
  assert.ok(width("WWWW") > width("ii"), "a wide character takes more room than a narrow one");
  assert.equal(widest("short<br>a much longer line"), width("a much longer line"));
});

test("a label is drawn a line under the next, centered on its point, each line's text made safe", () => {
  const drawn = new DOMParser().parseFromString(`<svg xmlns="http://www.w3.org/2000/svg">${textAt(100, 50, "a<br>b & c")}</svg>`, "image/svg+xml").documentElement;
  const lines = [...drawn.querySelectorAll("text")];
  assert.deepEqual(lines.map((one) => one.textContent), ["a", "b & c"]);
  assert.deepEqual(lines.map((one) => Number(one.getAttribute("y"))), [41.5, 58.5], "17 apart, centered on 50");
  assert.equal(lines[0].getAttribute("text-anchor"), "middle");
});

test("a line of a diagram's text has its %% comment cut and its ends trimmed", () => {
  assert.equal(lineOf("  A --> B %% the first edge"), "A --> B");
  assert.equal(lineOf("%% only a comment"), "");
});

test("a drawing is retained in an SVG of its type, its size rounded up, with the markers every edge can end in", () => {
  const drawing = new DOMParser().parseFromString(svg(10.2, 20.7, "<g/>", "pie"), "image/svg+xml").documentElement;
  assert.equal(drawing.getAttribute("class"), "mermaid-diagram pie");
  assert.deepEqual([drawing.getAttribute("width"), drawing.getAttribute("height"), drawing.getAttribute("viewBox")], ["11", "21", "0 0 11 21"]);
  assert.deepEqual([...drawing.querySelectorAll("marker")].map((one) => one.id), ["mm-arrow", "mm-arrow-back", "mm-circle", "mm-cross", "mm-triangle", "mm-diamond", "mm-diamond-open"]);
});
