// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Problems pane: the tree's check, how far it has gone, and stopped.

import { drawProblems } from "../../../gui/src-js/explorer/problems.js";
import { showGroup } from "../../../gui/src-js/explorer.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui, calls } = window.__harness;

test("the tree's check says in Problems how far it has gone, and Stop lets the files waiting go", async () => {
  const calls = window.__harness.calls;
  calls.answer("problems_check_stop", () => null);
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showGroup("problems");
  try {
    drawProblems([], { done: 25, total: 100 });
    const line = await ui.waitFor(() => document.querySelector("#problems .problems-checking"), 2000, "the check's line");
    assert.match(line.textContent, /^Checking the tree: 25 of 100 files\. Stop$/);
    await ui.click(line.querySelector(".pulse-stop"));
    assert.ok(calls.made.some((one) => one.name === "problems_check_stop"), "Stop ends the check");
    drawProblems([], { done: 100, total: 100 });
    assert.equal(document.querySelector("#problems .problems-checking"), null, "and the line goes once it is done");
  } finally {
    showGroup("explorer");
  }
});
