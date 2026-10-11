// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view: how far ahead of a view at rest its lines are colored, and what it holds back
// while it scrolls fast.

import { coloringReach } from "../../../src/editor/view.js";
import { editing, openFile } from "../../../src/edit.js";
import { showView } from "../../../src/views.js";
import { write } from "../../../src/status.js";

const { test, assert, ui } = window.__harness;

test("a view that came to rest slowly is colored three screens ahead", () => {
  assert.equal(coloringReach(40, 0, 30), 120);
  assert.equal(coloringReach(40, 0.1, 30), 120);
});

test("a view that came to rest fast is colored as far as its speed carries it over two screens' coloring", () => {
  const fast = coloringReach(40, 100, 30);
  assert.ok(fast > 120, `farther than three screens: ${fast}`);
  const twice = coloringReach(40, 200, 30);
  assert.ok(Math.abs(twice - 2 * fast) <= 2, `twice the speed reaches twice as far: ${fast}, ${twice}`);
  assert.ok(Math.abs(coloringReach(40, 100, 60) - 2 * fast) <= 2, "twice the coloring time reaches twice as far");
});

test("while the view scrolls fast the gutter's change marks and the diagnostics wait, and come back as it slows", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().editor?.s, 5000);
  const view = editing().editor;
  const s = view.s;
  const root = view.gutter.closest(".ed") ?? view.gutter.parentElement;
  const drawn = () => ({ change: Boolean(view.gutter.querySelector(".ed-change")), diagnostic: Boolean(root.querySelector(".ed-diag")) });
  view.setChanges(s, { added: new Set([0]), changed: new Set(), removed: new Set() });
  s.diagnostics = [{ from: { line: 0, col: 0 }, to: { line: 0, col: 3 }, severity: 1, message: "a test's" }];
  try {
    view.paint();
    assert.deepEqual(drawn(), { change: true, diagnostic: true }, "at rest everything is drawn");
    write("scroll", { level: 2 });
    view.paint();
    assert.deepEqual(drawn(), { change: false, diagnostic: false }, "at speed they wait");
    assert.ok(view.gutter.querySelector(".ed-num-text"), "the numbers stay");
    write("scroll", { level: 0 });
    view.paint();
    assert.deepEqual(drawn(), { change: true, diagnostic: true }, "at rest again they come back");
  } finally {
    write("scroll", { level: 0 });
    s.diagnostics = [];
    view.setChanges(s, null);
    view.paint();
  }
});
