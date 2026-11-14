// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's arithmetic: the easing curve, a mix, a seeded draw, and the vectors of points on the eye's sphere.

import { angle, cross, dot, flat, mix, plus, seeded, smooth, times, toward, unit } from "../../../gui/src-js/eye/math.js";

const { test, assert } = window.__harness;

const near = (a, b) => a.every((one, at) => Math.abs(one - b[at]) < 1e-9);

test("the curve eases in and out, from 0 to 1 through a half at the half, and a mix goes from one value to the other", () => {
  assert.deepEqual([smooth(0), smooth(0.5), smooth(1)], [0, 0.5, 1]);
  assert.ok(smooth(0.1) < 0.1 && smooth(0.9) > 0.9, "slow at both ends");
  assert.deepEqual([mix(2, 6, 0), mix(2, 6, 0.25), mix(2, 6, 1)], [2, 3, 6]);
});

test("a seeded draw gives the same values for the same seed, each in [0, 1), and others for another seed", () => {
  const first = seeded(7);
  const again = seeded(7);
  const drawn = Array.from({ length: 50 }, () => first());
  assert.deepEqual(Array.from({ length: 50 }, () => again()), drawn);
  assert.ok(drawn.every((one) => one >= 0 && one < 1));
  const other = seeded(8);
  assert.notEqual(JSON.stringify(Array.from({ length: 50 }, () => other())), JSON.stringify(drawn));
});

test("the vectors: dot, cross, sum, scale, unit length and the angle between two", () => {
  assert.equal(dot([1, 2, 3], [4, 5, 6]), 32);
  assert.deepEqual(cross([1, 0, 0], [0, 1, 0]), [0, 0, 1]);
  assert.deepEqual(plus([1, 2, 3], [1, 1, 1]), [2, 3, 4]);
  assert.deepEqual(times([1, 2, 3], 2), [2, 4, 6]);
  assert.ok(near(unit([3, 0, 4]), [0.6, 0, 0.8]));
  assert.deepEqual(unit([0, 0, 0]), [0, 0, 0], "no length, no direction");
  assert.ok(Math.abs(angle([1, 0, 0], [0, 1, 0]) - Math.PI / 2) < 1e-9);
});

test("a vector positioned flat on the sphere loses its part out from it, and the way from one point to another is as long as their angle", () => {
  assert.ok(near(flat([1, 1, 0], [0, 1, 0]), [1, 0, 0]));
  const way = toward([0, 0, 1], [1, 0, 0]);
  assert.ok(near(unit(way), [1, 0, 0]), "along the sphere toward the other point");
  assert.ok(Math.abs(Math.hypot(...way) - Math.PI / 2) < 1e-9);
  assert.deepEqual(toward([0, 0, 1], [0, 0, 1]), [0, 0, 0], "a point toward itself goes nowhere");
});
