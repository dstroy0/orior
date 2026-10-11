// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view: how far ahead of a view at rest its lines are colored, what it holds back
// while it scrolls fast, and text that runs right to left placed where the page draws it.

import { coloringReach, runsRightToLeft } from "../../../src/editor/view.js";
import { invoke } from "../../../src/bridge.js";
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

test("each character runs as its script does, and a space between two of a right-to-left script runs with them", () => {
  assert.deepEqual(runsRightToLeft("ab שלום cd"), [false, false, false, true, true, true, true, false, false, false]);
  assert.deepEqual(runsRightToLeft("שלום עולם"), [true, true, true, true, true, true, true, true, true]);
  assert.deepEqual(runsRightToLeft("a 1"), [false, false, false]);
});

test("on a line where Hebrew meets text that runs left to right, the caret goes by the order the text is read in and stands where the page draws it", async () => {
  await invoke("file_write", { path: "bidi.txt", text: "abc שלום def\nمرحبًا بك\n" });
  showView("edit");
  await openFile("bidi.txt");
  await ui.waitFor(() => editing().active === "bidi.txt" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.paint();
  const x = (col) => view.xOf({ line: 0, col });
  assert.ok(x(4) > x(5) && x(5) > x(6) && x(6) > x(7), `inside the Hebrew word the caret steps to the left as the reading goes on: ${[4, 5, 6, 7].map(x)}`);
  assert.ok(x(3) < x(7) && x(9) > x(4), "the words that run left to right stand on either side of it");
  view.goTo(0, 4);
  view.focus();
  await ui.key("Right");
  assert.equal(view.primary().head.col, 5, "the arrow goes on to the next character as it is read");
  const node = view.rowNode(0);
  const shin = view.charBox(node, 4);
  const pressed = view.posAt({ clientX: shin.right - 1, clientY: shin.top + shin.height / 2 });
  assert.deepEqual([pressed.line, pressed.col], [0, 4], "a press on the first half of a letter, as it is read, lands before it");
  const later = view.posAt({ clientX: shin.left + 1, clientY: shin.top + shin.height / 2 });
  assert.equal(later.col, 5, "and on its second half, after it");
  view.setSelections([{ anchor: { line: 0, col: 4 }, head: { line: 0, col: 6 }, goal: null }]);
  view.paint();
  const path = view.picked.querySelector("path")?.getAttribute("d") ?? "";
  const xs = [...path.matchAll(/[ML]\s*([\d.]+)/g)].map((one) => Number(one[1]));
  assert.ok(xs.length && Math.min(...xs) >= x(6) - 2 && Math.max(...xs) <= x(4) + 2, `the selection covers the two letters where they are drawn: ${xs} against ${x(6)}..${x(4)}`);
});

test("the word keys stop at the ends of words of either script, a letter's marks part of its word", async () => {
  await openFile("bidi.txt");
  await ui.waitFor(() => editing().active === "bidi.txt", 5000);
  const view = editing().editor;
  view.goTo(1, 0);
  view.focus();
  await ui.key("Ctrl+Right");
  assert.equal(view.primary().head.col, 6, "past the fathatan, to the end of the word");
  await ui.key("Ctrl+Right");
  await ui.key("Ctrl+Left");
  assert.equal(view.primary().head.col, 7, "back to the start of the next word");
});
