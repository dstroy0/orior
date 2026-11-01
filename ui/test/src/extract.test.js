// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Extract Function: the lines selected made a function of their own, taking the names they read that were set before
// them and giving back the names read after them, called where they stood; a line's indent and its width; and a name
// for the function no other name of the file takes.

import { extractFunction, freshName, indentOf, widthOf } from "../../gui/src-js/extract.js";
import { given, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("a line's indent, and its width with a tab as wide as its size", () => {
  assert.equal(indentOf("\t  x = 1"), "\t  ");
  assert.equal(widthOf("\t  x = 1", 4), 6);
  assert.equal(widthOf("x", 4), 0);
});

test("the function's name is one the file's text does not use yet", () => {
  assert.equal(freshName("x = 1"), "extracted");
  assert.equal(freshName("extracted = 1\nextracted2()"), "extracted3");
  assert.equal(freshName("obj.extracted()"), "extracted", "a member of another object is no name of the file's own");
});

test("lines selected become a function taking what they read and giving back what is read after them, called in their place", async () => {
  const view = await given("def f(a):\n    [b = a * 2\n    c = b + 1]\n    return c\n");
  extractFunction(view);
  await ui.waitFor(() => (view.doc.text().match(/\bdef /g) ?? []).length === 2, 5000, "a function made of the lines");
  const text = view.doc.text();
  assert.match(text, /\n    c = extracted\(a\)\n    return c\n/, "the call where the lines stood");
  assert.match(text, /\ndef extracted\(a\):\n    b = a \* 2\n    c = b \+ 1\n    return c\n/, "the function after the one that kept them");
});

test("the window is put back as the file found it", putBackAsFound);
