// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view: how far ahead of a view at rest its lines are colored, what it holds back
// while it scrolls fast, a fast scroll at the display's own rate, the level a scroll holds, smooth
// scrolling under reduced motion and the even steps of a steady wheel, and text that runs right to
// left placed where the page draws it.

import { coloringReach, runsRightToLeft, smoothScrolling } from "../../../src/editor/view.js";
import { invoke } from "../../../src/bridge.js";
import { editing, onEdit, openFile } from "../../../src/edit.js";
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

// The frames the page is given over `ms`, as requestAnimationFrame calls it: the times of each.
function framesOver(ms) {
  return new Promise((done) => {
    const times = [];
    const tick = (now) => {
      times.push(now);
      if (now - times[0] < ms) {
        requestAnimationFrame(tick);
      } else {
        done(times);
      }
    };
    requestAnimationFrame(tick);
  });
}

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
  while (performance.now() < until) {
    await ui.wheel(at, 200);
    if (performance.now() > saves * 500 + (until - 4000)) {
      saves += 1;
      invoke("file_write", { path: `rate_save_${saves}.txt`, text: "saved while the editor scrolls\n".repeat(2000) });
    }
  }
  const times = await counting;
  const gaps = times.slice(1).map((time, at) => time - times[at]);
  const rate = gaps.length / ((times.at(-1) - times[0]) / 1000);
  assert.ok(view.scrollY() > 1000, "the view moved");
  assert.ok(saves >= 6, `files were saved as it scrolled: ${saves}`);
  assert.ok(rate >= display * 0.8, `${rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}`);
  assert.ok(Math.max(...gaps) < 100, `the longest wait between two frames, ${Math.max(...gaps).toFixed(1)} ms`);
});

test("the level a frame drops to rises at once, and falls a step only once the view has stayed well under its speed for a while", async () => {
  await openFile("rate.py");
  await ui.waitFor(() => editing().active === "rate.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  // The view's own way of leveling, on a hold of the test's own, the level written just before each
  // ask: the editor's own scrolls and rests write the level too, and the test's waits give them time.
  // Its speeds, in pixels a millisecond, step the level up past 0.8, 2.5 and 7.
  const held = { fallSince: null };
  const leveled = (speed, level, strain = 0) => {
    write("scroll", { level });
    return view.leveled.call(held, speed, strain);
  };
  const wait = (ms) => new Promise((done) => setTimeout(done, ms));
  try {
    assert.equal(leveled(3, 0), 2, "a speed past a step raises it at once");
    assert.equal(leveled(0, 0, 2), 2, "and so does the strain");
    assert.equal(leveled(2.2, 2), 2, "just under the step that set it, it holds");
    await wait(400);
    assert.equal(leveled(2.2, 2), 2, "and holds however long the speed sways there");
    assert.equal(leveled(1, 2), 2, "well under, it holds at first");
    await wait(100);
    assert.equal(leveled(1, 2), 2, "and for a moment");
    await wait(300);
    assert.equal(leveled(1, 2), 1, "and then falls one step");
    assert.equal(leveled(0, 1), 1, "the next step waits as long again");
    assert.equal(leveled(3, 1), 2, "and a speed past a step raises it at once whatever it waited for");
  } finally {
    write("scroll", { level: 0 });
  }
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

test("under a steady wheel every frame moves the text an even step, and the level holds", async () => {
  await openFile("rate.py");
  await ui.waitFor(() => editing().active === "rate.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  const { status } = await import("../../../src/status.js");
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
  const frames = [];
  let wheeling = true;
  const tick = (now) => {
    frames.push([now, view.scrollY(), status.scroll.level]);
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
  const changes = steady.slice(1).filter(([, , level], index) => level !== steady[index][2]).length;
  assert.ok(steps.length > 100, `frames counted: ${steps.length}`);
  const missed = steps.map(([gap], index) => [index, Math.round(gap), steady[index + 1][2]]).filter(([, gap]) => gap > 1.5 * interval);
  assert.ok(late <= steps.length * (0.03 + missedAtRest), `frames the page missed: ${late} of ${steps.length}, ${(missedAtRest * 100).toFixed(1)}% at rest, each [frame, ms, level]: ${JSON.stringify(missed)}`);
  assert.ok(still <= 2, `frames the text stood still: ${still} of ${steps.length}`);
  assert.ok(spread < 0.3, `the steps spread by ${spread.toFixed(2)} of their middle, ${middle} px`);
  assert.ok(changes <= 3, `the level changed ${changes} times in ${steps.length} frames`);
});

// Whether the listener the test gives onEdit is listening: it stays given for the page's life.
let hearing = null;
onEdit((s) => hearing?.(s));

test("a letter typed is drawn before anything told of each edit hears of it", async () => {
  await invoke("file_write", { path: "told.py", text: "first = 1\n" });
  showView("edit");
  await openFile("told.py");
  await ui.waitFor(() => editing().active === "told.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.goTo(0, 0);
  view.focus();
  const heard = [];
  hearing = () => heard.push(view.rowNode(0)?.textContent ?? "");
  try {
    await ui.type("x");
    await ui.waitFor(() => heard.length > 0, 2000);
  } finally {
    hearing = null;
  }
  assert.equal(view.s.doc.line(0), "xfirst = 1");
  assert.ok(heard[0].startsWith("xfirst"), `the row as the first told of the edit found it: ${JSON.stringify(heard[0])}`);
});

// Types `count` letters into the editor shown and gives, for each: whether the letter was in its row
// when the frame after it began, the page's own milliseconds for it from its key to its row drawn,
// and the milliseconds from the frame after it to the next, the display's interval where that frame
// came on time.
async function keysTimed(view, line, count) {
  const timed = [];
  let key = null;
  let typed = 0;
  const began = (event) => {
    if (event.inputType !== "insertText") {
      return;
    }
    const at = performance.now();
    const want = event.data.repeat(typed);
    const one = { at };
    key = one;
    requestAnimationFrame((first) => {
      const drawn = (view.rowNode(line)?.textContent ?? "").includes(want);
      requestAnimationFrame((second) => timed.push({ drawn, own: one.done - at, frame: second - first }));
    });
  };
  const done = () => {
    if (key && key.done === undefined) {
      key.done = performance.now();
    }
  };
  document.addEventListener("beforeinput", began, true);
  document.addEventListener("input", done);
  try {
    for (let at = 0; at < count; at += 1) {
      const before = timed.length;
      typed = at + 1;
      await ui.type("q");
      await ui.waitFor(() => timed.length > before, 2000);
      await new Promise((rest) => setTimeout(rest, 40));
    }
  } finally {
    document.removeEventListener("beforeinput", began, true);
    document.removeEventListener("input", done);
  }
  return timed;
}

test("a letter typed in the middle of 250,000 lines, and inside a line of 20,000 characters, is drawn in the frame after its key", async () => {
  const rest = await framesOver(500);
  const interval = (rest.at(-1) - rest[0]) / (rest.length - 1);
  const files = [
    ["keys_long.py", Array.from({ length: 250000 }, (_, line) => `x_${line} = ${line} * 2  # line ${line}`).join("\n") + "\n", 125000, 0],
    ["keys_wide.txt", "w".repeat(20000) + "\n", 0, 5000],
  ];
  for (const [path, text, line, col] of files) {
    await invoke("file_write", { path, text });
    showView("edit");
    await openFile(path);
    await ui.waitFor(() => editing().active === path && editing().editor?.s, 10000);
    const view = editing().editor;
    await ui.waitFor(() => view.s.base + view.s.doc.count >= line + 1, 10000);
    view.goTo(line - view.s.base, col);
    view.focus();
    await framesOver(500);
    const timed = await keysTimed(view, line - view.s.base, 20);
    const own = timed.map((one) => one.own).sort((a, b) => a - b);
    const late = timed.filter((one) => one.frame > 1.5 * interval).length;
    const said = `${path}: own ms ${own.map((ms) => ms.toFixed(1)).join(" ")}; frames after ${timed.map((one) => one.frame.toFixed(1)).join(" ")}`;
    assert.equal(timed.filter((one) => one.drawn).length, timed.length, `each letter in its row as the frame after it began: ${said}`);
    assert.ok(own[Math.floor(own.length / 2)] <= 5, `the page's own work for a key, in the middle: ${said}`);
    assert.ok(own.at(-1) <= 12, `and at most: ${said}`);
    assert.ok(late <= 2, `frames after a key that came late: ${late} of ${timed.length}: ${said}`);
  }
});
