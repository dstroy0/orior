// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fuse along the foot of a run's output: burnt as far as the run's place, unlit with no run to follow.

import { makeFuse, REACH } from "../../gui/src-js/fuse.js";

const { test, assert } = window.__harness;

// How many pixels of the fuse's canvas are lit, in its left half and its right.
const litOf = (canvas) => {
  const gl = canvas.getContext("webgl");
  const pixels = new Uint8Array(canvas.width * canvas.height * 4);
  gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
  const lit = [0, 0];
  for (let at = 3; at < pixels.length; at += 4) {
    if (pixels[at] > 8) {
      lit[((at >> 2) % canvas.width) < canvas.width / 2 ? 0 : 1] += 1;
    }
  }
  return lit;
};

test("a fuse following a run is burnt as far as the run's place, and unlit with no run", async () => {
  let place = 0.25;
  const fuse = makeFuse(() => place);
  assert.equal(fuse.canvas.className, "fuse");
  assert.equal(fuse.canvas.getAttribute("aria-hidden"), "true");
  fuse.canvas.style.cssText = `position:fixed;left:0;top:0;width:${400 + 2 * REACH}px;height:${2 * REACH + 20}px`;
  document.body.append(fuse.canvas);
  try {
    fuse.follow({ lines: [], done: true, code: 1, stopped: false });
    const [left, right] = litOf(fuse.canvas);
    assert.ok(left > right, `burnt on the left (${left}) more than on the right (${right})`);
    fuse.flare();
    place = 1;
    fuse.follow({ lines: [], done: true, code: 1, stopped: false });
    const [, rightAfter] = litOf(fuse.canvas);
    assert.ok(rightAfter > right, "burnt across as the place moves on");
    fuse.follow(null);
    assert.deepEqual(litOf(fuse.canvas), [0, 0], "no run, no fire");
  } finally {
    fuse.follow(null);
    fuse.canvas.remove();
  }
});
