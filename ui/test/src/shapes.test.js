// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Search Structurally's sheet: its pattern, template and where, retained by language; the matches listed, each going to
// its place; and every match written again from the template.

import { shapeSearch } from "../../gui/src-js/shapes.js";
import { sheet } from "../../gui/src-js/menubar.js";
import { retentionStorage } from "../helpers.js";

const { test, assert, ui, calls } = window.__harness;

const RETAINED = "orior.shape-search";
const FOUND = { found: [{ path: "a.py", from: { line: 2, col: 4 }, line: "    x = old(1)" }, { path: "b.py", from: { line: 0, col: 0 }, line: "old(2)" }], files: [{ path: "a.py" }, { path: "b.py" }] };

// The sheet displayed for a Python file, what it opened and applied, while `work` runs.
async function withSheet(work) {
  const opened = [];
  const applied = [];
  let dialog = null;
  shapeSearch({
    sheet: (body) => (dialog = sheet(body)),
    language: "python",
    languageName: "Python",
    path: "a.py",
    open: (...place) => opened.push(place),
    apply: async (files) => applied.push(files),
  });
  const field = (label) => dialog.querySelector(`[aria-label="${label}"]`);
  try {
    return await work({ dialog, field, opened, applied });
  } finally {
    dialog.close();
  }
}

test("the sheet asks for a pattern, a template and where, and retains what was asked by language", async () => {
  await retentionStorage([RETAINED], async () => {
    localStorage.setItem(RETAINED, JSON.stringify({ python: { pattern: "old($x$)", template: "new($x$)", scope: "tree" } }));
    await withSheet(({ field }) => {
      assert.equal(field("Pattern").value, "old($x$)");
      assert.equal(field("Replace with").value, "new($x$)");
      assert.equal(field("Where").value, "tree");
      assert.deepEqual([...field("Where").options].map((one) => one.textContent), ["In a.py", "In every Python file of the tree"]);
    });
  });
});

test("an empty pattern is asked for, and the matches found are listed, each going to its place", async () => {
  await retentionStorage([RETAINED], async () => {
    localStorage.removeItem(RETAINED);
    calls.answer("shape_search", () => FOUND);
    await withSheet(async ({ dialog, field, opened }) => {
      await ui.click(dialog.querySelector('button[type="submit"]'));
      await ui.waitFor(() => /Write the code to find/.test(dialog.querySelector(".shape-said").textContent), 3000);
      field("Pattern").value = "old($x$)";
      await ui.click(dialog.querySelector('button[type="submit"]'));
      await ui.waitFor(() => dialog.querySelectorAll(".shape-row").length === 2, 3000, "the matches listed");
      assert.equal(dialog.querySelector(".shape-said").textContent, "2 matches in 2 files.");
      assert.equal(calls.made.filter((one) => one.name === "shape_search").at(-1).args.path, "a.py", "searched in the file alone");
      await ui.click(dialog.querySelector(".shape-row"));
      assert.deepEqual(opened, [["a.py", 2, 4]]);
      assert.deepEqual(JSON.parse(localStorage.getItem(RETAINED)).python.pattern, "old($x$)", "the pattern retained for Python");
    });
  });
});

test("Replace All writes every match again from the template, one step over every file", async () => {
  await retentionStorage([RETAINED], async () => {
    calls.answer("shape_search", ({ template }) => ({ ...FOUND, template }));
    await withSheet(async ({ dialog, field, applied }) => {
      field("Pattern").value = "old($x$)";
      field("Replace with").value = "new($x$)";
      await ui.click([...dialog.querySelectorAll("button")].find((one) => one.textContent === "Replace All"));
      await ui.waitFor(() => applied.length === 1, 3000, "the files applied");
      assert.deepEqual(applied[0], FOUND.files);
      assert.equal(calls.made.filter((one) => one.name === "shape_search").at(-1).args.template, "new($x$)");
    });
  });
});
