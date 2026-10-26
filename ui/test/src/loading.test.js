// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { FADE, hideLoading, killLoading, SHORTEST, showLoading } from "../../gui/src-js/loading.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { made, TREE } from "../menus.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("loading");

test("the loading pane is gone once the window has started", () => {
  assert.equal(pane().hidden, true);
});

test("a folder opened once the window has started shows no loading pane: the name shows once, on the first start in the system's session", async () => {
  const before = made("root_set").length;
  let shown = false;
  const watching = new MutationObserver(() => (shown = shown || !pane().hidden));
  watching.observe(pane(), { attributes: true, attributeFilter: ["hidden"] });
  try {
    await runCommand("open-folder", [TREE()]);
    await ui.waitFor(() => made("root_set").length > before, 5000, "the folder opened");
    await ui.rest(500);
  } finally {
    watching.disconnect();
  }
  assert.ok(!shown && pane().hidden, "the loading pane never shown");
});

test("the pane is glass over the window's lattice, the modes under it not drawn, the eye held back, and stays at least its shortest time", async () => {
  showLoading();
  assert.equal(pane().hidden, false);
  assert.equal(pane().dataset.leaving, "false");
  assert.ok(!document.querySelector("#loading canvas.eye"), "the eye held back");
  assert.ok(!pane().querySelector("canvas"), "no lattice of its own");
  assert.equal(getComputedStyle(pane()).backgroundColor, "rgba(0, 0, 0, 0)", "its ground clear");
  assert.ok(ui.shown("canvas.lattice-window"), "the window's lattice shows through it");
  assert.equal(getComputedStyle(document.getElementById("mode-edit")).visibility, "hidden", "the mode under it not drawn");
  const started = performance.now();
  await hideLoading();
  const took = performance.now() - started;
  assert.ok(took >= SHORTEST + FADE - 20, `the pane stayed ${Math.round(took)} ms`);
  assert.equal(pane().hidden, true);
  assert.equal(pane().dataset.leaving, "true");
});

test("a pane shown longer than its shortest time leaves after its fade alone", async () => {
  showLoading();
  await ui.rest(SHORTEST + 100);
  const started = performance.now();
  await hideLoading();
  const took = performance.now() - started;
  assert.ok(took >= FADE - 20 && took < SHORTEST, `it left in ${Math.round(took)} ms`);
  assert.equal(pane().hidden, true);
});

test("a pane killed in its wait is gone at once, and the hide it was in ends", async () => {
  showLoading();
  const started = performance.now();
  const hiding = hideLoading();
  await ui.rest(100);
  killLoading();
  await hiding;
  const took = performance.now() - started;
  assert.ok(took < SHORTEST, `it ended in ${Math.round(took)} ms`);
  assert.equal(pane().hidden, true);
  assert.equal(pane().dataset.leaving, "true");
});

test("showing it again while it shows keeps the one pane, which leaves after one fade", async () => {
  showLoading();
  showLoading();
  await hideLoading();
  assert.equal(pane().hidden, true);
});

test("a pane shown again while a hide waits stays shown: the hide it was in ends and leaves it", async () => {
  showLoading();
  const started = performance.now();
  const hiding = hideLoading();
  await ui.rest(100);
  showLoading();
  await hiding;
  const took = performance.now() - started;
  try {
    assert.ok(took < SHORTEST, `the first hide ended in ${Math.round(took)} ms`);
    assert.equal(pane().hidden, false, "the pane still shown");
    assert.equal(pane().dataset.leaving, "false", "the pane not leaving");
  } finally {
    killLoading();
  }
});
