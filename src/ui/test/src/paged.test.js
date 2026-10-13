// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A file of hundreds of megabytes held as pages: huge.log, three million numbered lines the harness
// makes and puts under bench/, opened at once, the page holding only a window of it about the view,
// the window moved as the view scrolls and put at a line far down the file, and an edit saved into
// the file and read back.

import { editing, openAt, openFile } from "../../src/edit.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const BENCH = window.__harness.servers.ORIOR_TEST_BENCH;
const LINES = 3000000;
const MOST = 120000;
const lineOf = (n) => `${String(n).padStart(9, "0")} a line of the paged file, the same width as every other, read from anywhere in it`.padEnd(99);

test("a file of hundreds of megabytes opens at once as pages, the page holding only a window of its lines", async () => {
  assert.ok(BENCH, "the harness made the file");
  const path = `${BENCH}/huge.log`;
  showView("edit");
  // The page's own memory, where the engine says it, as WebView2 does.
  const heap = () => performance.memory?.usedJSHeapSize ?? null;
  const before = heap();
  const began = performance.now();
  await openFile(path);
  await ui.waitFor(() => editing().active === path, 10000);
  const opened = performance.now() - began;
  const view = editing().editor;
  const s = view.s;
  assert.ok(s.window?.paged, "held as pages");
  assert.ok(opened < 1500, `in sight in ${Math.round(opened)} ms`);
  assert.equal(s.doc.line(0 - s.base), lineOf(0));
  assert.ok(s.doc.count <= MOST, `the page holds ${s.doc.count} lines`);
  await ui.waitFor(() => view.linesInFile() === LINES + 1, 30000);
  // A line far down the file: the window goes there.
  await openAt(path, 2500000, 3);
  assert.ok(s.base <= 2500000 && 2500000 < s.base + s.doc.count, `the window holds the line: ${s.base} to ${s.base + s.doc.count}`);
  assert.equal(s.doc.line(2500000 - s.base), lineOf(2500000));
  assert.equal(view.head().line + s.base, 2500000, "the cursor on it");
  // Scrolled down past the window's end, the window reads on and lets go above, never holding more
  // than MOST lines.
  const start = s.base;
  for (let step = 0; step < 12; step += 1) {
    view.scroller.scrollTop += 15000 * view.lineHeight;
    await ui.waitFor(() => !s.window.paged.moving, 5000);
    await new Promise((rest) => requestAnimationFrame(() => setTimeout(rest, 30)));
  }
  await ui.waitFor(() => !s.window.paged.moving, 5000);
  const first = s.base + Math.floor(view.scrollY() / view.lineHeight);
  assert.ok(s.base > start, `the window moved down: from ${start} to ${s.base}`);
  assert.ok(s.doc.count <= MOST, `the page holds ${s.doc.count} lines`);
  assert.equal(s.doc.line(first - s.base), lineOf(first), `the line at the top of the view is the file's ${first}`);
  if (before !== null) {
    const grew = (heap() - before) / 1024 / 1024;
    assert.ok(grew < 64, `the page took ${Math.round(grew)} MB more for a file of 300 MB`);
  }
});

test("an edit to a paged file is saved into the file, the rest of it as it was", async () => {
  const path = `${BENCH}/huge.log`;
  const view = editing().editor;
  const s = view.s;
  await openAt(path, 1000, 0);
  view.focus();
  await ui.type("EDITED ");
  assert.equal(s.doc.line(1000 - s.base), `EDITED ${lineOf(1000)}`);
  assert.ok(editing().activeChanged, "the tab has a change not saved");
  await ui.key("Ctrl+S");
  await ui.waitFor(() => !editing().activeChanged, 30000);
  await openAt(path, 2999999, 0);
  assert.equal(s.doc.line(2999999 - s.base), lineOf(2999999), "the file's last line as it was");
  await openAt(path, 1000, 0);
  assert.equal(s.doc.line(1000 - s.base), `EDITED ${lineOf(1000)}`, "the edit read back from the file");
  assert.equal(s.doc.line(999 - s.base), lineOf(999));
});
