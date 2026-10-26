// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A screen drawn into its view: its lines drawn as they change with the cursor on its cell, the focus marked, and a
// new size, the lines cut or filled out and a shorter screen keeping the lines that leave its top.

import { Screen } from "../../../gui/src-js/screen.js";

const { test, assert } = window.__harness;

async function withScreen(cols, rows, work) {
  const view = document.createElement("div");
  view.style.cssText = "position:fixed;left:0;top:0;width:400px;height:200px;overflow:auto";
  document.body.append(view);
  try {
    return await work(new Screen(view, cols, rows, () => {}), view);
  } finally {
    view.remove();
  }
}

const textOf = (screen, y) => screen.buffer.lines[y].cells.map(([char]) => char).join("").trimEnd();

test("a draw puts each changed line in its row, the cursor marked on its cell, and the focus marks the view", async () => {
  await withScreen(8, 2, (screen, view) => {
    screen.write("hi");
    screen.draw();
    const row = screen.buffer.rows[0];
    assert.equal(row.textContent.trimEnd(), "hi");
    assert.equal(row.querySelector(".t-cursor")?.textContent, " ", "the cursor stands after what was printed");
    screen.setFocus(true);
    assert.ok(view.classList.contains("term-focused"));
    screen.setFocus(false);
    assert.ok(!view.classList.contains("term-focused"));
  });
});

test("a new size cuts or fills out each line, and a shorter screen keeps the lines that leave its top", async () => {
  await withScreen(8, 4, (screen) => {
    screen.write("one\r\ntwo\r\nthree\r\nfour");
    screen.resize(4, 4);
    assert.equal(textOf(screen, 2), "thre", "cut to the new width");
    assert.equal(screen.buffer.lines[0].cells.length, 4);
    screen.resize(4, 2);
    assert.deepEqual([textOf(screen, 0), textOf(screen, 1)], ["thre", "four"], "the cursor's line kept in sight");
    assert.equal(screen.y, 1);
    assert.deepEqual([...screen.tail.children].map((row) => row.textContent.trim()), ["one", "two"]);
    screen.resize(6, 3);
    assert.equal(screen.buffer.lines[2].cells.length, 6, "a line added at the foot, as wide as the screen");
  });
});
