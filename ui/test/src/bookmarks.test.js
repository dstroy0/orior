// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Bookmarks: set and taken off a line, and listed.

import { bookmarksOf } from "../../gui/src-js/bookmarks.js";
import { paletteOpen } from "../../gui/src-js/palette.js";
import { GIVEN, given, press, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("Go, Toggle Bookmark sets and takes off a bookmark on the cursor's line, and Bookmarks lists them", async () => {
  await given("ab\nc|d");
  await press("bookmark");
  assert.ok(bookmarksOf(GIVEN)?.has(1), "the second line bookmarked");
  await press("bookmarks");
  try {
    await ui.waitFor(() => paletteOpen() && /^Bookmarks/.test(document.querySelector(".quick-input").placeholder), 3000, "the bookmarks").catch((error) => {
      throw new Error(`${error.message}; the palette asks ${document.querySelector(".quick-input")?.placeholder}`);
    });
  } finally {
    await ui.key("Escape");
  }
  await press("bookmark");
  assert.equal(bookmarksOf(GIVEN), null, "taken off");
});

test("the window is put back as the file found it", putBackAsFound);
