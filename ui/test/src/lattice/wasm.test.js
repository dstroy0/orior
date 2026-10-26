// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice's computing in wasm: a slot's points laid out over a box, placed for a 2D canvas a level of disorder
// at a time, moved and shaded from the pens as dots for the GPU, the pens written in the module's memory, whether the
// loop moved enough to be seen, and when it next will.

import { countsOf, dotsOf, lattice, offsetsOf, penColorsOf, penRadiiOf, placedOf, shownAfter, shows } from "../../../gui/src-js/lattice/wasm.js";

const { test, assert } = window.__harness;

// The last of the module's slots, which the page's own few lattices leave free.
const SLOT = 7;

test("a slot's points are laid out over the box, each kept as two bytes, its place on its circle around its origin", () => {
  const count = lattice.lattice_lay(SLOT, 300, 200);
  try {
    assert.ok(count > 10, `${count} points`);
    const offsets = offsetsOf(SLOT, count);
    for (let at = 0; at < count * 2; at += 2) {
      assert.ok(Math.hypot(offsets[at], offsets[at + 1]) <= 128, "inside the circle");
    }
    const kept = Int8Array.from(offsets);
    assert.equal(lattice.lattice_lay(SLOT, 300, 200), count, "the same box lays the same points");
    assert.deepEqual([...offsetsOf(SLOT, count)], [...kept], "each in the same place on its circle");
    lattice.lattice_dots_made(SLOT, 300, 200, 100);
    const { places } = dotsOf(SLOT, count);
    for (let at = 0; at < count * 4; at += 4) {
      assert.ok(places[at] >= -60 && places[at] <= 360 && places[at + 1] >= -60 && places[at + 1] <= 260, "moved near the box");
    }
  } finally {
    lattice.lattice_forget(SLOT);
  }
});

test("a lattice laid out again over a larger box keeps every point it had, at its origin and on its circle", () => {
  const step = 22 / Math.sqrt(3) / Math.sqrt(8);
  const columnsOf = (width) => Math.floor((width - step / 2) / step) + 1;
  const small = lattice.lattice_lay(SLOT, 300, 200);
  try {
    const before = Int8Array.from(offsetsOf(SLOT, small));
    const larger = lattice.lattice_lay(SLOT, 420, 260);
    const after = offsetsOf(SLOT, larger);
    const [was, is] = [columnsOf(300), columnsOf(420)];
    assert.ok(larger > small && is > was, `${small} points then ${larger}`);
    for (let row = 0; row < small / was; row += 1) {
      for (let column = 0; column < was; column += 1) {
        const old = (row * was + column) * 2;
        const now = (row * is + column) * 2;
        assert.deepEqual([after[now], after[now + 1]], [before[old], before[old + 1]], `the point at row ${row}, column ${column} kept`);
      }
    }
    lattice.lattice_dots_made(SLOT, 420, 260, 1);
    const { places } = dotsOf(SLOT, larger);
    const second = (1 * is + 1) * 4;
    assert.ok(Math.abs(places[second] - step * 1.5) < 1e-4 && Math.abs(places[second + 1] - step * 1.5) < 1e-4, "a step apart whatever the box");
  } finally {
    lattice.lattice_forget(SLOT);
  }
});

test("placed for a 2D canvas, every point stands at one level, the levels' counts adding up to the points", () => {
  const count = lattice.lattice_lay(SLOT, 300, 200);
  try {
    const levels = 24;
    lattice.lattice_place(SLOT, 300, 200, 3.5, levels);
    const counts = countsOf(SLOT, levels);
    assert.equal(counts.reduce((sum, one) => sum + one, 0), count);
    assert.equal(placedOf(SLOT, count).length, count * 2);
  } finally {
    lattice.lattice_forget(SLOT);
  }
});

test("the pens are written in the module's memory and read back as written", () => {
  const levels = lattice.lattice_pen_levels();
  const colors = penColorsOf(levels);
  const radii = penRadiiOf(levels);
  assert.equal(colors.length, levels * 4);
  const kept = [colors[0], radii[0]];
  colors[0] = 0.25;
  radii[0] = 1.5;
  assert.deepEqual([penColorsOf(levels)[0], penRadiiOf(levels)[0]], [0.25, 1.5]);
  colors[0] = kept[0];
  radii[0] = kept[1];
  lattice.lattice_pens_changed();
});

test("the loop is seen to move over a long enough time and not over none", () => {
  assert.equal(shows(10, 10), false);
  assert.equal(shows(0, 15), false, "the rest, its first 16 seconds, moves nothing");
  assert.equal(shows(0, 20), true);
});

test("the loop's next change is found where it first shows: none through the rest, and soon while it moves", () => {
  const after = shownAfter(1, 1);
  assert.ok(after >= 16 && after < 17, `the rest ends at 16 s and the first change is seen at ${after}`);
  assert.equal(shows(1, after), true, "the moment found shows");
  assert.equal(shows(1, after - 0.002), false, "and none before it does");
  const moving = shownAfter(20, 20);
  assert.ok(moving > 20 && moving < 20.5, `the spread shows a change within half a second: ${moving}`);
});

test("the dots are moved, then shaded and sized from the pens, four words a dot", () => {
  const count = lattice.lattice_lay(SLOT, 300, 200);
  const levels = lattice.lattice_pen_levels();
  const colors = penColorsOf(levels);
  const radii = penRadiiOf(levels);
  const kept = [Float32Array.from(colors), Float32Array.from(radii)];
  try {
    colors.fill(0);
    for (let level = 0; level < levels; level += 1) {
      colors.set([1, 0, 0, 0.5], level * 4);
      radii[level] = 0.75;
    }
    lattice.lattice_pens_changed();
    assert.equal(lattice.lattice_dots_made(SLOT, 300, 200, 1), 1);
    const { places, shades } = dotsOf(SLOT, count);
    const half = 22 / Math.sqrt(3) / Math.sqrt(8) / 2;
    assert.ok(Math.abs(places[0] - half) < 1e-4 && Math.abs(places[1] - half) < 1e-4, `at the rest the ordered side's first dot stands at its origin, half a step in: ${places[0]}, ${places[1]}`);
    assert.ok(Math.abs(places[2] - 0.75 * 0.955) < 1e-5, `its radius the pen's at FITS: ${places[2]}`);
    assert.deepEqual([...shades.subarray(12, 16)], [255, 0, 0, 128], "its shade the pen's, a byte a channel");
  } finally {
    colors.set(kept[0]);
    radii.set(kept[1]);
    lattice.lattice_pens_changed();
    lattice.lattice_forget(SLOT);
  }
});
