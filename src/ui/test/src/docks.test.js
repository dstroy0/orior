// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tool windows' docks: a window that shows comes in from the edge it docks at.

import { sideOf } from "../../src/docks.js";
import { runCommand } from "../../src/menubar.js";

const { test, assert, ui } = window.__harness;

test("a tool window docked at the foot rises from it as it shows", async () => {
  const panel = document.getElementById("comms");
  assert.equal(sideOf("comms"), "bottom");
  assert.equal(panel.hidden, true, "it starts closed");
  runCommand("comms-view");
  try {
    await ui.waitFor(() => !panel.hidden);
    const arriving = panel.getAnimations()[0];
    assert.ok(arriving, "it moves as it shows");
    const [first, last] = arriving.effect.getKeyframes();
    assert.match(first.transform, /translateY\(14px\)/, "from below, the edge it docks at");
    assert.equal(last.transform, "none");
  } finally {
    runCommand("comms-view");
    await ui.waitFor(() => panel.hidden);
  }
});
