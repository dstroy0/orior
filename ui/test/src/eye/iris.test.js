// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// An iris: the bodies of its pupil drawn from a seed, its picture painted at a size, and the picture laid down for an
// iris on the eye's sphere.

import { drawIris, irisPicture, lavaOf } from "../../../gui/src-js/eye/iris.js";
import { irisAt } from "../../../gui/src-js/eye/float.js";
import { seeded } from "../../../gui/src-js/eye/math.js";

const { test, assert } = window.__harness;

test("the pupil's bodies are drawn from a seed, the first the largest near the middle", () => {
  const lava = lavaOf(seeded(4));
  assert.equal(lava.length, 4);
  assert.equal(lava[0].size, 0.3);
  assert.ok(lava.slice(1).every((one) => one.size < lava[0].size && one.across > lava[0].across));
  assert.equal(JSON.stringify(lavaOf(seeded(4))), JSON.stringify(lava));
});

test("an iris's picture is painted at a size, and laid down inside the iris's circle and nowhere else", () => {
  const draw = seeded(6);
  const picture = irisPicture(draw);
  picture.paint(80, lavaOf(draw), 0.5, 1);
  assert.ok(picture.canvas && picture.canvas.width >= 80, "painted at the size asked");
  const canvas = Object.assign(document.createElement("canvas"), { width: 200, height: 200 });
  const pen = canvas.getContext("2d");
  drawIris(pen, picture, irisAt([0, 0, 1], [0, 0, 0], 1, draw), 100, 100, 40, 0, 1);
  const alpha = (x, y) => pen.getImageData(x, y, 1, 1).data[3];
  assert.ok(alpha(100, 100) > 0, "the middle drawn");
  assert.ok(alpha(130, 100) > 0, "inside the circle");
  assert.equal(alpha(150, 150), 0, "nothing past it");
});
