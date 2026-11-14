// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal's screen read the way an xterm reads its output: characters printed at the cursor, the cursor moved
// by the control characters, a full line wrapping to the next, a line scrolled off the top retained, and the folder the
// shell is in heard.

import { Screen } from "../../gui/src-js/screen.js";

const { test, assert } = window.__harness;

// A screen of `cols` by `rows` in a view of its own on the page while `work` runs, and what it said back.
async function withScreen(cols, rows, work) {
  const view = Object.assign(document.createElement("div"), { className: "term-view" });
  view.style.cssText = "position:fixed;left:0;top:0;width:400px;height:200px;overflow:auto";
  document.body.append(view);
  const said = [];
  try {
    return await work(new Screen(view, cols, rows, (text) => said.push(text)), said, view);
  } finally {
    view.remove();
  }
}

const textOf = (screen, y) => screen.buffer.lines[y].cells.map(([char]) => char).join("").trimEnd();

test("characters land at the cursor, a carriage return goes to the line's start and a line feed down", async () => {
  await withScreen(20, 4, (screen) => {
    screen.write("hello\r\nworld");
    assert.deepEqual([textOf(screen, 0), textOf(screen, 1)], ["hello", "world"]);
    assert.deepEqual([screen.x, screen.y], [5, 1]);
    screen.write("\rW");
    assert.equal(textOf(screen, 1), "World", "printed over from the line's start");
  });
});

test("a full line wraps to the next, a tab goes to the next stop of eight, and a backspace steps back", async () => {
  await withScreen(10, 4, (screen) => {
    screen.write("abcdefghijKL");
    assert.deepEqual([textOf(screen, 0), textOf(screen, 1)], ["abcdefghij", "KL"]);
    screen.write("\r\na\tb");
    assert.equal(textOf(screen, 2), "a       b");
    screen.write("\b\bX");
    assert.equal(textOf(screen, 2), "a      Xb");
  });
});

test("a line scrolled off the top of the main screen is retained above it with its text", async () => {
  await withScreen(10, 2, (screen) => {
    screen.write("one\r\ntwo\r\nthree");
    assert.deepEqual([textOf(screen, 0), textOf(screen, 1)], ["two", "three"]);
    assert.equal(screen.tail.childElementCount, 1);
    assert.equal(screen.tail.firstElementChild.textContent.trim(), "one");
  });
});

test("the folder the shell is in, as OSC 7 says it, is heard, and every other command is read past", async () => {
  await withScreen(20, 2, (screen) => {
    const places = [];
    screen.onPlace = (place) => places.push(place);
    screen.write("\x1b]7;file:///home/me/tree\x07\x1b]0;a title\x1b\\ok");
    assert.deepEqual(places, ["file:///home/me/tree"]);
    assert.equal(textOf(screen, 0), "ok");
  });
});
