// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The change peek: each change since the last commit stepped to and shown.

import { closePeek } from "../../../gui/src-js/edit/peek.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Go, Next Change and Previous Change step through the changes since the last commit, each peeked", async () => {
  showView("edit");
  await openFile("hello.py");
  const view = editing().editor;
  view.goTo(0, 0);
  view.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "# changed\n" }], null);
  try {
    await ui.waitFor(() => view.s.changes?.hunks?.length, 10000, "the change against the last commit");
    view.goTo(5, 0);
    await press("previous-change");
    assert.equal(view.head().line, 0, "the change before");
    await ui.waitFor(() => document.querySelector(".peek .peek-title")?.textContent.startsWith("Change 1 of"), 3000, "the change peeked");
    view.goTo(5, 0);
    await press("next-change");
    assert.equal(view.head().line, 0, "the first change, after the last");
  } finally {
    closePeek();
    document.querySelector(".peek")?.remove();
    view.change([{ from: { line: 0, col: 0 }, to: { line: 1, col: 0 }, text: "" }], null);
  }
});

test("the window is put back as the file found it", putBackAsFound);
