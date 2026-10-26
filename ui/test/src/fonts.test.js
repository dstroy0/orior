// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fonts the reader sets: each kept over the stylesheet's own, the code's size held between its least and most and
// its line height following it, and a value that could break out of its rule left out.

import { applyFonts, fontRule, fonts, FONTS, onFonts, setFont } from "../../gui/src-js/fonts.js";
import { keepingStorage } from "../helpers.js";

const { test, assert } = window.__harness;

test("the rule sets each font given, and the code's line height from its size", () => {
  assert.equal(fontRule({ "--code": "Fira Code, monospace", "--code-size": "13" }), ":root[data-scheme] { --code: Fira Code, monospace; --code-size: 13px; --code-line: 20px; }");
  assert.equal(fontRule({}), "");
});

test("a size outside its bounds and a value holding a brace or a semicolon are left out", () => {
  assert.equal(fontRule({ "--code-size": "64" }), "");
  assert.equal(fontRule({ "--code-size": "4" }), "");
  assert.equal(fontRule({ "--head": "x; } body { color: red" }), "");
});

test("a font set is kept and written into the page's rule, told to each listener, and an empty one gives the stylesheet's back", async () => {
  await keepingStorage([FONTS], () => {
    let told = 0;
    onFonts(() => (told += 1));
    setFont("--text", "Georgia, serif");
    assert.deepEqual(fonts(), { "--text": "Georgia, serif" });
    assert.match(document.getElementById("fonts").textContent, /--text: Georgia, serif;/);
    assert.equal(told, 1);
    setFont("--text", "  ");
    assert.deepEqual(fonts(), {});
    localStorage.setItem(FONTS, "{broken");
    assert.deepEqual(fonts(), {}, "a kept set that cannot be read is none");
  });
  applyFonts();
});
