// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The status bar's items: hidden and displayed, moved along their line, and the bar reset.

import { itemShown } from "../../gui/src-js/statusitems.js";
import { asFound, press, putBackAsFound } from "../menus.js";

const { test, assert } = window.__harness;

test("the status bar's menu, from the command line: an item hidden and displayed, moved along its line, and the bar reset", async () => {
  asFound();
  // Memory's line as it is on the screen, each line ordering its items as its style says.
  const order = () => {
    const line = document.querySelector('#statusbar [data-item="memory"]').parentElement;
    return [...line.querySelectorAll(":scope > [data-item]")].filter((one) => one.getClientRects().length).sort((a, b) => a.getBoundingClientRect().left - b.getBoundingClientRect().left).map((one) => one.dataset.item);
  };
  await press("status-show", "zoom", "off");
  assert.ok(!itemShown("zoom"), "Zoom hidden");
  await press("status-show", "zoom", "on");
  assert.ok(itemShown("zoom"), "and displayed");
  const before = order();
  const left = before[before.indexOf("memory") - 1];
  await press("status-move", "memory", "left");
  if (left) {
    assert.ok(order().indexOf("memory") < order().indexOf(left), `Memory moved past ${left}: ${order()}`);
  }
  await press("status-move", "memory", "right");
  await press("status-show", "focus", "off");
  await press("status-reset");
  assert.ok(itemShown("focus"), "every item displayed again");
  assert.deepEqual(order(), before, "in the bar's own order");
});

test("the window is put back as the file found it", putBackAsFound);
