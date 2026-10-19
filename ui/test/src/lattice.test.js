// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { drawLattice, keepLattice, shows } from "../../gui/src-js/lattice.js";

const { test, assert, ui } = window.__harness;

// The frames drawn over `seconds` of the loop from `from`, a frame offered every 1/30 s and drawn
// only where it shows, and the frames offered.
function framesOver(from, seconds) {
  let drawnAt = from;
  let drawn = 0;
  const offered = Math.round(seconds * 30);
  for (let frame = 1; frame <= offered; frame += 1) {
    const now = from + frame / 30;
    if (shows(drawnAt, now)) {
      drawn += 1;
      drawnAt = now;
    }
  }
  return { drawn, offered };
}

test("a lattice at rest draws no frame", () => {
  assert.equal(shows(0.5, 3.5), false, "the loop's rest moves nothing");
  assert.equal(framesOver(0, 4).drawn, 0);
});

test("a frame is drawn once the loop has moved a point by a whole pixel", () => {
  assert.equal(shows(8, 8.001), false, "a millisecond of the spread moves no point a pixel");
  assert.equal(shows(8, 9), true, "a second of the spread moves points");
});

test("over the whole loop about one frame in three is drawn", () => {
  const loop = framesOver(0, 50);
  assert.ok(loop.drawn > 0, "the lattice moves");
  assert.ok(loop.drawn < loop.offered * 0.4, `${loop.drawn} of ${loop.offered} frames drawn`);
});

test("no frame drawn moves the lattice less than a pixel from the one before", () => {
  let drawnAt = 0;
  for (let frame = 1; frame <= 1500; frame += 1) {
    const now = frame / 30;
    if (shows(drawnAt, now)) {
      assert.ok(!shows(drawnAt, drawnAt + 0.0001), "a frame a moment after another shows nothing");
      drawnAt = now;
    }
  }
});

test("a lattice is laid out and drawn on its canvas", async () => {
  const canvas = Object.assign(document.createElement("canvas"), { className: "harness-lattice" });
  canvas.style.cssText = "position:fixed;left:0;top:0;width:240px;height:160px;pointer-events:none";
  document.body.append(canvas);
  try {
    drawLattice(canvas);
    assert.equal(canvas.width, Math.round(240 * (window.devicePixelRatio || 1)));
    const copy = Object.assign(document.createElement("canvas"), { width: canvas.width, height: canvas.height });
    const pen = copy.getContext("2d");
    pen.drawImage(canvas, 0, 0);
    const pixels = pen.getImageData(0, 0, copy.width, copy.height).data;
    let inked = 0;
    for (let at = 3; at < pixels.length; at += 4) {
      inked += pixels[at] > 0 ? 1 : 0;
    }
    assert.ok(inked > 100, `${inked} pixels inked`);
  } finally {
    canvas.remove();
  }
});

test("a lattice kept is laid out again when its size changes", async () => {
  const canvas = Object.assign(document.createElement("canvas"), { className: "harness-lattice" });
  canvas.style.cssText = "position:fixed;left:0;top:0;width:200px;height:100px;pointer-events:none";
  document.body.append(canvas);
  try {
    keepLattice(canvas);
    await ui.waitFor(() => canvas.width === Math.round(200 * (window.devicePixelRatio || 1)));
    canvas.style.width = "320px";
    await ui.waitFor(() => canvas.width === Math.round(320 * (window.devicePixelRatio || 1)));
  } finally {
    canvas.remove();
  }
});
