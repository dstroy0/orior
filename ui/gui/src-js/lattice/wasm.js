// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice's computing and its GL driver, in C built to wasm32 (ui/gui/src-wasm), loaded once as the page loads:
// each lattice's points laid out in a slot of the module's memory, moved and shaded as dots drawn through the slot's
// WebGL context (webgl.js) or placed for a 2D canvas, whether the loop moved enough to be seen, and when it next will.
// The module takes the page's own cos, sin and remainder, so every number it gives is the one the page's arithmetic
// gives. A view of its memory is made anew each time it is read: memory the module grows leaves every view made before
// it empty.

import { glCalls } from "./webgl.js";

const memory = () => lattice.memory.buffer;
const imports = { env: { cos: Math.cos, sin: Math.sin, remainder: (value, by) => value % by, ...glCalls(memory) } };
const bytes = await (await fetch(new URL("./lattice.wasm", import.meta.url))).arrayBuffer();
export const lattice = (await WebAssembly.instantiate(bytes, imports)).instance.exports;

// The `count` points of a slot's lattice as the module keeps them, each the place it moves toward on its circle around
// its origin on the grid: its offset across and down, a signed byte each, 127 the circle's radius.
export const offsetsOf = (slot, count) => new Int8Array(memory(), lattice.lattice_offsets(slot), count * 2);

// How many of a slot's placed points stand at each of `levels` levels, and the `count` places, two a point, grouped
// by level.
export const countsOf = (slot, levels) => new Uint32Array(memory(), lattice.lattice_counts(slot), levels);
export const placedOf = (slot, count) => new Float64Array(memory(), lattice.lattice_placed(slot), count * 2);

// The `count` dots of a slot's lattice as the module last made them, sixteen bytes a dot: read as floats, its place
// across and down and its radius; read as bytes, its color's red, green, blue and ink from its thirteenth.
export function dotsOf(slot, count) {
  const at = lattice.lattice_dots(slot);
  return { places: new Float32Array(memory(), at, count * 4), shades: new Uint8Array(memory(), at, count * 16) };
}

// The pens the GL driver blends between, written in place: `levels` colors, red, green, blue and alpha each from 0
// to 1, and their radii.
export const penColorsOf = (levels) => new Float32Array(memory(), lattice.lattice_pen_colors(), levels * 4);
export const penRadiiOf = (levels) => new Float32Array(memory(), lattice.lattice_pen_radii(), levels);

// Whether the loop moved, from `then` to `now` in seconds, enough to be seen.
export const shows = (then, now) => lattice.lattice_shows(then, now) === 1;

// The soonest moment from `from` on, in seconds, at which the loop has moved since `then` enough to be seen.
export const shownAfter = (then, from) => lattice.lattice_shown_after(then, from);
