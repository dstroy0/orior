// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A side pane collapsing slides away whole, its ground and its inside together, and gives its room back
// once it is gone; shown again, it takes its room and slides in whole.

import { showView } from "../../gui/src-js/views.js";
import { EDGE_REST, togglePane } from "../../gui/src-js/sides.js";
import { setReduceMotion } from "../../gui/src-js/motion.js";

const { test, assert, ui } = window.__harness;

const frame = () => new Promise((done) => requestAnimationFrame(() => done()));

// The transitions of transform running on a node.
const sliding = (node) => node.getAnimations().filter((one) => one.transitionProperty === "transform");

test("the explorer collapsing slides away whole and gives its room back after, and slides in whole as it shows", async () => {
  showView("edit");
  // the slide is drawn where motion is not reduced: the system's preference and the View menu's are both set aside
  // for the test, and put back after
  const reduced = localStorage.getItem("orior.reduce-motion") === "true";
  await ui.media("prefers-reduced-motion", "no-preference");
  setReduceMotion(false);
  togglePane(true, { take: false, pin: true });
  const pane = document.getElementById("explorer");
  const inside = pane.querySelector(":scope > .side-inner");
  // the pane shown has arrived once its arrival's frames are drawn and its slide in has ended
  await ui.waitFor(() => pane.getBoundingClientRect().width > 100 && !pane.classList.contains("arriving") && !pane.getAnimations().length, 3000);
  const width = pane.getBoundingClientRect().width;
  try {
    togglePane(false, { take: false, pin: true });
    await frame();
    const running = pane.getAnimations().map((one) => `${one.constructor.name} ${one.transitionProperty ?? one.animationName ?? ""} ${one.playState}`);
    assert.equal(sliding(pane).length, 1, `the pane itself slides: classes ${pane.className}; animations ${running.join(", ") || "none"}; reduced ${document.documentElement.hasAttribute("data-reduced-motion")}`);
    assert.equal(sliding(inside).length, 0, "its inside stays in place in it");
    assert.ok(Math.abs(pane.getBoundingClientRect().width - width) < 1, "holding its room as it goes");
    await ui.waitFor(() => pane.getBoundingClientRect().width <= 2, 2000);
    togglePane(true, { take: false, pin: true });
    await ui.waitFor(() => sliding(pane).length === 1, 1000);
    assert.equal(sliding(inside).length, 0, "and comes back whole");
    assert.ok(pane.getBoundingClientRect().width > 100, "in the room it took at once");
  } finally {
    togglePane(true, { take: false, pin: true });
    setReduceMotion(reduced);
    await ui.media("prefers-reduced-motion", "").catch(() => {});
  }
});

// The pane collapsed comes back once the pointer has rested on its edge for EDGE_REST; a pointer that
// passes over the edge brings nothing.
test("the pane shows once the pointer has rested on its edge, and not for a pointer passing over it", async () => {
  showView("edit");
  const pane = document.getElementById("explorer");
  togglePane(false, { take: false, pin: true });
  await ui.waitFor(() => pane.classList.contains("collapsed") && pane.getBoundingClientRect().width <= 2, 3000);
  const edge = document.querySelector("#mode-edit .pane-edge-left");
  assert.ok(edge && !edge.hidden, "the edge stands where the pane went");
  const box = edge.getBoundingClientRect();
  try {
    await ui.hover({ x: box.left + 2, y: box.top + box.height / 2 });
    await ui.hover({ x: box.right + 120, y: box.top + box.height / 2 });
    await new Promise((done) => setTimeout(done, EDGE_REST + 150));
    assert.ok(pane.classList.contains("collapsed"), "a pointer passing over the edge shows nothing");
    await ui.hover({ x: box.left + 2, y: box.top + box.height / 2 });
    await ui.waitFor(() => !pane.classList.contains("collapsed"), EDGE_REST + 1500);
    assert.ok(!pane.classList.contains("collapsed"), "one resting on it brings the pane back");
  } finally {
    togglePane(true, { take: false, pin: true });
  }
});
