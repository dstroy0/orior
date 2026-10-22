// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The claim: the work takes the window's edges while the reader edits, each edge gives its part back
// on a rest in the gap there, the controls' corner drops the three controls alone with a ghost as
// the pointer nears, and the eye stands in their tab while the bar is away.

import { editing, openFile } from "../../gui/src-js/edit.js";
import { claimed, revealed, zoneAt } from "../../gui/src-js/claim.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const GAP = 12;
const SLIDE = 500;

async function editingHello() {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor?.s, 10000);
  await ui.click(document.querySelector("#mode-edit .editor"));
  await ui.waitFor(() => claimed(), 3000, "the claim made as the editor takes the keys");
}

// The pointer rested at a point of the window, and the reveal it brings.
async function restedAt(x, y, zone) {
  await ui.hover({ x, y });
  await ui.waitFor(() => revealed() === zone, 3000, `a rest at ${x},${y} reveals ${zone}`);
  await ui.rest(SLIDE);
}

const middle = () => ({ x: Math.round(window.innerWidth / 2), y: Math.round(window.innerHeight / 2) });

test("the work takes the bar, the toolbar, the strip and the status bar while the editor holds the keys, a gap kept on every side", async () => {
  await editingHello();
  const work = document.querySelector(".work").getBoundingClientRect();
  assert.ok(work.top >= GAP - 1 && work.top <= GAP + 1, `the work's top at ${work.top}`);
  assert.ok(work.left >= GAP - 1 && work.left <= GAP + 1, `the work's left at ${work.left}`);
  assert.ok(Math.round(window.innerHeight - work.bottom) === GAP, `the work's foot ${window.innerHeight - work.bottom} from the window's`);
  assert.ok(Math.round(window.innerWidth - work.right) === GAP, `the work's right ${window.innerWidth - work.right} from the window's`);
  const bar = document.querySelector("header.bar").getBoundingClientRect();
  assert.ok(bar.bottom <= 0, `the bar above the window, its foot at ${bar.bottom}`);
  assert.ok(document.getElementById("window-controls").contains(document.getElementById("bar-mark")), "the eye in the controls' tab");
});

test("a rest in the gap at the foot brings the status bar back, and it goes once the pointer leaves", async () => {
  await editingHello();
  await restedAt(middle().x, window.innerHeight - 4, "bottom");
  const status = document.getElementById("statusbar").getBoundingClientRect();
  assert.ok(status.bottom <= window.innerHeight && status.top < window.innerHeight, `the status bar in view, its top at ${status.top}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest in the gap at the head brings the bar back, with the eye at its head again", async () => {
  await editingHello();
  await restedAt(middle().x, 4, "top");
  const bar = document.querySelector("header.bar").getBoundingClientRect();
  assert.ok(bar.top >= 0 && bar.bottom > 0, `the bar in view, its top at ${bar.top}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest in the controls' corner drops the three controls alone, a ghost of them as the pointer nears", async () => {
  await editingHello();
  const controls = document.getElementById("window-controls");
  await ui.hover({ x: window.innerWidth - 40, y: GAP * 2 + 4 });
  await ui.waitFor(() => document.body.hasAttribute("data-near"), 3000, "the ghost as the pointer nears");
  await restedAt(window.innerWidth - 40, 4, "controls");
  const box = controls.getBoundingClientRect();
  assert.ok(box.top >= 0 && box.bottom > 0 && box.right <= window.innerWidth, `the controls' tab in view at ${box.top},${box.right}`);
  assert.equal(zoneAt(window.innerWidth - 40, 4), "controls");
  assert.ok(document.querySelector("header.bar").getBoundingClientRect().bottom <= 0, "the bar itself stays away");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("the claim goes as the keys leave the editor for the explorer", async () => {
  await editingHello();
  await ui.click(document.getElementById("file-filter"));
  await ui.waitFor(() => !claimed(), 3000, "the claim let go");
  const bar = document.querySelector("header.bar").getBoundingClientRect();
  await ui.waitFor(() => document.querySelector("header.bar").getBoundingClientRect().top >= 0, 3000, `the bar back, its top at ${bar.top}`);
  assert.ok(document.querySelector("header.bar").contains(document.getElementById("bar-mark")), "the eye at the bar's head");
});
