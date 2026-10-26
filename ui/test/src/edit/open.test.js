// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Files opened, and the split beside the editor.

import { openFile } from "../../../gui/src-js/edit/open.js";
import { editing } from "../../../gui/src-js/edit.js";
import { GIVEN, given, press, putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

test("View, Split Right and Split Down split the editor, Unsplit closes it, and a file moves and opens there", async () => {
  await given("a|b");
  const desk = document.querySelector("#mode-edit .desk");
  const splitTabs = () => [...document.querySelectorAll(".split-tabs .tab")].map((tab) => tab.textContent);
  await press("split-right");
  assert.ok(editing().splitShown());
  assert.equal(desk.dataset.split, "right");
  await press("unsplit");
  assert.ok(!editing().splitShown());
  await press("split-down");
  assert.equal(desk.dataset.split, "down");
  await press("unsplit");
  await openFile("hello.py");
  await openFile(GIVEN);
  await press("move-to-split");
  assert.ok(editing().splitShown(), "a split made for the file");
  assert.ok(splitTabs().some((name) => name.includes(GIVEN)), `${GIVEN} in the split: ${splitTabs()}`);
  await press("unsplit");
  await press("open-in-split", "hello.py");
  assert.ok(splitTabs().some((name) => name.includes("hello.py")), `hello.py in the split: ${splitTabs()}`);
  await press("unsplit");
  assert.ok(!editing().splitShown());
});

test("the window is put back as the file found it", putBackAsFound);
