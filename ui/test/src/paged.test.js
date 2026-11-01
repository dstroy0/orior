// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A file of hundreds of megabytes stored as pages: huge.log, three million numbered lines the harness
// makes and puts under bench/, opened at once, the page holding only a window of it about the view,
// the window moved as the view scrolls and put at a line far down the file, and an edit saved into
// the file and read back. A view whose lines wrap is placed by its rows.

import { onCalls } from "../../gui/src-js/bridge.js";
import { editing, openAt, openFile } from "../../gui/src-js/edit.js";
import { pos } from "../../gui/src-js/editor/document.js";
import { caret, WRAP_KEY } from "../../gui/src-js/editor/view.js";
import { settled } from "../../gui/src-js/paged.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const BENCH = window.__harness.servers.ORIOR_TEST_BENCH;
const LINES = 3000000;
const MOST = 1000;
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
  assert.ok(s.window?.paged, "stored as pages");
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
    await ui.waitFor(() => settled(s), 5000);
    await new Promise((rest) => requestAnimationFrame(() => setTimeout(rest, 30)));
  }
  await ui.waitFor(() => settled(s), 5000);
  const first = s.base + Math.floor(view.scrollY() / view.lineHeight);
  assert.ok(s.base > start, `the window moved down: from ${start} to ${s.base}`);
  assert.ok(s.doc.count <= MOST, `the page holds ${s.doc.count} lines`);
  assert.equal(s.doc.line(first - s.base), lineOf(first), `the line at the top of the view is the file's ${first}`);
  if (before !== null) {
    const grew = (heap() - before) / 1024 / 1024;
    assert.ok(grew < 16, `the page took ${Math.round(grew)} MB more for a file of 300 MB`);
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

test("a paged file's lines come with their colors: no frame draws a line in view uncolored, as it opens, jumps or scrolls", async () => {
  // large-file.json, a file of 26 MB in 11351 long lines, opens paged
  const path = `${BENCH}/large-file.json`;
  showView("edit");
  // Each frame from before the open, the rows on screen that hold text and no token of their colors: every line of
  // the file holds tokens, so a row without one was drawn before its colors came.
  const plain = [];
  let watching = true;
  const watch = () => {
    const host = editing().active === path ? editing().editor?.host : null;
    const drawn = host ? [...host.querySelectorAll(".ed-row")].filter((row) => row.textContent && row.getBoundingClientRect().bottom > 0 && row.getBoundingClientRect().top < innerHeight) : [];
    const bare = drawn.filter((row) => !row.querySelector('span[class^="t-"]')).length;
    if (bare) {
      plain.push(bare);
    }
    if (watching) {
      requestAnimationFrame(watch);
    }
  };
  requestAnimationFrame(watch);
  await openFile(path);
  await ui.waitFor(() => editing().active === path, 10000);
  const view = editing().editor;
  const s = view.s;
  assert.ok(s.window?.paged, "stored as pages");
  // the page holds the lines near the view, a few megabytes of the file's 26 at most
  const units = () => s.doc.lines.reduce((sum, line) => sum + line.length, 0);
  assert.ok(s.doc.count <= MOST && units() < 3 << 20, `the page holds ${s.doc.count} lines, ${units()} units`);
  const colored = (line) => s.highlight.runsOf(line - s.base).some(([, cls]) => cls);
  // the file opens at the place it was last left, its lines in view colored as it opens
  const inView = () => s.base + Math.floor(view.scrollY() / view.lineHeight);
  assert.ok(colored(inView()), "the lines in view colored as the file opens");
  // a line far down: the window is put there with its colors
  await openAt(path, 11000, 0);
  assert.ok(colored(11000), "a line far down colored as the window is put there");
  assert.ok(s.highlight.runsOf(11000 - s.base).some(([, cls]) => cls.includes("t-string")), "its strings colored as strings");
  // scrolled up past the window's edge, step by step, the window reading on as it goes
  for (let step = 0; step < 8; step += 1) {
    view.scroller.scrollTop -= 150 * view.lineHeight;
    await ui.waitFor(() => settled(s), 5000);
    await new Promise((rest) => requestAnimationFrame(() => setTimeout(rest, 30)));
  }
  watching = false;
  assert.equal(plain.length, 0, `frames drew lines in view without their colors: ${plain.slice(0, 8).join(", ")} rows`);
});

test("the caret kept moving down a paged file goes on line by line past every window, never sent back", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 100, 0);
  const view = editing().editor;
  view.focus();
  // each press moves the caret a line down, the window moving under it as it goes
  const caretLine = () => view.s.base + view.head().line;
  const lines = [];
  for (let press = 0; press < 300; press += 1) {
    await ui.key("ArrowDown");
    lines.push(caretLine());
  }
  await ui.waitFor(() => settled(view.s), 5000).catch(() => {});
  const back = lines.findIndex((line, at) => at > 0 && line < lines[at - 1]);
  assert.ok(back < 0, `the caret went back from ${lines[back - 1]} to ${lines[back]} at press ${back}`);
  assert.equal(caretLine(), 400, "300 presses down from line 100");
});

test("a paged file dragged by its scrollbar end to end and back: each frame draws the lines in view, colored, none late", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 0, 0);
  const view = editing().editor;
  const scroller = view.scroller;
  const box = scroller.getBoundingClientRect();
  // the scrollbar down the scroller's right edge, its arrows at each end, the thumb under the top one
  const bar = scroller.offsetWidth - scroller.clientWidth;
  assert.ok(bar > 0, "the editor shows its scrollbar");
  const x = box.right - bar / 2;
  const topY = box.top + bar + 4;
  const bottomY = box.bottom - bar - 4;
  const rows = Math.floor(view.viewSize().height / view.lineHeight) - 1;
  // Each frame the view draws, read as it leaves it: rows in view drawn with text and no token of their colors, too few
  // rows of text for the view, and the frames in a row that kept the last lines drawn while the view stood past the
  // window, the most of them; and each frame's time since the frame before.
  const plain = [];
  const blank = [];
  const late = [];
  let pastFrames = 0;
  let pastFramesMost = 0;
  // each frame's line of the file at the top of the view and the top of the minimap's slider
  const slides = [];
  let watching = true;
  let last = performance.now();
  const drawnFrame = () => {
    slides.push([view.s.base + view.scrollYKnown() / view.lineHeight, view.minimap.geometry(view.lastTop).sliderTop]);
    const drawn = [...view.host.querySelectorAll(".ed-row")].filter((row) => row.textContent && row.getBoundingClientRect().bottom > box.top && row.getBoundingClientRect().top < box.bottom);
    const bare = drawn.filter((row) => !row.querySelector('span[class^="t-"]')).length;
    if (bare) {
      plain.push(bare);
    }
    if (drawn.length < rows) {
      blank.push(drawn.length);
    }
    pastFrames = view.pastWindowFrame === view.s && view.lastTop !== undefined && view.pastWindow(view.lastTop) ? pastFrames + 1 : 0;
    pastFramesMost = Math.max(pastFramesMost, pastFrames);
  };
  const painted = view.painted;
  view.painted = function (...given) {
    const result = painted.apply(this, given);
    if (watching) {
      drawnFrame();
    }
    return result;
  };
  const frameTimes = (now) => {
    if (now - last > 50) {
      late.push(Math.round(now - last));
    }
    last = now;
    if (watching) {
      requestAnimationFrame(frameTimes);
    }
  };
  requestAnimationFrame(frameTimes);
  // the browser's own account of each long frame: its script, its style and layout, and the rest of its drawing
  const long = [];
  const observer = new PerformanceObserver((list) => {
    for (const entry of list.getEntries()) {
      const end = entry.startTime + entry.duration;
      const layout = entry.styleAndLayoutStart ? end - entry.styleAndLayoutStart : 0;
      const render = entry.renderStart ? end - entry.renderStart - layout : 0;
      const scripts = entry.scripts.map((one) => `${one.sourceFunctionName || one.invoker} (${one.sourceURL.split("/").pop()}:${one.sourceCharPosition}) ${Math.round(one.duration)}`).join(" ");
      long.push(`${Math.round(entry.duration)} ms: script ${scripts || "none"}, style and layout ${Math.round(layout)}, drawing ${Math.round(render)}`);
    }
  });
  observer.observe({ type: "long-animation-frame" });
  try {
    await ui.drag({ x, y: topY }, { x, y: bottomY }, { steps: 60 });
    await ui.drag({ x, y: bottomY }, { x, y: topY }, { steps: 60 });
  } finally {
    watching = false;
    observer.disconnect();
    view.painted = painted;
  }
  // the minimap's slider goes the way the view goes from frame to frame, never back
  const back = slides.filter(([line, slider], at) => at > 0 && Math.sign(slider - slides[at - 1][1]) * Math.sign(line - slides[at - 1][0]) < 0 && Math.abs(slider - slides[at - 1][1]) > 1).length;
  // the view shows the lines it stands on within three frames of reaching them, all the way
  const told = `${plain.length} frames with rows uncolored, ${blank.length} with too few rows (${blank.slice(0, 6).join(", ")} of ${rows}), ${late.length} late (${late.slice(0, 6).join(", ")} ms; the long ones ${long.filter((one) => parseInt(one, 10) > 50).slice(0, 4).join("; ") || "none told"}), ${pastFramesMost} frames in a row kept, the slider back against the view in ${back}`;
  assert.ok(plain.length === 0 && blank.length === 0 && late.length === 0 && pastFramesMost <= 3 && back === 0, told);
});

test("a paged file's minimap at rest: every row of the map beside the view holds a line of the window", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 5000, 0);
  const view = editing().editor;
  const s = view.s;
  const total = view.linesInFile();
  // the lines of the file the map stands beside, as the map places itself about the view
  const span = () => {
    const { start, fit } = view.minimap.geometry(view.scroller.scrollTop);
    return [Math.max(0, s.base + start), Math.min(total, s.base + start + fit), fit];
  };
  const covered = () => {
    const [from, to] = span();
    return s.base <= from && s.base + s.doc.count >= to;
  };
  await ui.waitFor(() => settled(s) && covered(), 10000).catch(() => {});
  const [from, to, fit] = span();
  assert.ok(fit > 50, `the map holds ${fit} rows`);
  assert.ok(covered(), `the map stands beside lines ${from} to ${to}, the window holds ${s.base} to ${s.base + s.doc.count}`);
  assert.ok(s.doc.count <= Math.max(MOST, fit + 2 * 100 + 64), `the page holds ${s.doc.count} lines`);
});

test("a paged file's reads wait for the edits sent before them and not for one another, so a report of where its view stands is never kept behind them", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 1000, 0);
  const view = editing().editor;
  const s = view.s;
  await ui.waitFor(() => settled(s), 10000).catch(() => {});
  // the bracket pair at forty places read at once, as a caret kept moving asks for the pair at each line it reaches
  let answered = 0;
  const pairs = Array.from({ length: 40 }, (_, line) => view.pairOf(s, { line, col: 0 }).finally(() => {
    answered += 1;
  }));
  await s.window.paged.chain;
  const behind = answered;
  await Promise.all(pairs);
  assert.ok(behind < 10, `the calls a report waits for behind it behind ${behind} of the 40 reads`);
});

test("a paged file's caret moved from place to place asks for the bracket pair at one place at a time, and at the place it stands once that answer comes", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 1000, 0);
  const view = editing().editor;
  const s = view.s;
  await ui.waitFor(() => settled(s), 10000).catch(() => {});
  let open = 0;
  let most = 0;
  let watching = true;
  onCalls((command, args, promise) => {
    if (watching && command === "branch_pair") {
      open += 1;
      most = Math.max(most, open);
      promise.finally(() => {
        open -= 1;
      }).catch(() => {});
    }
  });
  try {
    for (let step = 1; step <= 40; step += 1) {
      view.select([caret(pos(1000 - s.base + step, 0))]);
      view.paint();
    }
    await ui.waitFor(() => open === 0 && settled(s), 10000);
  } finally {
    watching = false;
  }
  assert.equal(most, 1, `${most} pairs asked at once`);
  const asked = await view.pairOf(s, view.primary().head);
  assert.deepEqual(view.bracketPair(), asked?.map((at) => pos(at.line - s.base, at.col)) ?? null, "the pair drawn the one at the place the caret stands");
});

test("a paged file whose lines wrap is placed by its rows: each report tells the width a row holds, no answer holds more than its rows take, and the view's top line is the one its top row shows", async () => {
  const path = `${BENCH}/large-file.json`;
  await openAt(path, 2000, 0);
  const view = editing().editor;
  const s = view.s;
  localStorage.setItem(WRAP_KEY, "true");
  view.takeSettings();
  // each report and the units of the lines its answer holds
  const answers = [];
  let watching = true;
  onCalls((command, args, promise) => {
    if (watching && command === "pages_view") {
      promise.then((answer) => answers.push([args.width, (answer?.lines ?? []).reduce((sum, line) => sum + line.length, 0)]), () => {});
    }
  });
  try {
    const scroller = view.scroller;
    const box = scroller.getBoundingClientRect();
    const bar = scroller.offsetWidth - scroller.clientWidth;
    const x = box.right - bar / 2;
    await ui.drag({ x, y: box.top + bar + 4 }, { x, y: box.bottom - bar - 4 }, { steps: 30 });
    await ui.drag({ x, y: box.bottom - bar - 4 }, { x, y: box.top + box.height / 2 }, { steps: 30 });
    await ui.waitFor(() => settled(s), 10000);
  } finally {
    watching = false;
  }
  const row = Math.floor(view.scrollY() / view.lineHeight);
  const rows = view.rows();
  const shown = row >= 0 && row < rows.size ? s.base + rows.lineOf(row) : null;
  const told = view.s.window.paged.told;
  localStorage.setItem(WRAP_KEY, "false");
  view.takeSettings();
  const most = Math.max(0, ...answers.map(([, units]) => units));
  assert.ok(answers.length > 0 && answers.every(([width]) => width > 0), `${answers.length} reports, each telling the width its rows hold`);
  assert.ok(most < 256 * 1024, `the largest answer holds ${most} units of its lines`);
  assert.equal(told, shown, "the line told as the view's top is the one its top row shows");
});
