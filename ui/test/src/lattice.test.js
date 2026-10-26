// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { DIM, drawnFrames, inkOf, latticeWaits, shows } from "../../gui/src-js/lattice.js";
import { lattice } from "../../gui/src-js/lattice/wasm.js";
import { setReduceMotion } from "../../gui/src-js/motion.js";

const { test, assert, ui } = window.__harness;

// The frames drawn over `seconds` of the loop from `from`, a frame offered every 1/30 s and drawn
// only where it shows, and the frames offered.
function framesOver(from, seconds) {
  let drawnAt = from;
  let drawn = 0;
  const offered = Math.round(seconds * 30);
  for (let frame = 1; frame <= offered; frame += 1) {
    const now = from + frame / 30;
    if (shows(drawnAt, now)) {
      drawn += 1;
      drawnAt = now;
    }
  }
  return { drawn, offered };
}

test("the most ordered point puts down 0.42 of its ink and the least 0.426 before the lattice's dimming", () => {
  assert.ok(Math.abs(inkOf(1) - 0.42) < 0.0005, `the brightest at ${inkOf(1)}`);
  assert.ok(Math.abs(inkOf(0) - 0.426 * DIM) < 0.0005, `the dimmest at ${inkOf(0)}`);
});

test("a lattice at rest draws no frame", () => {
  assert.equal(shows(0.5, 15.5), false, "the loop's rest moves nothing");
  assert.equal(framesOver(0, 16).drawn, 0);
});

test("a frame is drawn once the loop has moved a point by a quarter pixel", () => {
  assert.equal(shows(24, 24.0001), false, "a tenth of a millisecond of the spread moves no point a quarter pixel");
  assert.equal(shows(24, 25), true, "a second of the spread moves points");
});

test("over the whole loop the frames that move nothing are not drawn", () => {
  const loop = framesOver(0, 200);
  assert.ok(loop.drawn > 0, "the lattice moves");
  assert.ok(loop.drawn < loop.offered, `${loop.drawn} of ${loop.offered} frames drawn`);
});

test("no frame drawn moves the lattice less than a quarter pixel from the one before", () => {
  let drawnAt = 0;
  for (let frame = 1; frame <= 1500; frame += 1) {
    const now = frame / 30;
    if (shows(drawnAt, now)) {
      assert.ok(!shows(drawnAt, drawnAt + 0.0001), "a frame a moment after another shows nothing");
      drawnAt = now;
    }
  }
});

test("the lattice's points are laid out, placed and judged in its wasm module", () => {
  const slot = 7;
  const count = lattice.lattice_lay(slot, 240, 160);
  assert.ok(count > 100, `${count} points laid out`);
  lattice.lattice_dots_made(slot, 240, 160, 1);
  const dots = new Float32Array(lattice.memory.buffer, lattice.lattice_dots(slot), count * 4);
  const step = 22 / Math.sqrt(3) / Math.sqrt(8);
  assert.ok(Math.abs(dots[0] - step / 2) < 1e-4 && Math.abs(dots[1] - step / 2) < 1e-4, `the first point half a step in from the corner: ${dots[0]}, ${dots[1]}`);
  lattice.lattice_place(slot, 240, 160, 30, 96);
  const counts = new Uint32Array(lattice.memory.buffer, lattice.lattice_counts(slot), 96);
  assert.equal(counts.reduce((sum, one) => sum + one, 0), count, "every point placed at one level");
  assert.equal(lattice.lattice_shows(24, 25), 1, "a second of the spread moves points");
  assert.equal(lattice.lattice_shows(0.5, 15.5), 0, "the rest moves nothing");
});

const windowLattice = () => document.querySelector("canvas.lattice-window");

test("the page has one lattice, the window's, laid out over the whole window and drawn", async () => {
  assert.deepEqual([...document.querySelectorAll("canvas")].filter((one) => one.matches(".lattice")).length, 1, "one lattice canvas");
  await ui.waitFor(() => drawnFrames() > 0);
  const canvas = windowLattice();
  assert.equal(canvas.width, Math.round(canvas.getBoundingClientRect().width * (window.devicePixelRatio || 1)), "its buffer the window's pixels");
});

test("the lattice drawn waits for the loop's next change seen, asking for no frame before it", async () => {
  await ui.waitFor(() => drawnFrames() > 0);
  const waits = latticeWaits();
  assert.ok(waits.frame || waits.until > performance.now(), "a frame asked for, or a moment to ask for one");
  if (!waits.frame) {
    const before = drawnFrames();
    await new Promise((next) => requestAnimationFrame(() => requestAnimationFrame(next)));
    assert.equal(drawnFrames(), before, "no frame drawn while it waits");
  }
});

test("the lattice held still by less motion asks for no frame at all", async () => {
  setReduceMotion(true);
  try {
    await new Promise((next) => requestAnimationFrame(() => requestAnimationFrame(next)));
    const waits = latticeWaits();
    assert.ok(!waits.frame && waits.until === null, `nothing asked for: ${JSON.stringify(waits)}`);
  } finally {
    setReduceMotion(false);
  }
});

test("the lattice is laid out again when the window's size changes", async () => {
  const canvas = windowLattice();
  const scale = window.devicePixelRatio || 1;
  try {
    canvas.style.width = "320px";
    await ui.waitFor(() => canvas.width === Math.round(320 * scale));
  } finally {
    canvas.style.width = "";
  }
  await ui.waitFor(() => canvas.width === Math.round(canvas.getBoundingClientRect().width * scale));
});
