// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The work's line bending round the controls' tab as the controls come down.

import { revealed } from "../../gui/src-js/claim.js";
import { editingHello, GAP, middle, restedAt, SLIDE } from "../claims.js";

const { test, assert, ui } = window.__harness;

test("as the controls come down the work gives way round their tab, its line bending round the tab's side and foot, the rest of its top where it stood", async () => {
  await editingHello();
  const work = document.querySelector(".work");
  const top = work.getBoundingClientRect().top;
  await restedAt(window.innerWidth - 40, 4, "controls");
  const controls = document.getElementById("window-controls").getBoundingClientRect();
  // the tab's lip reaches 8px left of the controls; the work gives way the gap round the tab
  const side = controls.left - 8 - GAP;
  const foot = controls.bottom + GAP;
  const inWork = (x, y) => work.contains(document.elementFromPoint(x, y));
  assert.ok(!inWork(side + GAP / 2, controls.bottom - 4), "beside the tab the work has given way");
  assert.ok(!inWork(controls.left + 20, foot - GAP / 2), "under the tab's foot the work has given way");
  assert.ok(inWork(side - 20, top + 6), "left of the tab the work's top stands where it stood");
  assert.ok(inWork(controls.left + 20, foot + 10), "past the gap under the tab the work goes on");
  assert.equal(work.getBoundingClientRect().top, top, "the work's top where it stood");
  // the top line, drawn inside the outline, runs down the tab's side and along under its foot
  const line = document.querySelector(".work .work-line .top");
  const box = work.getBoundingClientRect();
  const onLine = (x, y) => line.isPointInStroke(new DOMPoint(x - box.left, y - box.top));
  assert.ok(onLine(side - 1, controls.bottom - 4), "the top line down the tab's side");
  assert.ok(onLine(controls.left + 20, foot + 1), "the top line along under the tab's foot");
  assert.ok(onLine(side - 40, top + 1), "the top line where it stood left of the tab");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
  await ui.rest(SLIDE);
  assert.ok(inWork(side + GAP / 2, controls.bottom - 4), "the work whole again as the controls go");
});
