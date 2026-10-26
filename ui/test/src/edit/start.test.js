// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor as the window starts it: made in the editor's place on the page, the line its cursor stands on, and an
// edit in a tab marking it changed and no longer a preview, saved again unmarked.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { tabOf } from "../../../gui/src-js/edit/tabs.js";
import { cursorLine } from "../../../gui/src-js/edit/start.js";
import { showView } from "../../../gui/src-js/views.js";
import { given, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the editor stands in its place on the page, and the line of its cursor is read from it", async () => {
  const view = await given("one\ntwo\nth|ree");
  assert.ok(document.getElementById("editor").contains(view.scroller));
  assert.equal(cursorLine(), 2);
  await given("|one");
  assert.equal(cursorLine(), 0);
});

test("an edit marks its tab changed and no longer a preview, and saved it is marked no more", async () => {
  const path = "compared_started.txt";
  await invoke("file_write", { path, text: "text\n" });
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path && editing().editor?.s, 5000);
  const tab = tabOf(path);
  tab.preview = true;
  assert.ok(!editing().activeChanged);
  editing().editor.type("x");
  assert.ok(editing().activeChanged);
  assert.ok(!tab.preview, "an edit keeps the tab");
  await editing().saveAll();
  await ui.waitFor(() => !editing().activeChanged, 3000, "saved, unmarked");
});

test("the window is put back as the file found it", putBackAsFound);
