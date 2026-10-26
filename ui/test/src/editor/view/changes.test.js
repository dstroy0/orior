// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's changes to whole lines: undo and redo, comments.

import { editing } from "../../../../gui/src-js/edit.js";
import { given, press, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("Undo takes back the last change and Redo brings it again", async () => {
  await given("[ab]");
  await press("upper-case");
  assert.equal(editing().editor.doc.text(), "AB");
  await press("undo");
  assert.equal(editing().editor.doc.text(), "ab");
  await press("redo");
  assert.equal(editing().editor.doc.text(), "AB");
});

test("Comment puts the language's comment before each line, and a second press takes it off", async () => {
  await given("x = 1|\ny = 2");
  await press("comment");
  assert.match(editing().editor.doc.line(0), /^# ?x = 1$/);
  assert.equal(editing().editor.doc.line(1), "y = 2", "the line the cursor is not on stays");
  await press("comment");
  assert.equal(editing().editor.doc.text(), "x = 1\ny = 2");
});

test("the window is put back as the file found it", putBackAsFound);
