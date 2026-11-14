// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's plasma: its triangles written corner by corner into one list that grows when it fills, an arc and a spot
// as pieces of them, and the list drawn by the GPU onto a canvas.

import { addArc, addSpot, CORNER, cornersOf, orchid, plasmaOn } from "../../gui/src-js/plasma.js";

const { test, assert } = window.__harness;

test("a corner is written as its place, its place in the shape, its color from 0 to 1 and its strength", () => {
  const corners = cornersOf();
  corners.add([10, 20, -1, 0.5, [255, 0, 51], 0.75]);
  assert.equal(corners.count, 1);
  assert.deepEqual([...corners.list.subarray(0, CORNER)].map((one) => Math.round(one * 1000) / 1000), [10, 20, -1, 0.5, 1, 0, 0.2, 0.75]);
  corners.shift(5, -5);
  assert.deepEqual([corners.list[0], corners.list[1]], [15, 15]);
  corners.clear();
  assert.equal(corners.count, 0);
});

test("a piece of four sides is two triangles, an arc a piece along each of its lines, and a spot a fan of sixteen", () => {
  const corners = cornersOf();
  addArc(corners, [[0, 0], [10, 0], [10, 10]], 1, [1, 2, 3], 1);
  assert.equal(corners.count, 12, "two lines, six corners each");
  corners.clear();
  addSpot(corners, 5, 5, 3, [1, 2, 3], 1);
  assert.equal(corners.count, 48);
});

test("the list grows when it fills, retention what it retained", () => {
  const corners = cornersOf();
  for (let at = 0; at < 5000; at += 1) {
    corners.add([at, 0, 0, 0, [0, 0, 0], 1]);
  }
  assert.equal(corners.count, 5000);
  assert.ok(corners.list.length >= 5000 * CORNER);
  assert.equal(corners.list[4999 * CORNER], 4999);
});

test("the GPU draws the triangles onto the canvas, and a canvas has one drawer", () => {
  const canvas = Object.assign(document.createElement("canvas"), { width: 64, height: 64 });
  const drawer = plasmaOn(canvas);
  assert.ok(drawer, "WebGL is there to draw with");
  assert.equal(plasmaOn(canvas), drawer);
  const corners = cornersOf();
  addSpot(corners, 32, 32, 20, orchid(), 1);
  drawer.draw(corners.list, corners.count, 64, 64);
  const gl = canvas.getContext("webgl");
  const pixel = new Uint8Array(4);
  gl.readPixels(32, 32, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
  assert.ok(pixel[3] > 0, "the spot's middle drawn");
  drawer.draw(corners.list, 0, 64, 64);
  gl.readPixels(32, 32, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
  assert.equal(pixel[3], 0, "nothing to draw clears it");
});
