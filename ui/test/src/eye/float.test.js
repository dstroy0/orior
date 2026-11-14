// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// How the irises float: the largest divides into two halves, each shrinking to its own size over the split; the
// irises stay on the eye's sphere, drift to its front, and retain apart; and the front follows a gaze.

import { divide, float, frontOf, grow, HALF, irisAt } from "../../../gui/src-js/eye/float.js";
import { angle, seeded } from "../../../gui/src-js/eye/math.js";

const { test, assert } = window.__harness;

const length = (p) => Math.hypot(...p);

test("the largest iris divides into two halves of HALF its size, twins of each other, starting as the whole and shrinking to their own", () => {
  const draw = seeded(3);
  const irises = [irisAt([0, 0, 1], [0, 0, 0], 1, draw), irisAt([0.6, 0, 0.8], [0, 0, 0], 0.5, draw)];
  divide(irises, draw, 2);
  assert.equal(irises.length, 3);
  const halves = irises.filter((iris) => iris.whole === HALF);
  assert.equal(halves.length, 2);
  assert.ok(halves[0].twin === halves[1] && halves[1].twin === halves[0]);
  assert.deepEqual(halves.map((iris) => [iris.size, iris.grown]), [[1, 0], [1, 0]], "each the whole's size at first");
  grow(irises, 2.3);
  assert.ok(halves.every((iris) => iris.size < 1 && iris.size > HALF), "part way through the split");
  grow(irises, 5);
  assert.ok(halves.every((iris) => iris.grown === 1 && Math.abs(iris.size - HALF) < 1e-12), "each at its own size once the split is over");
});

test("the irises stay on the sphere, drift to its front, and two that overlap push apart", () => {
  const draw = seeded(5);
  const irises = [irisAt([0.3, 0, Math.sqrt(1 - 0.09)], [0, 0, 0], 0.5, draw), irisAt([0.31, 0, Math.sqrt(1 - 0.31 ** 2)], [0, 0, 0], 0.5, draw)];
  const front = [0, 0, 1];
  const before = angle(irises[0].p, irises[1].p);
  for (let step = 0; step < 120; step += 1) {
    float(irises, front, [0, 0, 0], 1 / 60, 0.3, 0.3);
  }
  assert.ok(irises.every((iris) => Math.abs(length(iris.p) - 1) < 1e-9), "on the unit sphere");
  assert.ok(angle(irises[0].p, irises[1].p) > before, "pushed apart");
  assert.ok(irises.every((iris) => angle(iris.p, front) < 0.5), "near the front");
  assert.ok(irises.every((iris) => length(iris.v) <= 2.5 + 1e-9), "no faster than the fastest");
});

test("the front looks straight out for no gaze and turns across and down as the gaze does", () => {
  assert.deepEqual(frontOf([0, 0], 0.3).map((one) => Math.round(one * 1e9) / 1e9), [0, 0, 1]);
  const right = frontOf([1, 0], 0.3);
  const down = frontOf([0, 1], 0.3);
  assert.ok(right[0] > 0 && Math.abs(right[1]) < 1e-9, "across turns it sideways");
  assert.ok(down[1] !== 0 && Math.abs(down[0]) < 1e-9, "down turns it up or down");
  assert.ok(Math.abs(length(right) - 1) < 1e-9);
});
