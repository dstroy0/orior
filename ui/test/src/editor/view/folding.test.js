// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's folds, as orior's side reads them off the regions the grammar's roles open and close.

import { invoke } from "../../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../../gui/src-js/edit.js";
import { showView } from "../../../../gui/src-js/views.js";
import { given, press, putBackAsFound } from "../../../menus.js";

const { test, assert, ui } = window.__harness;

test("Fold All closes every region orior's side folds and Unfold All opens them again", async () => {
  await given("def f():\n    return 1\n\ndef g():\n    return 2\n|");
  const folds = () => [...editing().editor.s.highlight.folds];
  await ui.waitFor(() => folds().length === 2, 5000, "the folds of both functions from orior's side").catch((error) => {
    throw new Error(`${error.message}: ${JSON.stringify(folds())}`);
  });
  assert.deepEqual(folds(), [[0, [1, false]], [3, [4, false]]], "each function's body, the blank line after it left out");
  await press("fold-all");
  assert.equal(editing().editor.s.folded.size, 2, "both functions closed");
  await press("unfold-all");
  assert.equal(editing().editor.s.folded.size, 0);
});

test("a bracket's fold shows its closing line on the line it opens on", async () => {
  const path = "folds_given.js";
  await invoke("file_write", { path, text: "function f() {\n  return 1;\n}\n" });
  showView("edit");
  await openFile(path);
  try {
    await ui.waitFor(() => editing().active === path && editing().editor?.s, 5000, "the file's editor");
    const view = editing().editor;
    await ui.waitFor(() => view.s.highlight.folds.size === 1, 5000, "the function's fold from orior's side");
    assert.deepEqual([...view.s.highlight.folds], [[0, [2, true]]]);
    view.toggleFold(0);
    assert.ok(view.s.shows(0), "the closing brace drawn after the fold's mark");
    view.toggleFold(0);
  } finally {
    if (editing().active === path) {
      await press("close-editor");
    }
  }
});

test("the window is put back as the file found it", putBackAsFound);
