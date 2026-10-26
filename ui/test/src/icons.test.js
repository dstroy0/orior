// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The app's icons: each a shape drawn on a 16-unit square, its filled parts as dots, and a letter in its place where
// the name is one character.

import { icon } from "../../gui/src-js/icons.js";

const { test, assert } = window.__harness;

test("an icon is an SVG of its shape on a 16-unit square, hidden from a reader of the page's text", () => {
  const folder = icon("folder");
  assert.equal(folder.tagName.toLowerCase(), "svg");
  assert.equal(folder.getAttribute("viewBox"), "0 0 16 16");
  assert.equal(folder.getAttribute("aria-hidden"), "true");
  assert.equal(folder.getAttribute("class"), "glyph glyph-folder");
  assert.ok(folder.querySelectorAll("path").length > 0, "drawn in lines");
});

test("a shape's filled parts are dots, and a name with no shape draws nothing", () => {
  const view = icon("view");
  assert.deepEqual([...view.querySelectorAll("circle.fill")].map((dot) => [dot.getAttribute("cx"), dot.getAttribute("cy"), dot.getAttribute("r")]), [["8", "8", "1.9"]]);
  assert.equal(icon("no-such-icon").childElementCount, 0);
});

test("a one-letter name is the letter itself", () => {
  const letter = icon("m");
  assert.equal(letter.tagName, "SPAN");
  assert.equal(letter.className, "icon-letter");
  assert.equal(letter.textContent, "m");
});
