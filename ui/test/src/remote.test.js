// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A tree on another machine: joined again, and its address.

import { calls, made, press, putBackAsFound, told } from "../menus.js";

const { test, assert } = window.__harness;

test("the machine button's menu, from the command line: joining again, and its address, which a tree on this machine has none of", async () => {
  calls.answer("remote_rejoin", () => null);
  await press("remote-rejoin");
  assert.ok(made("remote_rejoin").length, "the link asked to join again");
  await press("remote-copy-address");
  assert.ok(told(/on this machine/), "no address for a tree on this machine");
});

test("the window is put back as the file found it", putBackAsFound);
