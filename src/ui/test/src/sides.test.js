// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A side pane collapsing slides away whole, its ground and its inside together, and gives its room back
// once it is gone; shown again, it takes its room and slides in whole.

import { showView } from "../../src/views.js";
import { EDGE_REST, togglePane } from "../../src/sides.js";

const { test, assert, ui } = window.__harness;

const frame = () => new Promise((done) => requestAnimationFrame(() => done()));

// The transitions of transform running on a node.
const sliding = (node) => node.getAnimations().filter((one) => one.transitionProperty === "transform");

test("the explorer collapsing slides away whole and gives its room back after, and slides in whole as it shows", async () => {
  showView("edit");
  togglePane(true, { take: false, pin: true });
  const pane = document.getElementById("explorer");
  const inside = pane.querySelector(":scope > .side-inner");
  await ui.waitFor(() => pane.getBoundingClientRect().width > 100 && !pane.getAnimations().length, 3000);
  const width = pane.getBoundingClientRect().width;
  try {
    togglePane(false, { take: false, pin: true });
    await frame();
    assert.equal(sliding(pane).length, 1, "the pane itself slides");
    assert.equal(sliding(inside).length, 0, "its inside stays in place in it");
    assert.ok(Math.abs(pane.getBoundingClientRect().width - width) < 1, "holding its room as it goes");
    await ui.waitFor(() => pane.getBoundingClientRect().width <= 2, 2000);
    togglePane(true, { take: false, pin: true });
    await ui.waitFor(() => sliding(pane).length === 1, 1000);
    assert.equal(sliding(inside).length, 0, "and comes back whole");
    assert.ok(pane.getBoundingClientRect().width > 100, "in the room it took at once");
  } finally {
    togglePane(true, { take: false, pin: true });
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
    await ui.hover(box.left + 2, box.top + box.height / 2);
    await ui.hover(box.right + 120, box.top + box.height / 2);
    await new Promise((done) => setTimeout(done, EDGE_REST + 150));
    assert.ok(pane.classList.contains("collapsed"), "a pointer passing over the edge shows nothing");
    await ui.hover(box.left + 2, box.top + box.height / 2);
    await ui.waitFor(() => !pane.classList.contains("collapsed"), EDGE_REST + 1500);
    assert.ok(!pane.classList.contains("collapsed"), "one resting on it brings the pane back");
  } finally {
    togglePane(true, { take: false, pin: true });
  }
});
