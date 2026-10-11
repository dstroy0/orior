// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cssOf, rgbOf, withAlpha } from "../../src/colors.js";
import { notifyScheme } from "../../src/scheme.js";

const { test, assert } = window.__harness;

const root = document.documentElement.style;

test("a variable's color is read as red, green and blue", () => {
  root.setProperty("--harness-color", "#102030");
  notifyScheme();
  try {
    assert.deepEqual(rgbOf("--harness-color"), [16, 32, 48]);
    assert.equal(cssOf("--harness-color"), "#102030");
  } finally {
    root.removeProperty("--harness-color");
  }
});

test("a color with alpha keeps its alpha", () => {
  root.setProperty("--harness-color", "rgba(1, 2, 3, 0.5)");
  notifyScheme();
  try {
    assert.deepEqual(rgbOf("--harness-color"), [1, 2, 3]);
    assert.match(cssOf("--harness-color"), /rgba\(1, 2, 3, 0\.5\)/);
  } finally {
    root.removeProperty("--harness-color");
  }
});

test("a variable that holds no color reads as black", () => {
  notifyScheme();
  assert.deepEqual(rgbOf("--harness-none"), [0, 0, 0]);
  root.setProperty("--harness-color", "not a color");
  notifyScheme();
  try {
    assert.deepEqual(rgbOf("--harness-color"), [0, 0, 0]);
  } finally {
    root.removeProperty("--harness-color");
  }
});

test("a color is kept until the scheme or a theme changes", () => {
  root.setProperty("--harness-color", "#010203");
  notifyScheme();
  try {
    assert.deepEqual(rgbOf("--harness-color"), [1, 2, 3]);
    root.setProperty("--harness-color", "#040506");
    assert.deepEqual(rgbOf("--harness-color"), [1, 2, 3], "kept");
    notifyScheme();
    assert.deepEqual(rgbOf("--harness-color"), [4, 5, 6], "read again");
  } finally {
    root.removeProperty("--harness-color");
  }
});

test("withAlpha gives the color at the alpha asked", () => {
  root.setProperty("--harness-color", "#ff8000");
  notifyScheme();
  try {
    assert.equal(withAlpha("--harness-color", 0.25), "rgba(255, 128, 0, 0.25)");
  } finally {
    root.removeProperty("--harness-color");
  }
});
