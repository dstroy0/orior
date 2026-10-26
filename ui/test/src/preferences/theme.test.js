// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences' color theme: a color as the swatch takes it and given back with its alpha, the palette's groups, and
// a theme of the reader's made, renamed, colored, given an image's share and chosen by its name or the scheme's.

import { BASE_NAMES, chooseTheme, groupOf, newTheme, renameTheme, setColor, shareImage, shownTheme, swatchOf, withAlpha } from "../../../gui/src-js/preferences/theme.js";
import { scheme } from "../../../gui/src-js/scheme.js";
import { chosen, deleteTheme, paletteOf, themes } from "../../../gui/src-js/themes.js";
import { putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

test("a color as the swatch takes it, #rrggbb with the alpha it cannot show, and given back with that alpha", () => {
  assert.deepEqual(swatchOf("#FF8800"), { hex: "#ff8800", alpha: 1 });
  assert.deepEqual(swatchOf("red"), { hex: "#ff0000", alpha: 1 });
  assert.deepEqual(swatchOf("rgba(0, 128, 255, 0.5)"), { hex: "#0080ff", alpha: 0.5 });
  assert.equal(withAlpha("#0080ff", 0.5), "rgba(0, 128, 255, 0.5)");
  assert.equal(withAlpha("#0080ff", 1), "#0080ff");
  assert.equal(withAlpha("#000000", 1 / 3), "rgba(0, 0, 0, 0.333)");
});

test("the palette's names fall in groups by how they start", () => {
  assert.equal(groupOf("--t7"), "--t0 … --t15");
  assert.equal(groupOf("--t-comment"), "--t-*");
  assert.equal(groupOf("--ed-bg"), "--ed-*");
  assert.equal(groupOf("--accent"), "--*");
});

test("a theme made from the one shown is renamed, colored, given an image's share, and the scheme's own chosen again", () => {
  const base = scheme();
  const before = new Set(themes().map((one) => one.name));
  try {
    chooseTheme(base);
    assert.equal(shownTheme(), null, "the scheme's own colors");
    newTheme();
    const made = shownTheme();
    assert.match(made.name, new RegExp(`^${BASE_NAMES[base]} \\d+$`), "named for the scheme, numbered past any taken");
    assert.equal(chosen(base), made.name);
    renameTheme("Theme of the test");
    assert.equal(shownTheme().name, "Theme of the test");
    const own = paletteOf(base).get("--ed-bg");
    assert.deepEqual(setColor("--ed-bg", "#123456"), { "--ed-bg": "#123456" });
    assert.deepEqual(setColor("--ed-bg", own), {}, "the scheme's own color is no color of the theme's");
    shareImage(0.3);
    assert.equal(shownTheme().image.opacity, 0.3);
    assert.ok(!chooseTheme("No such theme"), "a name no theme has chooses nothing");
    assert.ok(chooseTheme(base));
    assert.equal(shownTheme(), null);
    assert.ok(chooseTheme("Theme of the test"));
    assert.equal(chosen(base), "Theme of the test");
  } finally {
    chooseTheme(base);
    for (const one of themes().filter((theme) => !before.has(theme.name))) {
      deleteTheme(one.name);
    }
  }
});

test("the window is put back as the file found it", putBackAsFound);
