// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The time ruler under the fuse: a run that has ended fitted whole, one still going retained with its latest moment near
// the right, and the keys that zoom it, move it and fit it again.

import { makeRuler } from "../../gui/src-js/ruler.js";

const { test, assert } = window.__harness;

async function withRuler(work) {
  const fuse = document.createElement("canvas");
  const ruler = makeRuler(fuse);
  ruler.element.style.cssText = "position:fixed;left:0;top:0;width:400px;height:40px";
  document.body.append(ruler.element);
  try {
    return await work(ruler, fuse);
  } finally {
    ruler.follow(null);
    ruler.element.remove();
  }
}

const key = (element, name) => element.dispatchEvent(new KeyboardEvent("keydown", { key: name, bubbles: true, cancelable: true }));
const ENDED = { lines: [{ ms: 100, stream: "stdout" }, { ms: 900, stream: "stderr" }], steps: [0], done: true, code: 0, endMs: 2000 };

test("the ruler holds the fuse and its own canvas, a group named for the run's time, and no run has no latest moment", async () => {
  await withRuler((ruler, fuse) => {
    assert.equal(ruler.element.getAttribute("role"), "group");
    assert.equal(ruler.element.ariaLabel, "Run time");
    assert.equal(fuse.parentElement, ruler.element);
    assert.ok(ruler.element.querySelector("canvas.ruler"));
    assert.equal(ruler.headAt(), null);
  });
});

test("a run that has ended is fitted whole, its end at the right; zoomed in, its end is past the right, and 0 fits it again", async () => {
  await withRuler((ruler) => {
    ruler.follow(ENDED);
    assert.equal(ruler.headAt(), 1);
    key(ruler.element, "+");
    assert.ok(ruler.headAt() > 1, "zoomed about the middle");
    key(ruler.element, "0");
    assert.equal(ruler.headAt(), 1);
  });
});

test("a run still going holds its latest moment near the right however long it runs", async () => {
  await withRuler((ruler) => {
    ruler.follow({ lines: [], steps: [0], done: false, clock: performance.now() - 3000 });
    const head = ruler.headAt();
    assert.ok(head > 0.85 && head < 1, `the latest moment ${head} of the way across`);
  });
});
