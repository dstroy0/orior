// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's opening and its lids: where the eye is in its canvas, the opening's edge at an openness, each lid
// with the way out of the opening along it, the lids' bands, and the white drawn under the irises and the lids over.

import { along, drawOver, drawUnder, edges, lidOf, LOWER_BAND, placeOf, sphereOf, tallOf, UPPER_BAND } from "../../../gui/src-js/eye/lids.js";

const { test, assert } = window.__harness;

test("the eye is above its canvas's middle, its sphere a little wider than its opening", () => {
  const size = placeOf(400, 200);
  assert.deepEqual([size.cx, size.cy, size.a, size.b], [200, 90, 132, 36]);
  const sphere = sphereOf(size.cx, size.cy, size.a, size.b);
  assert.ok(sphere.radius > size.a && sphere.y < size.cy);
  assert.equal(tallOf({ width: 400, height: 200 }), size.b / sphere.radius);
});

test("the lids meet at each corner, the outer a little higher, and part further as the eye opens", () => {
  const half = edges(0.5);
  const full = edges(1);
  assert.equal(full.upper[0][1], full.lower[0][1], "they meet at the inner corner");
  assert.equal(full.upper.at(-1)[1], full.lower.at(-1)[1], "and at the outer");
  assert.ok(full.upper.at(-1)[1] < full.upper[0][1], "the outer corner higher");
  const middle = full.upper.length >> 1;
  assert.ok(full.upper[middle][1] < full.lower[middle][1], "the upper lid above the lower");
  assert.ok(full.lower[middle][1] - full.upper[middle][1] > half.lower[middle][1] - half.upper[middle][1], "wider open");
});

test("each lid's way out points out of the opening, up from the upper and down from the lower, and a point between two is read along", () => {
  const size = placeOf(400, 200);
  const { upper, lower } = edges(1);
  const top = lidOf(upper, size, 1);
  const foot = lidOf(lower, size, -1);
  const middle = top.length >> 1;
  assert.ok(top[middle].n[1] < -0.9, "up");
  assert.ok(foot[middle].n[1] > 0.9, "down");
  const at = along(top, 0);
  assert.ok(Math.abs(at.p[0] - size.cx) < 1e-9, "the middle of the lid at x 0");
  assert.ok(Math.abs(Math.hypot(...at.n) - 1) < 1e-9);
  assert.ok(UPPER_BAND(0) > LOWER_BAND(0) && UPPER_BAND(0) > UPPER_BAND(0.9), "the upper lid heavy over the middle");
});

test("the white is drawn inside the opening and the lids over its edge", () => {
  const canvas = Object.assign(document.createElement("canvas"), { width: 400, height: 200 });
  const pen = canvas.getContext("2d");
  const size = placeOf(400, 200);
  const lid = edges(1);
  drawUnder(pen, size, lid);
  const inside = pen.getImageData(size.cx, size.cy, 1, 1).data;
  assert.ok(inside[3] > 0, "the white in the middle of the opening");
  assert.equal(pen.getImageData(5, 195, 1, 1).data[3], 0, "nothing far outside it");
  drawOver(pen, size, lid);
  const lidAt = lidOf(lid.upper, size, 1)[lid.upper.length >> 1].p;
  assert.ok(pen.getImageData(Math.round(lidAt[0]), Math.round(lidAt[1]) - 2, 1, 1).data[3] > 0, "the upper lid drawn on its edge");
});
