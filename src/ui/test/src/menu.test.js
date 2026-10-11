// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Menus: each grows out of the point it was opened from, within the tenth of a second a press's
// answer keeps to.

import { closeMenu, GROWS, showMenu } from "../../src/menu.js";

const { test, assert } = window.__harness;

const items = [
  { label: "One", run: () => {} },
  { label: "Two", run: () => {} },
];

test("a menu grows out of the point it was opened from", () => {
  const menu = showMenu(300, 200, items);
  try {
    const growing = menu.getAnimations()[0];
    assert.ok(growing, "it moves as it opens");
    assert.equal(growing.effect.getComputedTiming().duration, GROWS);
    assert.ok(GROWS <= 100, "within a tenth of a second");
    const [x, y] = getComputedStyle(menu).transformOrigin.split(" ").map(parseFloat);
    assert.ok(Math.abs(parseFloat(menu.style.left) + x - 300) <= 1 && Math.abs(parseFloat(menu.style.top) + y - 200) <= 1, `from the point pressed: ${x}, ${y}`);
  } finally {
    closeMenu(false);
  }
});

test("a menu pushed back from the window's edge grows from the edge nearest the point", () => {
  const menu = showMenu(window.innerWidth - 2, 200, items);
  try {
    const [x] = getComputedStyle(menu).transformOrigin.split(" ").map(parseFloat);
    assert.ok(x >= menu.offsetWidth - 1, `from its right edge, under the point: ${x} of ${menu.offsetWidth}`);
  } finally {
    closeMenu(false);
  }
});
