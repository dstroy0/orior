// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's panes bound as the window starts: a link in a hover, a diagnostic or a document opened in the browser
// with the window retained, and the tree's changes read again as the window comes back to the front.

import { putBackAsFound } from "../../menus.js";

const { test, assert, ui, calls } = window.__harness;

test("a link of the page opens in the browser, and the window stays", async () => {
  const opened = [];
  calls.answer("link_open", ({ url }) => opened.push(url));
  const link = Object.assign(document.createElement("a"), { href: "https://example.com/page", textContent: "a link" });
  link.dataset.link = "";
  document.body.append(link);
  try {
    const event = new MouseEvent("click", { bubbles: true, cancelable: true });
    link.dispatchEvent(event);
    assert.ok(event.defaultPrevented, "the page does not follow it");
    await ui.waitFor(() => opened.length === 1, 3000);
    assert.equal(opened[0], "https://example.com/page");
  } finally {
    link.remove();
  }
});

test("the window coming back to the front reads the tree's repositories and changes again", async () => {
  const before = calls.made.filter((one) => one.name === "git_repositories").length;
  window.dispatchEvent(new Event("focus"));
  await ui.waitFor(() => calls.made.filter((one) => one.name === "git_repositories").length > before, 5000, "the repositories read again");
});

test("the window is put back as the file found it", putBackAsFound);
