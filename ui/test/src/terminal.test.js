// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal panel: shells opened, split, cleared and killed.

import { isChecked, runCommand } from "../../gui/src-js/menubar.js";
import { press, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("the Terminal menu opens, splits, clears and kills shells", async () => {
  const panel = document.getElementById("term");
  const tabs = () => panel.querySelectorAll(".term-tab").length;
  await press("new");
  const opened = tabs();
  assert.ok(!panel.hidden && opened >= 1, "a shell in a tab of its own");
  await press("split");
  assert.ok(isChecked("terminal-split"), "a shell beside");
  await press("split");
  assert.ok(!isChecked("terminal-split"), "one shell displayed again");
  const screen = () => document.querySelector("#term-views .term-view:not([hidden])")?.textContent ?? "";
  await press("clear");
  await ui.waitFor(() => !screen().trim() || screen().trim().split("\n").length <= 2, 5000, "the screen cleared");
  await press("kill");
  await ui.waitFor(() => tabs() === opened - 1 || panel.hidden, 5000, "the shell ended");
  const displayed = !panel.hidden;
  await press("toggle");
  assert.equal(!panel.hidden, !displayed);
  await press("toggle");
  assert.equal(!panel.hidden, displayed);
  await runCommand("toggle");
  if (!panel.hidden) {
    await runCommand("toggle");
  }
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
