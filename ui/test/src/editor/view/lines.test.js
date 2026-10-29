// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's selections grown and shrunk, and its lines rewritten.

import { given, marked, press, putBackAsFound } from "../../../menus.js";

const { test, assert, ui } = window.__harness;

test("Expand Selection takes in the word at the cursor and Shrink Selection gives it back", async () => {
  await given("f(a|b)");
  await press("expand-selection");
  await ui.waitFor(() => marked() === "f([ab])", 3000, "the word selected");
  await press("shrink-selection");
  assert.equal(marked(), "f(a|b)");
});

test("the bracket before the cursor pairs with the one it closes, lines apart, and a bracket in a string is passed over", async () => {
  const view = await given('f(a, ")",\n  {b: 1})|');
  await ui.waitFor(() => view.bracketPair() !== null, 5000, "the pair, as orior's side reads it off the branch");
  assert.deepEqual(view.bracketPair(), [{ line: 1, col: 8 }, { line: 0, col: 1 }]);
});

test("a bracket that starts at the cursor pairs where none ends at it", async () => {
  const view = await given("f|(a, b)");
  await ui.waitFor(() => view.bracketPair() !== null, 5000, "the pair, as orior's side reads it off the branch");
  assert.deepEqual(view.bracketPair(), [{ line: 0, col: 1 }, { line: 0, col: 6 }]);
});

test("the window is put back as the file found it", putBackAsFound);
