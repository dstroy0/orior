// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys in sight: Ctrl or Alt held alone shows each button's keys over it.

import { HOLD, hintsShown, keysOf } from "../../src/keyhints.js";

const { test, assert, ui } = window.__harness;

test("a button's keys are read from its keys or from the end of its title", () => {
  const named = document.createElement("button");
  named.setAttribute("aria-keyshortcuts", "Ctrl+K");
  assert.equal(keysOf(named), "Ctrl+K");
  const titled = document.createElement("button");
  titled.title = "Explorer (Ctrl+Shift+E)";
  assert.equal(keysOf(titled), "Ctrl+Shift+E");
  const counted = document.createElement("button");
  counted.title = "Notifications (3)";
  assert.equal(keysOf(counted), null);
});

test("holding Ctrl shows each button's keys over it, and letting go takes them away", async () => {
  await ui.hold("Ctrl");
  try {
    await ui.waitFor(() => hintsShown().length > 0, HOLD + 1500);
    const explorer = document.querySelector(".strip-button.strip-explorer").getBoundingClientRect();
    const hint = hintsShown().find((one) => one.keys === "Ctrl+Shift+E");
    assert.ok(hint, JSON.stringify(hintsShown()));
    assert.ok(Math.abs(hint.x - (explorer.left + explorer.width / 2)) < 1 && Math.abs(hint.y - (explorer.top + explorer.height / 2)) < 1, "over its button");
    assert.ok(hintsShown().some((one) => one.keys === "Alt+F"), `the File menu's letter: ${JSON.stringify(hintsShown().map((one) => one.keys))}`);
  } finally {
    await ui.release("Ctrl");
  }
  await ui.waitFor(() => hintsShown().length === 0, 500);
});

test("Ctrl let go sooner than the hold shows no keys", async () => {
  await ui.hold("Ctrl");
  await ui.rest(HOLD / 4);
  await ui.release("Ctrl");
  await ui.rest(HOLD + 200);
  assert.equal(hintsShown().length, 0);
});

test("holding Alt shows the keys too, and a wheel takes them away", async () => {
  await ui.hold("Alt");
  try {
    await ui.waitFor(() => hintsShown().length > 0, HOLD + 1500);
    await ui.wheel("#statusbar", 10);
    await ui.waitFor(() => hintsShown().length === 0, 500);
  } finally {
    await ui.release("Alt");
    // Alt let go alone puts the keys in the menu bar; Escape gives them back.
    await ui.key("Escape");
  }
});
