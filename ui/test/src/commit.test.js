// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Commit window: a commit, the review of its structure, push and pull.

import { changeShown, closeChange } from "../../gui/src-js/changeview.js";
import { paneOpen } from "../../gui/src-js/explorer.js";
import { calls, made, press, putBackAsFound, told } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("Git, Commit, Review Structure, Push and Pull", async () => {
  await press("commit-view");
  assert.ok(paneOpen("changes"));
  await press("review-structure");
  await ui.waitFor(() => made("review_structure").length, 5000, "the change's structure read");
  await ui.waitFor(() => changeShown(), 5000, "the change's structure shown");
  closeChange();
  calls.answer("git_push", () => "pushed by the test");
  calls.answer("git_pull", () => "pulled by the test");
  await press("push");
  await ui.waitFor(() => told(/pushed by the test/), 5000, "the push said");
  await press("pull");
  await ui.waitFor(() => told(/pulled by the test/), 5000, "the pull said");
});

test("the window is put back as the file found it", putBackAsFound);
