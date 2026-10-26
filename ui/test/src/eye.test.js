// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye drawn on its canvas a frame at a time, and stopped by what starting it gives back.

import { startEye } from "../../gui/src-js/eye.js";

const { test, assert, ui } = window.__harness;

const painted = (canvas) => {
  const data = canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height).data;
  let count = 0;
  for (let at = 3; at < data.length; at += 4) {
    count += data[at] > 0 ? 1 : 0;
  }
  return count;
};

test("the eye draws on its canvas as it runs, and stops when told", async () => {
  const host = document.createElement("div");
  host.style.cssText = "position:fixed;left:0;top:0;width:300px;height:200px";
  const canvas = Object.assign(document.createElement("canvas"), { className: "eye" });
  canvas.style.cssText = "width:300px;height:200px";
  host.append(canvas);
  document.body.append(host);
  try {
    const stop = startEye(canvas);
    await ui.rest(400);
    assert.ok(canvas.width >= 300 && canvas.height >= 200, "sized to the page's pixels");
    assert.ok(painted(canvas) > 1000, "the eye drawn");
    stop();
    const kept = canvas.toDataURL();
    await ui.rest(200);
    assert.equal(canvas.toDataURL(), kept, "no frame after it stops");
  } finally {
    host.remove();
  }
});
