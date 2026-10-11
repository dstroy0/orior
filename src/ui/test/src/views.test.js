// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { onView, showView, shownView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const active = () => [...document.querySelectorAll(".mode")].filter((one) => one.dataset.active === "true").map((one) => one.id);

test("the window opens on the edit view", () => {
  assert.equal(shownView(), "edit");
  assert.deepEqual(active(), ["mode-edit"]);
});

test("showing a view marks it alone, names it on the body and tells each listener", () => {
  const told = [];
  onView((name) => told.push(name));
  try {
    showView("run");
    assert.equal(shownView(), "run");
    assert.equal(document.body.dataset.view, "run");
    assert.deepEqual(active(), ["mode-run"]);
    assert.deepEqual(told, ["run"]);
  } finally {
    showView("edit");
  }
  assert.deepEqual(told, ["run", "edit"]);
  assert.equal(document.body.dataset.view, "edit");
});

test("View, Run and View, Edit show their views from the keys", async () => {
  try {
    await ui.key("Ctrl+Shift+D");
    await ui.waitFor(() => shownView() === "run");
    assert.ok(ui.shown("#mode-run"), "the run view shows");
    await ui.key("Ctrl+Shift+E");
    await ui.waitFor(() => shownView() === "edit");
    assert.ok(ui.shown("#mode-edit"), "the edit view shows");
  } finally {
    showView("edit");
  }
});
