// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The escape sequences' work on a screen: what it answers the shell, erasing, a scroll region, lines put in, the
// second screen, and the colors and weight of what is printed next.

import { Screen } from "../../../gui/src-js/screen.js";

const { test, assert } = window.__harness;

async function withScreen(cols, rows, work) {
  const view = document.createElement("div");
  view.style.cssText = "position:fixed;left:0;top:0;width:400px;height:200px;overflow:auto";
  document.body.append(view);
  const said = [];
  try {
    return await work(new Screen(view, cols, rows, (text) => said.push(text)), said);
  } finally {
    view.remove();
  }
}

const textOf = (screen, y) => screen.buffer.lines[y].cells.map(([char]) => char).join("").trimEnd();
const textsOf = (screen) => screen.buffer.lines.map((_, y) => textOf(screen, y));

test("asked where its cursor is and what it is, the screen answers the shell", async () => {
  await withScreen(20, 4, (screen, said) => {
    screen.write("ab\r\ncd\x1b[6n\x1b[c\x1b[5n");
    assert.deepEqual(said, ["\x1b[2;3R", "\x1b[?1;2c", "\x1b[0n"]);
  });
});

test("erasing: the rest of a line, its start, the whole of it, and the whole screen", async () => {
  await withScreen(10, 3, (screen) => {
    screen.write("abcdefgh\x1b[1;4H\x1b[K");
    assert.equal(textOf(screen, 0), "abc");
    screen.write("\x1b[1;2H\x1b[1K");
    assert.equal(textOf(screen, 0), "  c");
    screen.write("\x1b[2K");
    assert.equal(textOf(screen, 0), "");
    screen.write("x\r\ny\r\nz\x1b[2J");
    assert.deepEqual(textsOf(screen), ["", "", ""]);
  });
});

test("a scroll region scrolls within itself only, and a line put in pushes the region's lines down", async () => {
  await withScreen(10, 4, (screen) => {
    screen.write("top\r\none\r\ntwo\r\nfoot");
    screen.write("\x1b[2;3r\x1b[3;1H\n");
    assert.deepEqual(textsOf(screen), ["top", "two", "", "foot"], "a line feed at the region's foot scrolls the region alone");
    screen.write("\x1b[r\x1b[2;1H\x1b[L");
    assert.deepEqual(textsOf(screen), ["top", "", "two", ""], "the region is the whole screen again, the last line pushed off");
  });
});

test("the second screen is drawn on apart, and leaving it gives back the main screen's text and cursor", async () => {
  await withScreen(10, 3, (screen) => {
    screen.write("main\r\nretained");
    screen.write("\x1b[?1049h");
    assert.notEqual(screen.buffer, screen.main);
    assert.deepEqual(textsOf(screen), ["", "", ""]);
    screen.write("\x1b[1;1Hfull screen");
    screen.write("\x1b[?1049l");
    assert.equal(screen.buffer, screen.main);
    assert.deepEqual(textsOf(screen), ["main", "retained", ""]);
    assert.deepEqual([screen.x, screen.y], [4, 1]);
  });
});

test("a style set by SGR is printed in, and a reset goes back to plain", async () => {
  await withScreen(10, 2, (screen) => {
    screen.write("\x1b[1;31mR\x1b[0mp\x1b[38;5;200mc");
    const [red, plain, cube] = screen.buffer.lines[0].cells;
    assert.deepEqual([red[1].bold, red[1].fg], [true, 1]);
    assert.deepEqual([plain[1].bold, plain[1].fg], [false, null]);
    assert.equal(cube[1].fg, 200);
  });
});
