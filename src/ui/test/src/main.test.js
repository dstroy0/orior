// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window as it opens: on the tree and the tabs left, nothing in the way of them, the tree read
// behind it, and no address reached. The page each file of tests runs on is loaded fresh, and this
// file reads it as it stands once it has started.

import { invoke } from "../../src/bridge.js";
import { write } from "../../src/status.js";

const { test, assert, ui } = window.__harness;

test("the window opens with nothing in the way: no sheet, dialog, menu or page of its own", () => {
  assert.ok(document.documentElement.dataset.started !== undefined, "the window has started");
  assert.equal(document.querySelectorAll("dialog[open]").length, 0, "no sheet or dialog is open");
  assert.equal(document.querySelectorAll("body > .menu").length, 0, "no menu is open");
  assert.equal(document.querySelector(".sheet-changes"), null, "What's New waits for Help");
  const titles = [...document.querySelectorAll("#tabs .tab")].map((tab) => tab.textContent.trim());
  assert.ok(!titles.some((title) => /welcome|release notes|walkthrough|what's new/i.test(title)), `no tab of the window's own: ${titles.join(", ")}`);
});

test("the tree's files and what they declare are read behind the window, and the status bar says what is not read yet until it is", async () => {
  const unreadNode = () => document.querySelector("#pulse .pulse-unread");
  await ui.waitFor(() => !unreadNode(), 10000);
  assert.deepEqual(await invoke("tree_unread"), { files: false, symbols: false }, "both are read");
  write("unread", { files: true, symbols: true });
  await ui.waitFor(() => unreadNode()?.textContent === "The tree's files and symbols not read yet", 2000);
  write("unread", { files: false, symbols: true });
  await ui.waitFor(() => unreadNode()?.textContent === "The tree's symbols not read yet", 2000);
  write("unread", { files: false, symbols: false });
  await ui.waitFor(() => !unreadNode(), 2000);
});

test("the window reaches no address on the network as it opens, and files no error report the reader has not let file", async () => {
  const reached = (await invoke("network_reached")).filter((one) => one.at >= performance.timeOrigin);
  assert.deepEqual(reached, [], "no address reached since this window opened");
  if (!(await invoke("report_asked"))[0]) {
    assert.equal(await invoke("report_auto"), false, "no report files before the reader answers");
  }
});
