// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view scrolled: a fast scroll at the display's own rate, smooth scrolling under reduced motion, the
// even steps of a steady wheel, and a trackpad's fingers over plain and wrapped text.

import { smoothScrolling } from "../../../../gui/src-js/editor/view.js";
import { invoke } from "../../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../../gui/src-js/edit.js";
import { showView } from "../../../../gui/src-js/views.js";
import { joinRun, TRACE_BLOB } from "../../../../gui/src-js/trace.js";
import { framesOver, paintsOf, seen } from "../../../frames.js";

const { test, assert, ui } = window.__harness;

test("a fast scroll through a long file holds the display's own rate, as files save beside it", async () => {
  const lines = [];
  for (let line = 0; line < 20000; line += 1) {
    lines.push(`def step_${line}(value, other=${line}):  # a line of a long file, number ${line}`, `    return [value * other for _ in range(${line % 17})]`);
  }
  await invoke("file_write", { path: "rate.py", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("rate.py");
  await ui.waitFor(() => editing().active === "rate.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.goTo(0, 0);
  const rest = await framesOver(1000);
  const display = (rest.length - 1) / ((rest.at(-1) - rest[0]) / 1000);
  const box = view.scroller.getBoundingClientRect();
  const at = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
  const counting = framesOver(4000);
  const until = performance.now() + 4000;
  let saves = 0;
  let wheels = 0;
  while (performance.now() < until) {
    await ui.wheel(at, 200);
    wheels += 1;
    if (performance.now() > saves * 500 + (until - 4000)) {
      saves += 1;
      invoke("file_write", { path: `rate_save_${saves}.txt`, text: "saved while the editor scrolls\n".repeat(2000) });
    }
  }
  const times = await counting;
  const gaps = times.slice(1).map((time, at) => time - times[at]);
  const rate = gaps.length / ((times.at(-1) - times[0]) / 1000);
  // As far as the wheel's steps carry the view, half of them at the least, a glide still under way at
  // the end: the steps that came, however many the machine sends in the time.
  const carried = Math.min(view.scroller.scrollHeight - view.scroller.clientHeight, wheels * 200 * 0.5);
  assert.ok(view.scrollY() >= carried, `the view moved ${view.scrollY()} of the ${wheels * 200} px its ${wheels} steps sent, over ${times.length} frames at ${rate.toFixed(1)} a second`);
  assert.ok(saves >= 6, `files were saved as it scrolled: ${saves}`);
  assert.ok(rate >= display * 0.8, `${rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}`);
  assert.ok(Math.max(...gaps) < 100, `the longest wait between two frames, ${Math.max(...gaps).toFixed(1)} ms`);
});

test("smooth scrolling starts off where the system asks for reduced motion, and the reader's own choice wins either way", async () => {
  const kept = localStorage.getItem("orior.smooth-scroll");
  try {
    localStorage.removeItem("orior.smooth-scroll");
    await ui.media("prefers-reduced-motion", "reduce");
    assert.equal(smoothScrolling(), false, "reduced motion: off");
    localStorage.setItem("orior.smooth-scroll", "true");
    assert.equal(smoothScrolling(), true, "turned on here, it is on");
    localStorage.removeItem("orior.smooth-scroll");
    await ui.media("prefers-reduced-motion", "no-preference");
    assert.equal(smoothScrolling(), true, "no preference: on");
    localStorage.setItem("orior.smooth-scroll", "false");
    assert.equal(smoothScrolling(), false, "turned off here, it is off");
  } finally {
    await ui.media("prefers-reduced-motion");
    if (kept === null) {
      localStorage.removeItem("orior.smooth-scroll");
    } else {
      localStorage.setItem("orior.smooth-scroll", kept);
    }
  }
});

test("under a steady wheel every frame moves the text an even step", async () => {
  await openFile("rate.py");
  await ui.waitFor(() => editing().active === "rate.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  const smooth = view.smoothOn;
  view.smoothOn = true;
  view.goTo(0, 0);
  // The share of frames the page misses at rest, the machine's own, which a scroll may miss too, and 3%
  // past it.
  const rest = await framesOver(1000);
  const restGaps = rest.slice(1).map((time, index) => time - rest[index]);
  const restInterval = [...restGaps].sort((a, b) => a - b)[Math.floor(restGaps.length / 2)];
  const missedAtRest = restGaps.filter((gap) => gap > 1.5 * restInterval).length / restGaps.length;
  const box = view.scroller.getBoundingClientRect();
  const at = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
  const writer = window.__harness.servers.ORIOR_TEST_QRY_WRITER;
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("folder_create", { path: "build/trace" }).catch(() => {});
  const traceRun = "build/trace/query1-steady.qry";
  await ui.run(writer, [`${root}/${traceRun}`, root], { wait: false });
  assert.ok(await invoke("qry_ready", { path: `${root}/${traceRun}` }), "the run's writer came ready");
  await joinRun(traceRun);
  await ui.browserTrace();
  const frames = [];
  let wheeling = true;
  const tick = (now) => {
    frames.push([now, seen(view)]);
    if (wheeling) {
      requestAnimationFrame(tick);
    }
  };
  requestAnimationFrame(tick);
  const until = performance.now() + 3000;
  while (performance.now() < until) {
    await ui.wheel(at, 120);
    await new Promise((done) => setTimeout(done, 30));
  }
  wheeling = false;
  view.smoothOn = smooth;
  await joinRun(null);
  await ui.browserTraced(writer, `${root}/${traceRun}`, `${root}/browser.trace`);
  await new Promise((done) => setTimeout(done, 300));
  const paints = paintsOf(await invoke("qry_joined", { path: traceRun, name: TRACE_BLOB }));
  const browser = (await invoke("qry_joined", { path: traceRun, name: "browser.trace" })).split("\n").filter((line, at) => at < 8 || line.startsWith("forced")).join("; ");
  await invoke("qry_stop", { path: `${root}/${traceRun}` });
  const slowest = [...paints].sort((a, b) => b.spent - a.spent).slice(0, 5).map((paint) => `${paint.spent.toFixed(2)} ms ${JSON.stringify(Object.fromEntries(Object.entries(paint.stages).map(([stage, ms]) => [stage, Number(ms.toFixed(2))])))}`).join("; ");
  // The glide's first frames gather speed, and its last ones after the wheel stops lose it.
  const steady = frames.filter(([time]) => time > frames[0][0] + 300 && time < until - 100);
  const steps = steady.slice(1).map(([time, y], index) => [time - steady[index][0], y - steady[index][1]]);
  const middleOf = (values) => [...values].sort((a, b) => a - b)[Math.floor(values.length / 2)];
  const interval = middleOf(steps.map(([gap]) => gap));
  const late = steps.filter(([gap]) => gap > 1.5 * interval).length;
  const still = steps.filter(([, moved]) => moved === 0).length;
  // A step read in a frame may be the glide's of the frame before: only the steps whose frame and the
  // frames on either side came on time say how even the glide is.
  const onTime = (index) => steps[index] && steps[index][0] <= 1.5 * interval;
  const even = steps.filter((_, index) => onTime(index - 1) && onTime(index) && onTime(index + 1)).map(([, moved]) => moved);
  const middle = middleOf(even);
  const spread = Math.sqrt(even.reduce((sum, moved) => sum + (moved - middle) ** 2, 0) / even.length) / middle;
  assert.ok(steps.length > 100, `frames counted: ${steps.length}`);
  const missed = steps.map(([gap], index) => [index, Math.round(gap)]).filter(([, gap]) => gap > 1.5 * interval);
  assert.ok(late <= steps.length * (0.03 + missedAtRest), `frames the page missed: ${late} of ${steps.length}, ${(missedAtRest * 100).toFixed(1)}% at rest, each [frame, ms]: ${JSON.stringify(missed)}; the slowest frames the trace holds: ${slowest}; the browser's busiest work: ${browser}`);
  assert.ok(still <= 2, `frames the text stood still: ${still} of ${steps.length}`);
  assert.ok(spread < 0.3, `the steps spread by ${spread.toFixed(2)} of their middle, ${middle} px`);
});

// Fingers held on a trackpad: small steps in pixels, one each frame or so, which the view follows
// as they come. The frames drawn at the display's own rate, and the text moved as far as the steps.
async function trackpadOver(view, ms) {
  const box = view.scroller.getBoundingClientRect();
  const at = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
  const counting = framesOver(ms);
  const before = view.scrollY();
  const until = performance.now() + ms;
  let sent = 0;
  while (performance.now() < until) {
    await ui.wheel(at, 7);
    sent += 7;
    await new Promise((done) => setTimeout(done, 6));
  }
  const times = await counting;
  const gaps = times.slice(1).map((time, index) => time - times[index]);
  return { rate: gaps.length / ((times.at(-1) - times[0]) / 1000), longest: Math.max(...gaps), moved: view.scrollY() - before, sent };
}

test("fingers held on a trackpad move the text at the display's own rate", async () => {
  await openFile("rate.py");
  await ui.waitFor(() => editing().active === "rate.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.goTo(0, 0);
  const rest = await framesOver(1000);
  const display = (rest.length - 1) / ((rest.at(-1) - rest[0]) / 1000);
  const held = await trackpadOver(view, 3000);
  assert.ok(held.moved >= held.sent * 0.9, `the text moved ${held.moved} px of the ${held.sent} the fingers sent`);
  assert.ok(held.rate >= display * 0.8, `${held.rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}`);
  assert.ok(held.longest < 100, `the longest wait between two frames, ${held.longest.toFixed(1)} ms`);
});

// A file of long lines of words, for word wrap.
async function wrappedFile(name, count) {
  const words = "wrap the long line of words at the view's edge and go on below it";
  const lines = Array.from({ length: count }, (_, line) => `${line}: ${Array.from({ length: 6 }, () => words).join(" ")}`);
  await invoke("file_write", { path: name, text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile(name);
  await ui.waitFor(() => editing().active === name && editing().editor?.s, 5000);
  return editing().editor;
}

// The view's width settled: the columns a row holds the same for a second, as the editor taking the keys slides the
// window's edges away and the work takes their room, and the scrollbar across goes as lines wrap.
async function widthSettled(view) {
  let last = -1;
  let since = performance.now();
  await ui.waitFor(
    () => {
      const now = view.wrapColumns();
      if (now !== last) {
        last = now;
        since = performance.now();
      }
      return performance.now() - since >= 1000;
    },
    5000,
    "the view's width settled",
  );
}

test("word wrap shows a long line as rows that end after a space, each moved through and pressed on as a row", async () => {
  const view = await wrappedFile("wrapped.txt", 40);
  const kept = view.wrapped();
  try {
    view.setWrap(true);
    // The caret put in the line first, so the editor has taken the keys and its width has settled before the rows
    // are read at it.
    view.goTo(0, 0);
    await widthSettled(view);
    const rows = view.rows();
    const columns = view.wrapColumns();
    const text = view.doc.line(0);
    assert.ok(rows.size > 40, `the lines take ${rows.size} rows`);
    assert.equal(rows.lineOf(rows.rowOf(1) - 1), 0, "the first line's rows come before the second's");
    const second = rows.rowOf(0) + 1;
    const start = rows.startOf(second);
    assert.ok(start > 0 && start <= columns && text[start - 1] === " ", `the first row ends after a space, at ${start} of ${columns} columns`);
    // The caret on the second row stands at the left, a row down.
    view.goTo(0, start + 2);
    await new Promise((rest) => requestAnimationFrame(() => requestAnimationFrame(rest)));
    const at = view.rectOf({ line: 0, col: start + 2 });
    const first = view.rectOf({ line: 0, col: 2 });
    assert.equal(Math.round(at.left), Math.round(first.left), "a col two into the row stands where col two of the line does");
    assert.equal(Math.round(at.top - first.top), view.lineHeight, "a row down");
    // Down goes to the next row of the same line, Up back.
    view.goTo(0, 2);
    await ui.key("Down");
    assert.deepEqual(view.head(), { line: 0, col: start + 2 }, "Down goes a row");
    await ui.key("Up");
    assert.deepEqual(view.head(), { line: 0, col: 2 }, "Up comes back");
    // A press on the second row lands in it.
    await ui.click({ x: at.left + 0.5, y: (at.top + at.bottom) / 2 });
    assert.deepEqual(view.head(), { line: 0, col: start + 2 }, "a press on the row");
    // Only the first row of a line has its number.
    await new Promise((rest) => requestAnimationFrame(() => requestAnimationFrame(rest)));
    const numbers = [...view.host.querySelectorAll(".ed-num-text")].map((node) => node.textContent);
    assert.equal(numbers.filter((number) => number === "1").length, 1, "line 1's number once");
    view.setWrap(false);
    assert.equal(view.rows().size, view.doc.count, "unwrapped, a row a line");
  } finally {
    view.setWrap(kept);
  }
});

test("fingers held on a trackpad move wrapped text at the display's own rate", async () => {
  const view = await wrappedFile("wrapped_long.txt", 20000);
  const kept = view.wrapped();
  try {
    view.setWrap(true);
    view.goTo(0, 0);
    const rest = await framesOver(1000);
    const display = (rest.length - 1) / ((rest.at(-1) - rest[0]) / 1000);
    const held = await trackpadOver(view, 3000);
    assert.ok(held.moved >= held.sent * 0.9, `the text moved ${held.moved} px of the ${held.sent} the fingers sent`);
    assert.ok(held.rate >= display * 0.8, `${held.rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}`);
    assert.ok(held.longest < 100, `the longest wait between two frames, ${held.longest.toFixed(1)} ms`);
  } finally {
    view.setWrap(kept);
  }
});
