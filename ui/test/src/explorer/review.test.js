// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Review pane's comments and their menu.

import { editing } from "../../../gui/src-js/edit.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { addComment, comments } from "../../../gui/src-js/review.js";
import { asFound, calls, press, putBackAsFound, told } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Review's menu, from the command line: a comment opened, its changes shown, resolved, copied and deleted", async () => {
  asFound();
  addComment("hello.py", 0, "def greet(name):", "a note of the test's");
  const one = comments().find((comment) => comment.body === "a note of the test's");
  assert.ok(one, "the comment left");
  await press("comment-open", one.id);
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor.head().line === 0, 5000, "hello.py at the comment's line");
  await press("comment-changes", one.id);
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button") || told(/No change/), 5000, "the file's changes");
  closeDiff();
  await press("comment-resolve", one.id, "done");
  assert.ok(comments().find((comment) => comment.id === one.id).done, "resolved");
  await press("comment-resolve", one.id, "open");
  assert.ok(!comments().find((comment) => comment.id === one.id).done, "and open again");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("comment-copy", one.id);
  assert.deepEqual(copied, ["a note of the test's"]);
  await press("comment-delete", one.id);
  assert.ok(!comments().some((comment) => comment.id === one.id), "deleted");
  asFound();
});

test("the window is put back as the file found it", putBackAsFound);
