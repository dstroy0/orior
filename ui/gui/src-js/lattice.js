// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice the docs site draws behind its hero: a grid of points, each moved off its place
// by an amount that is zero where the lattice is ordered and grows where it is not. A point in place
// is signal green, and a moved one passes through the link blue to violet. Each point's way off its
// place is seeded and the same every time; how far it goes is the lattice's order where it stands.
//
// The order moves, slowly, through a loop. From ordered at the left and disordered at the right, the
// order spreads left to right until the whole lattice is in place, then it inverts and the whole
// lattice comes apart. The order comes back with the top ordered and the bottom not, and the ordered
// side turns: to the right, to the bottom, and to the left, where the loop began. Where the reader asks
// for less motion the lattice holds still, ordered at the left.
//
// The page has one lattice, the window's, behind the whole of it: every view is glass over it. Its
// computing and its GL driver are C built to wasm (lattice/wasm.js): its points stand in the module's
// memory, and the module reads the loop, moves and shades the points as dots it draws through the
// canvas's WebGL context or places them for a 2D canvas, judges whether a frame shows a change, and
// finds when the next will. The page keeps the canvas, the pens' colors and the clock, and asks for a
// frame only at the moment the next change shows.

import { clock, frameMs, idle, motionReduced, still as paused, whenMoving } from "./motion.js";
import { onScheme } from "./scheme.js";
import { LEVELS_DRAWN, pensOf } from "./lattice/pens.js";
import { countsOf, lattice, penColorsOf, penRadiiOf, placedOf, shownAfter, shows } from "./lattice/wasm.js";
import { contextOf } from "./lattice/webgl.js";

export { DIM, inkOf } from "./lattice/pens.js";
export { shows };

// The module's slot the window's lattice stands in.
const SLOT = 0;

// The longest between two frames of a lattice placed for a 2D canvas, in milliseconds.
const FRAME = 66;

// The window's lattice as laid out: its canvas, its size and scale, how many points, whether the GL driver
// draws it, the moment of the loop it was last drawn at, whether the scheme recolored it since, and how
// many frames it has been drawn; null before the window has a size.
let laid = null;

// The pens the GL driver blends between written in the scheme's colors, once and again whenever the scheme or a
// theme changes.
let pensStale = true;
onScheme(() => {
  pensStale = true;
});
function pensWritten() {
  if (!pensStale) {
    return;
  }
  const levels = lattice.lattice_pen_levels();
  const colors = penColorsOf(levels);
  const radii = penRadiiOf(levels);
  pensOf(levels).forEach(({ color, size }, level) => {
    const [red, green, blue, alpha] = color.match(/[\d.]+/g).map(Number);
    colors.set([red / 255, green / 255, blue / 255, alpha], level * 4);
    radii[level] = size;
  });
  lattice.lattice_pens_changed();
  pensStale = false;
}

// The lattice laid out over `canvas` at the size it stands at, its drawing buffer sized to the display's pixels.
function layOut(canvas) {
  const box = canvas.getBoundingClientRect();
  if (!box.width || !box.height) {
    return null;
  }
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(box.width * scale);
  canvas.height = Math.round(box.height * scale);
  const count = lattice.lattice_lay(SLOT, box.width, box.height);
  return count ? { canvas, width: box.width, height: box.height, scale, count, frames: laid?.frames ?? 0 } : null;
}

// Draws the lattice at `seconds` of the loop: through WebGL where the page has it, by the module's GL driver, else on
// its 2D canvas, each point placed by the module and drawn with the nearest of LEVELS_DRAWN pens.
function paint(seconds) {
  const { canvas, width, height, scale, count } = laid;
  laid.frames += 1;
  laid.drawnAt = seconds;
  laid.recolored = false;
  if (contextOf(SLOT, canvas)) {
    pensWritten();
    if (lattice.lattice_paint(SLOT, width, height, scale, seconds)) {
      laid.gl = true;
      return;
    }
  }
  laid.gl = false;
  lattice.lattice_place(SLOT, width, height, seconds, LEVELS_DRAWN);
  const counts = countsOf(SLOT, LEVELS_DRAWN);
  const placed = placedOf(SLOT, count);
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  pen.clearRect(0, 0, width, height);
  let at = 0;
  counts.forEach((many, level) => {
    if (!many) {
      return;
    }
    const { color, size } = pensOf(LEVELS_DRAWN)[level];
    pen.fillStyle = color;
    pen.beginPath();
    for (const end = at + many * 2; at < end; at += 2) {
      pen.moveTo(placed[at] + size, placed[at + 1]);
      pen.arc(placed[at], placed[at + 1], size, 0, Math.PI * 2);
    }
    pen.fill();
  });
}

// How many frames the window's lattice has been drawn, for the tests.
export const drawnFrames = () => laid?.frames ?? 0;

const still = () => motionReduced();

// The lattice's own time: the motion clock less every stretch the lattice was kept still, while the page
// was hidden or less motion was asked for, so the loop takes up where it stood rather than jumping to catch
// up. `since` is when the stretch now holding it began, null where none is.
const pause = { lost: 0, since: null };
const latticeSeconds = (now = performance.now()) => (clock(now) - pause.lost) / 1000;

// The frame asked for, and the timer that asks for one at the moment the next change shows, with that
// moment on the page's clock: none of either while the lattice waits on nothing.
const asked = { frame: 0, timer: 0, until: null };

// A frame asked for now, in place of any the timer would ask for later.
function wake() {
  window.clearTimeout(asked.timer);
  asked.timer = 0;
  asked.until = null;
  asked.frame ||= requestAnimationFrame(tick);
}

// What the lattice waits on, for the tests: whether a frame is asked for, and the moment one will be.
export const latticeWaits = () => ({ frame: asked.frame !== 0, until: asked.until });

// Moves the lattice, drawn only where the loop has moved since it was last drawn enough to be seen, or the
// scheme recolored it. Drawn by the GL driver it is drawn in the frame the change shows in; placed for a 2D
// canvas it is the window's lowest work, every point placed on the page's own thread: it moves no faster
// than one frame each FRAME and only in a frame nothing else wants. After the frame no other is asked for
// until the moment the next change shows, which the module finds, or motion changes; a frame only while a
// 2D lattice waits for one nothing else wants; and none at all while the page is hidden or less motion is
// asked for, until that changes.
let last = 0;
function tick(now) {
  asked.frame = 0;
  if (!laid) {
    return;
  }
  if (paused() || still()) {
    pause.since ??= now;
    whenMoving(wake);
    return;
  }
  if (pause.since !== null) {
    pause.lost += now - pause.since;
    pause.since = null;
  }
  const placing = idle(now) && now - last >= FRAME;
  const seconds = latticeSeconds(now);
  const due = laid.recolored || shows(laid.drawnAt, seconds);
  if (due && (laid.gl || placing)) {
    last = laid.gl ? last : now;
    paint(seconds);
  }
  const soonest = shownAfter(laid.drawnAt, seconds);
  if ((due && !laid.gl && !placing) || soonest - seconds <= frameMs() / 1000) {
    asked.frame = requestAnimationFrame(tick);
  } else {
    asked.until = now + (soonest - seconds) * 1000 - frameMs();
    asked.timer = window.setTimeout(wake, asked.until - now);
    // less motion asked for while it waits looks again at once, in place of the timer
    whenMoving(wake);
  }
}

// The scheme or a theme changed: the lattice is drawn again in its colors at the next frame, moving or not.
onScheme(() => {
  if (laid) {
    laid.recolored = true;
    wake();
  }
});

// The window's lattice laid out and drawn as the window takes its size, and again each time the size changes.
export function startLattice() {
  const canvas = document.querySelector("canvas.lattice-window");
  new ResizeObserver(() => {
    laid = layOut(canvas);
    if (laid) {
      paint(still() ? 0 : latticeSeconds());
      wake();
    }
  }).observe(canvas);
}
