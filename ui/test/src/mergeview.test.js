// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Cherry-picking and the merge window's conflicts.

import { calls, made, press, putBackAsFound, told } from "../menus.js";

const { test, assert } = window.__harness;

test("Git, Cherry-Pick applies the commit named, and Resolve Conflicts says where none is in conflict", async () => {
  calls.answer("git_cherry_pick", () => "picked by the test");
  await press("cherry-pick", "abc1234");
  assert.ok(made("git_cherry_pick").some((one) => one.args.id === "abc1234"));
  await press("conflicts");
  assert.ok(told(/No file is in conflict/));
});

test("the window is put back as the file found it", putBackAsFound);
