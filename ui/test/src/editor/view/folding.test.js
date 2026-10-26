// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's folds.

import { editing } from "../../../../gui/src-js/edit.js";
import { given, press, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("Fold All closes every region and Unfold All opens them again", async () => {
  await given("def f():\n    return 1\n\ndef g():\n    return 2\n|");
  await press("fold-all");
  assert.equal(editing().editor.s.folded.size, 2, "both functions closed");
  await press("unfold-all");
  assert.equal(editing().editor.s.folded.size, 0);
});

test("the window is put back as the file found it", putBackAsFound);
