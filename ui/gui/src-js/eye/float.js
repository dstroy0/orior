// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational


import { angle, cross, dot, flat, mix, plus, smooth, times, toward, unit } from "./math.js";

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
export const DIVIDE_FROM = 1.9;
export const DIVIDE_EVERY = 0.9;
export const SMALLEST = 0.1;
export const HALF = 0.72;
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

// An iris's `whole` is the size it grows to, `size` the size it has now, and `grown` how far through
// its split it is, from 0 to 1. `twin` is the other half of the same split.
export const irisAt = (p, v, whole, draw) => ({ p, v, size: whole, whole, from: whole, born: -Infinity, grown: 1, twin: null, phase: draw() * Math.PI * 2, look: [0, 0] });

// The largest iris divides into two in one place, leaving each other at POP along a line near the
// level, which keeps their momentum.
export function divide(irises, draw, seconds) {
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
export function grow(irises, seconds) {
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
export function float(irises, front, spin, dt, ratio, tall) {
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
export const frontOf = (gaze, tall) => {
  const across = gaze[0] * 0.4;
  const down = gaze[1] * 0.26 * tall;
  return [Math.sin(across) * Math.cos(down), Math.sin(down), Math.cos(across) * Math.cos(down)];
};
