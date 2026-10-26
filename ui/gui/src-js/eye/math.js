// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's arithmetic: a curve that eases in and out, a mix of two values, a seeded draw, and the vectors of points
// on the eye's sphere.

export const smooth = (x) => x * x * (3 - 2 * x);
export const mix = (a, b, x) => a + (b - a) * x;

export function seeded(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let mixed = state;
    mixed = Math.imul(mixed ^ (mixed >>> 15), mixed | 1);
    mixed ^= mixed + Math.imul(mixed ^ (mixed >>> 7), mixed | 61);
    return ((mixed ^ (mixed >>> 14)) >>> 0) / 4294967296;
  };
}

export const dot = (p, q) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
export const cross = (p, q) => [p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]];
export const plus = (p, q) => [p[0] + q[0], p[1] + q[1], p[2] + q[2]];
export const times = (p, k) => [p[0] * k, p[1] * k, p[2] * k];
export const unit = (p) => times(p, 1 / (Math.hypot(...p) || 1));
export const angle = (p, q) => Math.acos(Math.max(-1, Math.min(1, dot(p, q))));
// The part of a vector along the sphere at p, its part out from the sphere taken off.
export const flat = (v, p) => plus(v, times(p, -dot(v, p)));
// From p toward q along the sphere, as long as the angle between them.
export function toward(p, q) {
  const way = flat(q, p);
  const length = Math.hypot(...way);
  return length < 1e-9 ? [0, 0, 0] : times(way, angle(p, q) / length);
}
