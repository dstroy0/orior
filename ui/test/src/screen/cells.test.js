// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A screen's cells and their styles: a style changed by SGR's numbers, a run's output drawn in its colors, the lines
// of a colored text with the codes taken out, and a screen line drawn with its cursor.

import { blank, coloredHtml, coloredLines, lineHtml, lineOf, PLAIN, restyled } from "../../../gui/src-js/screen/cells.js";

const { test, assert } = window.__harness;

test("a style is changed by SGR's numbers: weight, the 16 colors, the 256 and any red, green and blue, and reset", () => {
  const bold = restyled(PLAIN, [1, 31, 44]);
  assert.deepEqual([bold.bold, bold.fg, bold.bg], [true, 1, 4]);
  assert.equal(restyled(bold, [22]).bold, false);
  assert.equal(restyled(PLAIN, [93]).fg, 11, "the bright colors follow the eight");
  assert.equal(restyled(PLAIN, [38, 5, 196]).fg, 196);
  assert.equal(restyled(PLAIN, [48, 2, 1, 2, 3]).bg, "rgb(1, 2, 3)");
  assert.equal(restyled(PLAIN, [38, 2, null, 4, 5, 6]).fg, "rgb(4, 5, 6)", "an empty color space before the three parts");
  assert.deepEqual(restyled(bold, [0]), PLAIN);
  assert.ok(Object.isFrozen(bold), "a style is never changed once made");
});

test("a run's output is drawn in the colors and weight its codes set, a 256 color as its red, green and blue", () => {
  assert.equal(coloredHtml("\x1b[1;32mok\x1b[0m done"), '<span class="t-f2 t-bold">ok</span> done');
  assert.equal(coloredHtml("\x1b[38;5;196mX"), '<span style="color:rgb(255, 0, 0)">X</span>');
  assert.equal(coloredHtml("a < b \x1b]0;title\x07& c"), "a &lt; b &amp; c", "text made safe, other sequences read past");
  assert.equal(coloredHtml("\x1b[7mI"), '<span class="t-f-default-bg t-b-default-fg">I</span>', "inverse swaps the defaults");
});

test("a colored text's lines carry their style from one to the next, each with its codes taken out", () => {
  assert.deepEqual(coloredLines(["\x1b[31mred", "still\x1b[0m plain"]), [
    { text: "red", html: '<span class="t-f1">red</span>' },
    { text: "still plain", html: '<span class="t-f1">still</span> plain' },
  ]);
});

test("a screen line is drawn a run a style, the cursor's cell a run of its own", () => {
  const line = lineOf(3);
  line.cells[0] = ["a", PLAIN];
  line.cells[1] = ["b", PLAIN];
  assert.equal(lineHtml(line, 1), 'a<span class="t-cursor">b</span> ');
  assert.equal(lineHtml(line, -1), "ab ");
  assert.deepEqual(blank(restyled(PLAIN, [41]))[1].bg, 1, "a blank cell retains the background it was erased in");
  assert.equal(blank(restyled(PLAIN, [1]))[1], PLAIN, "and nothing else");
});
