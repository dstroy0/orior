// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice the docs site draws behind its hero: a square grid of points, each moved off its place
// by an amount that is zero where the lattice is ordered and grows where it is not. A point in place
// is signal green, and a moved one passes through the link blue to violet. Each point's way off its
// place is seeded and the same every time; how far it goes is the lattice's order where it stands.
//
// The order moves, slowly, through a loop. From ordered at the left and disordered at the right, the
// order spreads left to right until the whole lattice is in place, then it inverts and the whole
// lattice comes apart. The order comes back with the top ordered and the bottom not, and the ordered
// side turns: to the right, to the bottom, and to the left, where the loop began. Every lattice on the
// page stands at the same point of the loop. Where the reader asks for less motion the lattice holds
// still, ordered at the left.

import { rgbOf } from "./colors.js";
import { clock, still as paused, whenMoving } from "./motion.js";
import { onScheme } from "./scheme.js";

const STEP = 22;
const SEED = 1729;
// The colors from ordered to disordered, the stylesheet's --lattice-1 to --lattice-3.
const STOPS = ["--lattice-1", "--lattice-2", "--lattice-3"];

// The loop's stages, each its length in seconds.
const STAGES = [
  ["rest", 4],
  ["spread", 10],
  ["invert", 5],
  ["gather", 7],
  ["turn", 8],
  ["turn", 8],
  ["turn", 8],
];
const LOOP = STAGES.reduce((sum, [, seconds]) => sum + seconds, 0);

// Where one side is ordered: the share of the lattice before its points start to move, and the share
// over which they go from in place to as far as they go. A point moves by the square of how far along
// that share it stands, and stays within three pixels of its place to about 0.23 of the lattice, which
// reads as ordered against the 0.77 that does not: about 0.3 to 1.
const ORDERED = 0.1;
const RAMP = 0.54;

// The shades a point takes, one a level of disorder.
const LEVELS = 24;

// The longest between two frames of a moving lattice, in milliseconds.
const FRAME = 33;

function random(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let mixed = state;
    mixed = Math.imul(mixed ^ (mixed >>> 15), mixed | 1);
    mixed ^= mixed + Math.imul(mixed ^ (mixed >>> 7), mixed | 61);
    return ((mixed ^ (mixed >>> 14)) >>> 0) / 4294967296;
  };
}

function shade(loose) {
  const along = loose * (STOPS.length - 1);
  const low = Math.min(Math.floor(along), STOPS.length - 2);
  const part = along - low;
  const to = rgbOf(STOPS[low + 1]);
  return rgbOf(STOPS[low]).map((from, at) => Math.round(from + (to[at] - from) * part));
}

// The pen of each level, made again when the scheme or a theme changes.
let pens = null;
onScheme(() => {
  pens = null;
});
function pensOf() {
  pens ??= Array.from({ length: LEVELS }, (_, level) => {
    const loose = level / (LEVELS - 1);
    const kept = 1 - loose;
    const [red, green, blue] = shade(loose);
    return { color: `rgba(${red}, ${green}, ${blue}, ${(0.36 + 0.16 * kept).toFixed(3)})`, size: 1.3 + 0.4 * kept };
  });
  return pens;
}

const clamp = (value) => Math.max(0, Math.min(1, value));
const ease = (part) => part * part * (3 - 2 * part);

// The disorder where a point stands, with the ordered side away from `angle`: 0 orders the left,
// a quarter turn the top, a half turn the right, three quarters the bottom. `u` and `v` are the
// point's place across and down, each from 0 to 1.
function sided(angle, u, v) {
  const dx = Math.cos(angle);
  const dy = Math.sin(angle);
  const along = 0.5 + ((u - 0.5) * dx + (v - 0.5) * dy) / (Math.abs(dx) + Math.abs(dy));
  return clamp((along - ORDERED) / RAMP);
}

// The loop at `seconds`: a way to read the disorder at any point.
function orderAt(seconds) {
  let left = seconds % LOOP;
  let turns = 0;
  for (const [name, length] of STAGES) {
    if (left >= length) {
      left -= length;
      turns += name === "turn" ? 1 : 0;
      continue;
    }
    const part = ease(left / length);
    if (name === "rest") {
      return (u, v) => sided(0, u, v);
    }
    if (name === "spread") {
      const front = ORDERED + (1 - ORDERED) * part;
      return (u) => clamp((u - front) / RAMP);
    }
    if (name === "invert") {
      return () => part;
    }
    if (name === "gather") {
      return (u, v) => Math.max(sided(Math.PI / 2, u, v), 1 - part);
    }
    const angle = (Math.PI / 2) * (1 + turns + part);
    return (u, v) => sided(angle, u, v);
  }
  return (u, v) => sided(0, u, v);
}

// Each lattice's points: place across, place down, and the way and share of the reach it moves.
const lattices = new Map();

function layOut(canvas) {
  const box = canvas.getBoundingClientRect();
  if (!box.width || !box.height) {
    return null;
  }
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(box.width * scale);
  canvas.height = Math.round(box.height * scale);
  const draw = random(SEED);
  const points = [];
  for (let y = STEP / 2; y < box.height; y += STEP) {
    for (let x = STEP / 2; x < box.width; x += STEP) {
      const turn = draw() * Math.PI * 2;
      const reach = draw();
      points.push(x, y, Math.cos(turn) * reach, Math.sin(turn) * reach);
    }
  }
  const laid = { width: box.width, height: box.height, scale, points };
  lattices.set(canvas, laid);
  return laid;
}

function paint(canvas, laid, seconds) {
  const { width, height, scale, points } = laid;
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  pen.clearRect(0, 0, width, height);
  const order = orderAt(seconds);
  const levels = Array.from({ length: LEVELS }, () => []);
  for (let at = 0; at < points.length; at += 4) {
    const x = points[at];
    const y = points[at + 1];
    const loose = order(x / width, y / height);
    const shift = loose * loose * STEP * 2.4;
    levels[Math.round(loose * (LEVELS - 1))].push(x + points[at + 2] * shift, y + points[at + 3] * shift);
  }
  levels.forEach((placed, level) => {
    if (!placed.length) {
      return;
    }
    const { color, size } = pensOf()[level];
    pen.fillStyle = color;
    pen.beginPath();
    for (let at = 0; at < placed.length; at += 2) {
      pen.moveTo(placed[at] + size, placed[at + 1]);
      pen.arc(placed[at], placed[at + 1], size, 0, Math.PI * 2);
    }
    pen.fill();
  });
}

const still = () => matchMedia("(prefers-reduced-motion: reduce)").matches;

export function drawLattice(canvas) {
  const laid = layOut(canvas);
  if (laid) {
    paint(canvas, laid, still() ? 0 : clock() / 1000);
  }
}

// Moves every lattice in sight a frame, no faster than one each FRAME.
let last = 0;
function tick(now) {
  if (paused()) {
    whenMoving(() => requestAnimationFrame(tick));
    return;
  }
  requestAnimationFrame(tick);
  if (now - last < FRAME || document.hidden || still()) {
    return;
  }
  last = now;
  for (const [canvas, laid] of lattices) {
    if (!canvas.isConnected) {
      lattices.delete(canvas);
    } else if (canvas.offsetParent !== null) {
      paint(canvas, laid, clock(now) / 1000);
    }
  }
}
requestAnimationFrame(tick);

// Lays a lattice out again each time its size changes, which a lattice in a hidden view also does
// when the view is shown. One taken off the page is let go.
const sized = new ResizeObserver((entries) =>
  entries.forEach((entry) => (entry.target.isConnected ? drawLattice(entry.target) : sized.unobserve(entry.target))),
);

export function keepLattice(canvas) {
  sized.observe(canvas);
}

// Keeps every lattice on the page drawn.
export function keepLattices() {
  document.querySelectorAll("canvas.lattice").forEach(keepLattice);
}
