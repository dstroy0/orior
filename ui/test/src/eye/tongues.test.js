// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The plasma round the opening: its tongues drawn from a seed, spread along both lids and longest over the middle,
// and the plasma positioned down as it grows.

import { drawPlasma, tonguesOf } from "../../../gui/src-js/eye/tongues.js";
import { edges, placeOf } from "../../../gui/src-js/eye/lids.js";
import { seeded } from "../../../gui/src-js/eye/math.js";
import { cornersOf } from "../../../gui/src-js/plasma.js";

const { test, assert } = window.__harness;

test("the tongues are drawn from a seed, the same for the same seed, spread from corner to corner along each lid", () => {
  const tongues = tonguesOf(seeded(9));
  assert.deepEqual([tongues.upper.length, tongues.lower.length], [30, 26]);
  assert.deepEqual(JSON.stringify(tonguesOf(seeded(9))), JSON.stringify(tongues));
  for (const lid of [tongues.upper, tongues.lower]) {
    assert.ok(lid.every((one) => one.x > -1 && one.x < 1));
    assert.ok(lid.every((one, at) => at === 0 || one.x > lid[at - 1].x), "in order along the lid");
  }
  const middle = tongues.lower[13];
  const corner = tongues.lower[0];
  assert.ok(middle.length > corner.length, "longer over the middle");
});

test("the plasma positions down nothing while the eye is a slit and its tongues and rim once it opens", () => {
  const drawn = [];
  const state = { plasma: { draw: (list, count) => drawn.push(count) }, corners: cornersOf(), tongues: tonguesOf(seeded(2)), seconds: 1, still: true, arcs: [], sparkUntil: 0, draw: seeded(2) };
  const size = placeOf(400, 200);
  drawPlasma(size, edges(0.2), state, 0);
  drawPlasma(size, edges(1), state, 1);
  assert.equal(drawn[0], 0);
  assert.ok(drawn[1] > 0);
  drawPlasma(size, edges(1), { ...state, plasma: null }, 1);
  assert.equal(drawn.length, 2, "without a canvas to draw on, nothing is drawn");
});
