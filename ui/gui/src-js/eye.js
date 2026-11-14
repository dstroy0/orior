// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye the app shows while it loads. It opens once, from a low slit to wide over OPENING
// seconds, and its lids hold still from then on. Once it is open its iris divides, and divides again;
// it follows the pointer for FOLLOWS seconds, and from then on it rolls and stares.
//
// The opening is an almond, pointed at both corners, between heavy dark lids. A shadowy purple
// plasma burns all round it: a smooth glowing rim along both lids, and tongues that rise from the
// upper lid and run down from the lower. It lights up as the eye opens, wavers, and throws arcs.
//
// Each iris floats free on the sphere of the eye, a disc with a mass of its area. It speeds up and
// slows down, and irises that meet push off each other, the heavier one moving less. When the eye
// rolls, friction draws each iris's velocity toward that of the sphere under it, by FRICTION: a
// roll leaves them sliding and bumping, and a stare lets them come to rest turning with the eye.
//
// An iris is a plasma torus, a ring of plasma flowing round the pupil. Its pupil is dark and moves as
// a lava lamp does: one round body, and smaller ones that draw out of it, part from it as pupils of
// their own, and run back in, and it leans toward the mouse. A reader who asks the system for less
// motion gets the eye retained still and open, with one iris.

import { DIVIDE_EVERY, DIVIDE_FROM, HALF, SMALLEST, divide, float, frontOf, grow, irisAt } from "./eye/float.js";
import { DIM, GLINT, LEAN, LOOK, drawIris, irisPicture, lavaOf } from "./eye/iris.js";
import { drawOver, drawUnder, edges, openingPath, placeOf, sphereOf, tallOf } from "./eye/lids.js";
import { angle, cross, mix, seeded, smooth, times, unit } from "./eye/math.js";
import { drawPlasma, tonguesOf } from "./eye/tongues.js";
import { clock, motionReduced, still as paused, whenMoving } from "./motion.js";
import { cornersOf, plasmaOn } from "./plasma.js";

const OPENING = 1.4;
const OPEN_FROM = 0.1;

// One of the eye's still parts drawn once, onto a canvas of its own at the screen's pixels.
function layer(size, draw) {
  const scale = window.devicePixelRatio || 1;
  const canvas = document.createElement("canvas");
  canvas.width = Math.round(size.width * scale);
  canvas.height = Math.round(size.height * scale);
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  draw(pen);
  return canvas;
}

// The lids do not move once the eye is open, and nothing under or over the irises changes after.
// While it opens the parts under and over them are drawn each frame; once it is open they are drawn
// once into two layers, which each frame positions down as they are. The blurs in them, the dearest work
// in a frame, do not run again.
function drawEye(pen, width, height, state) {
  pen.clearRect(0, 0, width, height);
  const size = placeOf(width, height);
  const { cx, cy, a, b } = size;
  const r = b * 0.92;
  const lid = edges(state.open);
  const open = state.open >= 1;
  if (open && (state.layers?.width !== width || state.layers?.height !== height)) {
    state.layers = { width, height, under: layer(size, (layerPen) => drawUnder(layerPen, size, lid)), over: layer(size, (layerPen) => drawOver(layerPen, size, lid)) };
  }
  if (open) {
    pen.drawImage(state.layers.under, 0, 0, width, height);
  } else {
    drawUnder(pen, size, lid);
  }

  // The irises nearest the front of the sphere are drawn last, over the ones further round.
  pen.save();
  openingPath(pen, lid, cx, cy, a, b);
  pen.clip();
  const sphere = sphereOf(cx, cy, a, b);
  const displayed = state.irises.filter((iris) => iris.p[2] > 0.05).sort((one, two) => one.p[2] - two.p[2]);
  state.picture.paint(2 * r * (window.devicePixelRatio || 1), state.lava, state.seconds, state.shine);
  const ease = 1 - Math.exp(-state.dt * LEAN);
  for (const iris of displayed) {
    const [x, y, z] = iris.p;
    const sx = sphere.x + x * sphere.radius;
    const sy = sphere.y + y * sphere.radius;
    const radius = iris.size * r;
    let aim = [0, 0];
    if (state.mouse) {
      const dx = state.mouse[0] - sx;
      const dy = state.mouse[1] - sy;
      const far = Math.hypot(dx, dy) || 1;
      const lean = LOOK * Math.min(1, far / (radius * 3));
      aim = [(dx / far) * lean, (dy / far) * lean];
    }
    iris.look = iris.look.map((part, at) => part + (aim[at] - part) * ease);
    drawIris(pen, state.picture, iris, sx, sy, radius, Math.atan2(y, x), z);
  }
  pen.restore();

  if (open) {
    pen.drawImage(state.layers.over, 0, 0, width, height);
  } else {
    drawOver(pen, size, lid);
  }
  drawPlasma(size, lid, state, smooth(Math.max(0, Math.min(1, (state.open - OPEN_FROM) / (1 - OPEN_FROM)))));
}

// Once open, the eye follows the pointer for FOLLOWS seconds: its gaze turns to where the pointer
// is over the eye, as far as the eye turns, closing FOLLOW of the gap a second, and to the
// middle while the pointer is off the window. Then it rolls and stares by turns. A roll carries the
// gaze round a loop at ROLL radians a second, one way or the other, for ROLLS seconds; a stare snaps
// it to the middle, straight at the reader, for STARES seconds. Each is a span from the first number
// to the second.
const FOLLOWS = 8;
const FOLLOW = 14;
const ROLL = 3;
const ROLLS = [1.6, 2.8];
const STARES = [0.8, 1.6];

// The gaze that looks at the pointer at (x, y) in a canvas of this size: across and down from the
// eye's middle in parts of its half width and half height, each retained within -1 and 1.
function gazeAt(x, y, width, height) {
  const { cx, cy, a, b } = placeOf(width, height);
  const clamped = (part) => Math.max(-1, Math.min(1, part));
  return [clamped((x - cx) / a), clamped((y - cy) / b)];
}

// The plasma's canvas holds LIT of the screen's pixels and is stretched over the eye. Its light is
// soft, and the GPU fills fewer pixels a frame.
const LIT = 0.6;

// Starts the eye on a canvas and returns what stops it. The plasma goes on the canvas of class
// `plasma` beside it, where there is one.
export function startEye(canvas) {
  const pen = canvas.getContext("2d");
  const lit = canvas.parentElement?.querySelector("canvas.plasma") ?? null;
  const still = motionReduced();
  const draw = seeded(Date.now());
  const state = {
    open: still ? 1 : OPEN_FROM,
    shine: still ? 1 : 0,
    gaze: [0, 0],
    seconds: 0,
    irises: [irisAt([0, 0, 1], [0, 0, 0], 1, draw)],
    picture: irisPicture(draw),
    lava: lavaOf(draw),
    tongues: tonguesOf(draw),
    plasma: lit ? plasmaOn(lit) : null,
    corners: cornersOf(),
    arcs: [],
    sparkUntil: 0,
    draw,
    still,
    mouse: null,
    dt: 0,
  };
  let front = [0, 0, 1];
  let divideAt = DIVIDE_FROM;
  let rolling = false;
  let following = true;
  let way = 1;
  let loop = 0;
  let turnAt = OPENING + FOLLOWS;
  let last = clock();
  const begun = last;
  let frame = 0;

  const size = () => {
    const box = canvas.getBoundingClientRect();
    const scale = window.devicePixelRatio || 1;
    canvas.width = Math.round(box.width * scale);
    canvas.height = Math.round(box.height * scale);
    pen.setTransform(scale, 0, 0, scale, 0, 0);
    if (lit) {
      lit.width = Math.round(box.width * scale * LIT);
      lit.height = Math.round(box.height * scale * LIT);
    }
    return box;
  };
  let box = size();
  const resized = () => (box = size());
  window.addEventListener("resize", resized);
  // Where the mouse is, in the canvas's own pixels, or null once it leaves the window.
  const moved = (event) => (state.mouse = [event.clientX - box.left, event.clientY - box.top]);
  const left = () => (state.mouse = null);
  window.addEventListener("pointermove", moved);
  document.documentElement.addEventListener("pointerleave", left);

  // The eye retains its time on the animation's clock. While motion is stopped it draws the frame it
  // is at and holds there, and takes up from there when motion comes back.
  const tick = (moment) => {
    const now = clock(moment);
    // A frame's time is when the frame began, which can be a little before the clock was read at the
    // start. Neither the step nor the time since the start is let below zero.
    const dt = Math.max(0, Math.min(0.05, (now - last) / 1000));
    last = now;
    const seconds = Math.max(0, (now - begun) / 1000);
    state.seconds = seconds;
    state.dt = dt;
    state.open = seconds < OPENING ? mix(OPEN_FROM, 1, smooth(seconds / OPENING)) : 1;

    while (seconds >= divideAt) {
      const largest = Math.max(...state.irises.map((iris) => iris.whole));
      if (largest * HALF >= SMALLEST) {
        divide(state.irises, draw, seconds);
      }
      divideAt += DIVIDE_EVERY;
    }
    grow(state.irises, seconds);

    if (seconds > turnAt) {
      following = false;
      rolling = !rolling;
      way = draw() < 0.5 ? -1 : 1;
      const span = rolling ? ROLLS : STARES;
      turnAt = seconds + mix(span[0], span[1], draw());
    }
    let aim = [0, 0];
    if (rolling) {
      loop += way * ROLL * dt;
      aim = [Math.cos(loop) * 0.95, Math.sin(loop) * 0.85];
    } else if (following && state.mouse) {
      aim = gazeAt(state.mouse[0], state.mouse[1], box.width, box.height);
    }
    const pull = 1 - Math.exp(-dt * (rolling ? 10 : following ? FOLLOW : 24));
    state.gaze = state.gaze.map((g, at) => g + (aim[at] - g) * pull);
    state.shine += ((rolling ? DIM : 1) - state.shine) * (1 - Math.exp(-dt * GLINT));

    // The eye's spin is the turn from the last front to this one over the frame, its axis square to
    // both.
    const tall = tallOf(box);
    const next = frontOf(state.gaze, tall);
    const spin = dt > 0 ? times(unit(cross(front, next)), angle(front, next) / dt) : [0, 0, 0];
    front = next;
    float(state.irises, front, spin, dt, 0.92 * tall, tall);
    drawEye(pen, box.width, box.height, state);
    if (paused()) {
      frame = 0;
      whenMoving(() => {
        frame = requestAnimationFrame(tick);
      });
      return;
    }
    frame = requestAnimationFrame(tick);
  };

  if (still) {
    drawEye(pen, box.width, box.height, state);
  } else {
    frame = requestAnimationFrame(tick);
  }
  return () => {
    cancelAnimationFrame(frame);
    window.removeEventListener("resize", resized);
    window.removeEventListener("pointermove", moved);
    document.documentElement.removeEventListener("pointerleave", left);
  };
}
