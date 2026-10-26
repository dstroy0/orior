// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The refactorings on the file in the editor last pressed in: docstrings, sorted methods, Change Signature, Move and Search Structurally.

import { editing } from "../../../gui/src-js/edit.js";
import { given, press, putBackAsFound, sheetShown } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Edit, Docstring writes one in the form chosen, and Sort Methods sorts a class's methods", async () => {
  const was = editing().docstringForm();
  await press("docstring-form", "numpy");
  assert.equal(editing().docstringForm(), "numpy");
  await press("docstring-form", "google");
  try {
    await given("def add(a, b):\n    return a| + b\n");
    await press("docstring");
    await ui.waitFor(() => editing().editor.doc.text().includes('"""'), 5000, "the docstring");
    assert.match(editing().editor.doc.text(), /Args:/);
    await given("class C:\n    def b(self):\n        pass|\n\n    def a(self):\n        pass\n");
    await press("sort-methods");
    await ui.waitFor(() => editing().editor.doc.text().indexOf("def a") < editing().editor.doc.text().indexOf("def b"), 5000, "the methods sorted");
  } finally {
    editing().setDocstringForm(was);
  }
});

test("Edit, Change Signature, Move and Search Structurally ask in their sheets", async () => {
  await given("def f|(a, b):\n    return a + b\n\nf(1, 2)\n");
  await press("change-signature");
  await sheetShown("*");
  await given("def f|(a, b):\n    return a + b\n");
  await press("move-declaration");
  await sheetShown("*");
  await given("x = 1|\n");
  await press("shape-search");
  await sheetShown("*");
});

test("the window is put back as the file found it", putBackAsFound);
