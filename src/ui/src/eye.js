// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye the app shows while it loads. It opens once, from a low slit to wide over OPENING
// seconds, and its lids hold still from then on. Once it is open its iris divides, and divides again,
// and it rolls and stares.
//
// Each iris floats free on the sphere of the eye, a disc with a mass of its area. It speeds up and
// slows down, and irises that meet push off each other, the heavier one moving less. When the eye
// rolls, friction draws each iris's velocity toward that of the sphere under it, by FRICTION: a
// roll leaves them sliding and bumping, and a stare lets them come to rest turning with the eye.
//
// An iris is a few clear discs in the lattice's three colors, signal green, the link blue and
// violet, each a little off the middle and turning against the next. Its pupil is dark and moves as
// a lava lamp does: one round body, and smaller ones that draw out of it, part from it as pupils of
// their own, and run back in, and it leans toward the mouse. A reader who asks the system for less
// motion gets the eye held still and open, with one iris.

const OPENING = 1.4;
const OPEN_FROM = 0.1;

const SIGNAL = [111, 220, 180];
const LINK = [138, 184, 255];
const VIOLET = [180, 156, 255];
const DARK = [7, 6, 26];

const smooth = (x) => x * x * (3 - 2 * x);
const mix = (a, b, x) => a + (b - a) * x;
const rgba = ([r, g, b], a) => `rgba(${r}, ${g}, ${b}, ${a})`;

function seeded(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let mixed = state;
    mixed = Math.imul(mixed ^ (mixed >>> 15), mixed | 1);
    mixed ^= mixed + Math.imul(mixed ^ (mixed >>> 7), mixed | 61);
    return ((mixed ^ (mixed >>> 14)) >>> 0) / 4294967296;
  };
}

// The iris's discs, from the largest in: its radius and how far its middle sits from the iris's,
// both as parts of the iris radius, how fast it turns in radians a second, the sign giving the way,
// its color and how clear it is.
const DISCS = [
  { size: 0.96, off: 0.04, speed: 0.3, color: VIOLET, alpha: 0.5 },
  { size: 0.84, off: 0.1, speed: -0.5, color: LINK, alpha: 0.5 },
  { size: 0.72, off: 0.13, speed: 0.75, color: SIGNAL, alpha: 0.5 },
  { size: 0.6, off: 0.12, speed: -1.05, color: VIOLET, alpha: 0.5 },
  { size: 0.5, off: 0.1, speed: 1.4, color: LINK, alpha: 0.5 },
];

// The pupil is the dark where the bodies' summed pull, each body's size squared over the squared
// distance to it, comes to 1 or more. It is worked out on a FIELD by FIELD grid over the middle of
// the iris, REACH of its radius each way, and drawn scaled up, which leaves its edge soft.
const FIELD = 72;
const REACH = 0.62;

// Each pupil leans toward the mouse, as far as LOOK of its iris's radius when the mouse is three
// radii off or more and less when it is nearer, closing LEAN of the gap a second.
const LOOK = 0.3;
const LEAN = 6;

// The bodies of the pupil. Each wanders on two slow waves, one across and one down, and swells and
// shrinks a little. The first is large and near the middle; the others swing out far enough to part
// from it.
function lavaOf(draw) {
  const body = (wide, size) => ({
    across: wide * (0.7 + draw() * 0.3),
    down: wide * (0.5 + draw() * 0.4),
    rates: [0.35 + draw() * 0.7, 0.3 + draw() * 0.7, 0.8 + draw() * 0.8],
    phases: [draw() * Math.PI * 2, draw() * Math.PI * 2],
    size,
  });
  return [body(0.06, 0.3), body(0.3, 0.13 + draw() * 0.05), body(0.32, 0.12 + draw() * 0.05), body(0.26, 0.11 + draw() * 0.04)];
}

// Every iris is one picture, drawn once a frame at the size of a whole iris and then laid down for
// each iris turned by its own angle, squeezed and scaled. A divided eye costs a frame what one iris
// costs, however many irises it holds.
function irisPicture() {
  const canvas = document.createElement("canvas");
  const pen = canvas.getContext("2d");
  const field = document.createElement("canvas");
  field.width = FIELD;
  field.height = FIELD;
  const fieldPen = field.getContext("2d");
  const image = fieldPen.createImageData(FIELD, FIELD);

  const drawLava = (lava, seconds) => {
    const bodies = lava.map((one) => [
      one.across * Math.sin(one.rates[0] * seconds + one.phases[0]),
      one.down * Math.sin(one.rates[1] * seconds + one.phases[1]),
      (one.size * (1 + 0.1 * Math.sin(one.rates[2] * seconds + one.phases[1]))) ** 2,
    ]);
    const data = image.data;
    for (let row = 0; row < FIELD; row += 1) {
      const v = ((row + 0.5) / FIELD) * 2 * REACH - REACH;
      for (let column = 0; column < FIELD; column += 1) {
        const u = ((column + 0.5) / FIELD) * 2 * REACH - REACH;
        let pull = 0;
        for (const [x, y, size] of bodies) {
          pull += size / ((u - x) ** 2 + (v - y) ** 2 + 1e-6);
        }
        const at = (row * FIELD + column) * 4;
        data[at] = DARK[0];
        data[at + 1] = DARK[1];
        data[at + 2] = DARK[2];
        data[at + 3] = 255 * Math.max(0, Math.min(1, (pull - 0.85) / 0.3));
      }
    }
    fieldPen.putImageData(image, 0, 0);
  };

  // Draws the picture for this moment, `across` pixels wide.
  const paint = (across, lava, seconds) => {
    const side = Math.max(16, Math.ceil(across));
    if (canvas.width !== side) {
      canvas.width = side;
      canvas.height = side;
    }
    const r = side / 2;
    pen.setTransform(1, 0, 0, 1, r, r);
    pen.clearRect(-r, -r, side, side);
    pen.save();
    pen.beginPath();
    pen.arc(0, 0, r, 0, Math.PI * 2);
    pen.clip();
    pen.fillStyle = "#0c0930";
    pen.fillRect(-r, -r, side, side);
    DISCS.forEach((disc, at) => {
      const spin = at * 1.3 + disc.speed * seconds;
      pen.fillStyle = rgba(disc.color, disc.alpha);
      pen.beginPath();
      pen.arc(Math.cos(spin) * disc.off * r, Math.sin(spin) * disc.off * r, disc.size * r, 0, Math.PI * 2);
      pen.fill();
    });
    const rim = pen.createRadialGradient(0, 0, r * 0.75, 0, 0, r);
    rim.addColorStop(0, "rgba(10, 8, 34, 0)");
    rim.addColorStop(1, "rgba(10, 8, 34, 0.7)");
    pen.fillStyle = rim;
    pen.fillRect(-r, -r, side, side);
    drawLava(lava, seconds);
    pen.restore();
  };
  return { canvas, pupil: field, paint };
}

// An iris on the sphere: the discs turned by the iris's own angle, then squeezed along the line out
// from the sphere's middle through it, `turn`, by `face`, how squarely the disc faces the reader, as
// a disc on a sphere is. The pupil lies over them shifted by the iris's `look`, inside the iris. The
// highlight on the cornea over it is not squeezed or turned, because a reflection stays where the
// light is.
function drawIris(pen, picture, iris, x, y, r, turn, face) {
  pen.save();
  pen.translate(x, y);
  pen.rotate(turn);
  pen.scale(face, 1);
  pen.rotate(-turn);
  pen.save();
  pen.rotate(iris.phase);
  pen.drawImage(picture.canvas, -r, -r, r * 2, r * 2);
  pen.restore();
  pen.beginPath();
  pen.arc(0, 0, r * 0.97, 0, Math.PI * 2);
  pen.clip();
  pen.translate(iris.look[0] * r, iris.look[1] * r);
  pen.rotate(iris.phase);
  pen.drawImage(picture.pupil, -REACH * r, -REACH * r, 2 * REACH * r, 2 * REACH * r);
  pen.restore();

  const hx = x - r * 0.32 * face;
  const hy = y - r * 0.36;
  const shine = pen.createRadialGradient(hx, hy, 0, hx, hy, r * 0.11);
  shine.addColorStop(0, "rgba(255, 255, 255, 0.75)");
  shine.addColorStop(1, "rgba(255, 255, 255, 0)");
  pen.fillStyle = shine;
  pen.beginPath();
  pen.arc(hx, hy, r * 0.11, 0, Math.PI * 2);
  pen.fill();
}

// How the irises float. An iris is a point on the eye's unit sphere, a velocity along the sphere in
// radians a second, and a size, its radius as a part of one whole iris's. Its mass is its area,
// size squared. The eye opens with one iris. From DIVIDE_FROM seconds on, every DIVIDE_EVERY
// seconds the largest iris splits into two of HALF its size, until a half would be smaller than
// SMALLEST. Two halves hold about the area of the one they came from, and the irises fill the
// opening about as much however many there are.
//
// A split takes SPLIT seconds. The halves start as the whole iris in one place, leaving each other at
// POP; over the split each shrinks to its own size, and the room the two keep from each other grows
// from none to their full reach. They draw apart instead of being thrown apart, and no iris moves
// faster than FASTEST radians a second, however hard a crowd squeezes it.
//
// FRICTION: each second, how much of the gap between an iris's velocity and that of the sphere's
// surface under it closes. High, and the irises turn with the eye as if fixed to it; low, and the
// eye rolls under them and they catch up late.
// WELL: the pull toward the front of the eye, in radians a second squared for each radian away.
// LEVEL: the pull toward the level line across the eye's front, the same way. The opening is wide
// and low, and an iris may slide part way under a lid, but not out of sight.
// RIM: how far from the front, in radians, an iris's edge may go before the rim pushes it back.
// RISE: how far above or below the level line, as a part of the opening's half height, an iris's
// middle may go before WALL pushes it back, harder than any two irises push each other.
// STIFF and DAMP: how hard two irises that overlap push apart, and how much of their closing speed
// a push takes up.
const DIVIDE_FROM = 1.9;
const DIVIDE_EVERY = 0.9;
const SMALLEST = 0.1;
const HALF = 0.72;
const SPLIT = 0.6;
const POP = 0.35;
const FASTEST = 2.5;
const FRICTION = 5;
const WELL = 7;
const LEVEL = 10;
const RIM = 1.1;
const RISE = 0.75;
const WALL = 6000;
const STIFF = 700;
const DAMP = 10;
// The steps each frame is cut into, which keeps a hard push from passing one iris through another.
const STEPS = 4;

const dot = (p, q) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
const cross = (p, q) => [p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]];
const plus = (p, q) => [p[0] + q[0], p[1] + q[1], p[2] + q[2]];
const times = (p, k) => [p[0] * k, p[1] * k, p[2] * k];
const unit = (p) => times(p, 1 / (Math.hypot(...p) || 1));
const angle = (p, q) => Math.acos(Math.max(-1, Math.min(1, dot(p, q))));
// The part of a vector along the sphere at p, its part out from the sphere taken off.
const flat = (v, p) => plus(v, times(p, -dot(v, p)));
// From p toward q along the sphere, as long as the angle between them.
function toward(p, q) {
  const way = flat(q, p);
  const length = Math.hypot(...way);
  return length < 1e-9 ? [0, 0, 0] : times(way, angle(p, q) / length);
}

// An iris's `whole` is the size it grows to, `size` the size it has now, and `grown` how far through
// its split it is, from 0 to 1. `twin` is the other half of the same split.
const irisAt = (p, v, whole, draw) => ({ p, v, size: whole, whole, from: whole, born: -Infinity, grown: 1, twin: null, phase: draw() * Math.PI * 2, look: [0, 0] });

// The largest iris divides into two in one place, leaving each other at POP along a line near the
// level, which keeps their momentum.
function divide(irises, draw, seconds) {
  const largest = irises.reduce((most, iris) => (iris.whole > most.whole ? iris : most));
  const level = unit(cross([0, 1, 0], largest.p));
  const upright = cross(largest.p, level);
  const turn = (draw() - 0.5) * 0.8;
  const line = plus(times(level, Math.cos(turn)), times(upright, Math.sin(turn)));
  irises.splice(irises.indexOf(largest), 1);
  const halves = [-1, 1].map((side) => {
    const half = irisAt(unit(plus(largest.p, times(line, side * 0.002))), plus(largest.v, times(line, side * POP)), largest.whole * HALF, draw);
    return Object.assign(half, { size: largest.size, from: largest.size, born: seconds, grown: 0, look: [...largest.look] });
  });
  halves[0].twin = halves[1];
  halves[1].twin = halves[0];
  irises.push(...halves);
}

// Each half of a split shrinks from the size it began at to its own over SPLIT seconds.
function grow(irises, seconds) {
  for (const iris of irises) {
    if (iris.grown < 1) {
      iris.grown = Math.min(1, (seconds - iris.born) / SPLIT);
      iris.size = mix(iris.from, iris.whole, smooth(iris.grown));
    }
  }
}

// Moves the irises on by dt seconds while the eye's front is at `front` and the eye turns at `spin`,
// radians a second about that axis. `ratio` is one whole iris's radius over the sphere's, and
// `tall` the opening's half height as an angle on the sphere.
function float(irises, front, spin, dt, ratio, tall) {
  // The eye's own up, square to its front: the level line is where a point's part along it is 0.
  const up = unit(flat([0, -1, 0], front));
  const step = dt / STEPS;
  for (let at = 0; at < STEPS; at += 1) {
    const speeds = irises.map((iris) => {
      let push = times(plus(cross(spin, iris.p), times(iris.v, -1)), FRICTION);
      const home = toward(iris.p, front);
      push = plus(push, times(home, WELL));
      const out = angle(iris.p, front) + iris.size * ratio - RIM;
      if (out > 0) {
        push = plus(push, times(unit(home), STIFF * out));
      }
      const rise = Math.asin(Math.max(-1, Math.min(1, dot(iris.p, up))));
      const down = times(unit(flat(up, iris.p)), -Math.sign(rise));
      push = plus(push, times(down, LEVEL * Math.abs(rise)));
      const over = Math.abs(rise) - RISE * tall;
      if (over > 0) {
        push = plus(push, times(down, WALL * over));
      }
      return push;
    });
    for (let i = 0; i < irises.length; i += 1) {
      for (let j = i + 1; j < irises.length; j += 1) {
        const one = irises[i];
        const two = irises[j];
        const gap = angle(one.p, two.p);
        const reach = (one.size + two.size) * ratio * (one.twin === two ? smooth(one.grown) : 1);
        if (gap >= reach) {
          continue;
        }
        const awayOne = times(unit(toward(one.p, two.p)), -1);
        const awayTwo = times(unit(toward(two.p, one.p)), -1);
        const closing = -(dot(one.v, awayOne) + dot(two.v, awayTwo));
        const force = Math.max(0, STIFF * (reach - gap) + DAMP * closing);
        speeds[i] = plus(speeds[i], times(awayOne, force / one.size ** 2));
        speeds[j] = plus(speeds[j], times(awayTwo, force / two.size ** 2));
      }
    }
    irises.forEach((iris, index) => {
      iris.v = flat(plus(iris.v, times(speeds[index], step)), iris.p);
      const speed = Math.hypot(...iris.v);
      if (speed > FASTEST) {
        iris.v = times(iris.v, FASTEST / speed);
      }
      iris.p = unit(plus(iris.p, times(iris.v, step)));
      iris.v = flat(iris.v, iris.p);
    });
  }
}

// The front of the eye for a gaze, gaze[0] across and gaze[1] down, each from -1 to 1. Across it
// turns as far as 0.4 of the eye's half width, and down as far as 0.26 of its half height, `tall`.
const frontOf = (gaze, tall) => {
  const across = gaze[0] * 0.4;
  const down = gaze[1] * 0.26 * tall;
  return [Math.sin(across) * Math.cos(down), Math.sin(down), Math.cos(across) * Math.cos(down)];
};

// The sphere of the eye in pixels: its middle and radius, for the eye's half width a and half
// height b.
const sphereOf = (cx, cy, a, b) => ({ x: cx, y: cy - b * 0.12, radius: a * 1.05 });

// The opening's half height as an angle on the sphere, for a canvas of this size.
const tallOf = (box) => (0.34 * box.height) / sphereOf(0, 0, box.width * 0.42, 0).radius;

// The edge of the opening, in parts of the eye's half width across and half height down, from x = -1
// at the inner corner to 1 at the outer, at an openness from OPEN_FROM to 1. The outer corner sits
// higher than the inner. The upper lid is pressed down toward the inner corner and stands highest
// toward the outer one; the lower lid is nearly straight. Near each corner both lids turn steeply
// into it, which leaves the corners blunt.
function edges(open) {
  const upper = [];
  const lower = [];
  for (let step = 0; step <= 48; step += 1) {
    const x = (step / 48) * 2 - 1;
    const s = (x + 1) / 2;
    const corner = mix(0.1, -0.12, s);
    upper.push([x, corner - mix(0.08, 0.72, open) * (1 - Math.abs(x) ** 3.2) ** 0.5 * (0.5 + 0.5 * smooth(s))]);
    lower.push([x, corner + mix(0.12, 0.4, open) * (1 - Math.abs(x) ** 3) ** 0.5 * (1 - 0.15 * s)]);
  }
  return { upper, lower };
}

function trace(pen, points, cx, cy, a, b, start = true) {
  points.forEach(([x, y], at) => (at === 0 && start ? pen.moveTo(cx + x * a, cy + y * b) : pen.lineTo(cx + x * a, cy + y * b)));
}

function openingPath(pen, lid, cx, cy, a, b) {
  pen.beginPath();
  trace(pen, lid.upper, cx, cy, a, b);
  trace(pen, [...lid.lower].reverse(), cx, cy, a, b, false);
  pen.closePath();
}

// Under the irises: the socket, a soft dark round the opening that the eye sits in instead of on the
// page, and the white as the front of a sphere, lit from up and to the left and falling into shadow
// round the edge.
function drawUnder(pen, size, lid) {
  const { cx, cy, a, b, width, height } = size;
  pen.save();
  pen.filter = `blur(${Math.max(4, b * 0.35)}px)`;
  openingPath(pen, edges(1), cx, cy, a * 1.06, b * 1.5);
  pen.fillStyle = "rgba(5, 4, 20, 0.8)";
  pen.fill();
  pen.restore();

  pen.save();
  openingPath(pen, lid, cx, cy, a, b);
  pen.clip();
  const white = pen.createRadialGradient(cx - a * 0.15, cy - b * 0.3, b * 0.1, cx, cy, a * 1.02);
  white.addColorStop(0, "#e6e3f6");
  white.addColorStop(0.5, "#b9b3e0");
  white.addColorStop(1, "#3c3672");
  pen.fillStyle = white;
  pen.fillRect(0, 0, width, height);
  pen.restore();
}

// Over the irises: the shadow the upper lid throws onto the eye, deep and wide, and the lid edges,
// the upper one heavy and the lower one thin.
function drawOver(pen, size, lid) {
  const { cx, cy, a, b } = size;
  pen.save();
  openingPath(pen, lid, cx, cy, a, b);
  pen.clip();
  pen.filter = `blur(${Math.max(2, b * 0.16)}px)`;
  pen.lineCap = "round";
  pen.strokeStyle = "rgba(6, 5, 22, 0.78)";
  pen.lineWidth = b * 0.6;
  pen.beginPath();
  trace(pen, lid.upper, cx, cy, a, b);
  pen.stroke();
  pen.restore();

  pen.lineJoin = "round";
  pen.lineCap = "round";
  pen.strokeStyle = rgba(DARK, 1);
  pen.lineWidth = Math.max(3, b * 0.11);
  pen.beginPath();
  trace(pen, lid.upper, cx, cy, a, b);
  pen.stroke();
  pen.lineWidth = Math.max(1.5, b * 0.05);
  pen.beginPath();
  trace(pen, lid.lower, cx, cy, a, b);
  pen.stroke();
}

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
// once into two layers, which each frame lays down as they are. The blurs in them, the dearest work
// in a frame, do not run again.
function drawEye(pen, width, height, state) {
  pen.clearRect(0, 0, width, height);
  const size = { cx: width / 2, cy: height / 2, a: width * 0.42, b: height * 0.34, width, height };
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
  const shown = state.irises.filter((iris) => iris.p[2] > 0.05).sort((one, two) => one.p[2] - two.p[2]);
  state.picture.paint(2 * r * (window.devicePixelRatio || 1), state.lava, state.seconds);
  const ease = 1 - Math.exp(-state.dt * LEAN);
  for (const iris of shown) {
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
}

// Once open, the eye rolls and stares by turns. A roll carries the gaze round a loop at ROLL
// radians a second, one way or the other, for ROLLS seconds; a stare snaps it to the middle, straight
// at the reader, for STARES seconds. Each is a span from the first number to the second.
const ROLL = 3;
const ROLLS = [1.6, 2.8];
const STARES = [0.8, 1.6];

// Starts the eye on a canvas and returns what stops it.
export function startEye(canvas) {
  const pen = canvas.getContext("2d");
  const still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const draw = seeded(Date.now());
  const state = {
    open: still ? 1 : OPEN_FROM,
    gaze: [0, 0],
    seconds: 0,
    irises: [irisAt([0, 0, 1], [0, 0, 0], 1, draw)],
    picture: irisPicture(),
    lava: lavaOf(draw),
    mouse: null,
    dt: 0,
  };
  let front = [0, 0, 1];
  let divideAt = DIVIDE_FROM;
  let rolling = false;
  let way = 1;
  let loop = 0;
  let turnAt = OPENING + 0.4;
  let last = performance.now();
  const begun = last;
  let frame = 0;

  const size = () => {
    const box = canvas.getBoundingClientRect();
    const scale = window.devicePixelRatio || 1;
    canvas.width = Math.round(box.width * scale);
    canvas.height = Math.round(box.height * scale);
    pen.setTransform(scale, 0, 0, scale, 0, 0);
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

  const tick = (now) => {
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
      rolling = !rolling;
      way = draw() < 0.5 ? -1 : 1;
      const span = rolling ? ROLLS : STARES;
      turnAt = seconds + mix(span[0], span[1], draw());
    }
    let aim = [0, 0];
    if (rolling) {
      loop += way * ROLL * dt;
      aim = [Math.cos(loop) * 0.95, Math.sin(loop) * 0.85];
    }
    const pull = 1 - Math.exp(-dt * (rolling ? 10 : 24));
    state.gaze = state.gaze.map((g, at) => g + (aim[at] - g) * pull);

    // The eye's spin is the turn from the last front to this one over the frame, its axis square to
    // both.
    const tall = tallOf(box);
    const next = frontOf(state.gaze, tall);
    const spin = dt > 0 ? times(unit(cross(front, next)), angle(front, next) / dt) : [0, 0, 0];
    front = next;
    float(state.irises, front, spin, dt, 0.92 * tall, tall);
    drawEye(pen, box.width, box.height, state);
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
