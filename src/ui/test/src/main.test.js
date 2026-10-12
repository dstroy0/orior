// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window as it opens: on the tree and the tabs left, and nothing in the way of them. The page
// each file of tests runs on is loaded fresh, and this file reads it as it stands once it has
// started.

const { test, assert } = window.__harness;

test("the window opens with nothing in the way: no sheet, dialog, menu or page of its own", () => {
  assert.ok(document.documentElement.dataset.started !== undefined, "the window has started");
  assert.equal(document.querySelectorAll("dialog[open]").length, 0, "no sheet or dialog is open");
  assert.equal(document.querySelectorAll("body > .menu").length, 0, "no menu is open");
  assert.equal(document.querySelector(".sheet-changes"), null, "What's New waits for Help");
  const titles = [...document.querySelectorAll("#tabs .tab")].map((tab) => tab.textContent.trim());
  assert.ok(!titles.some((title) => /welcome|release notes|walkthrough|what's new/i.test(title)), `no tab of the window's own: ${titles.join(", ")}`);
});
