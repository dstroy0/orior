// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { hideLoading, showLoading } from "../../src/loading.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("loading");

test("the loading pane is gone once the window has started", () => {
  assert.equal(pane().hidden, true);
});

test("the pane shows the eye over the lattice, and stays at least its shortest time", async () => {
  showLoading();
  assert.equal(pane().hidden, false);
  assert.equal(pane().dataset.leaving, "false");
  assert.ok(ui.shown("#loading canvas.eye"), "the eye shows");
  assert.ok(ui.shown("#loading canvas.lattice"), "the lattice shows");
  const started = performance.now();
  await hideLoading();
  const took = performance.now() - started;
  assert.ok(took >= 900 + 350 - 20, `the pane stayed ${Math.round(took)} ms`);
  assert.equal(pane().hidden, true);
  assert.equal(pane().dataset.leaving, "true");
});

test("a pane shown longer than its shortest time leaves after its fade alone", async () => {
  showLoading();
  await ui.rest(1000);
  const started = performance.now();
  await hideLoading();
  const took = performance.now() - started;
  assert.ok(took >= 330 && took < 900, `it left in ${Math.round(took)} ms`);
  assert.equal(pane().hidden, true);
});

test("showing it again while it shows starts one eye in place of the other", async () => {
  showLoading();
  showLoading();
  await hideLoading();
  assert.equal(pane().hidden, true);
});
