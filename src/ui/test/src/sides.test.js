// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A side pane collapsing slides away whole, its ground and its inside together, and gives its room back
// once it is gone; shown again, it takes its room and slides in whole.

import { showView } from "../../src/views.js";
import { togglePane } from "../../src/sides.js";

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
