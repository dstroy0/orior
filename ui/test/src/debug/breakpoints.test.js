// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Breakpoints: set, printing, stepping's settings, and written out and taken in.

import { breakpointsOf } from "../../../gui/src-js/debug/breakpoints.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { calls, GIVEN, given, HOME, press, putBackAsFound, sheetShown } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Run, Toggle Breakpoint and Print Here set the cursor's line to stop or to print", async () => {
  await given("x = 1\ny = |x + 1\n");
  await press("breakpoint");
  assert.ok(breakpointsOf(GIVEN)?.has(1), "a breakpoint on the second line");
  await press("breakpoint");
  assert.equal(breakpointsOf(GIVEN), null);
  await press("print-here");
  assert.ok(breakpointsOf(GIVEN)?.get(1)?.log, "a breakpoint that prints");
  await press("breakpoint");
});

test("Run, Stepping asks how steps go, and Export and Import Breakpoints write and take in a file of them", async () => {
  await press("stepping");
  assert.equal((await sheetShown("h2")).textContent, "Stepping");
  const file = `${HOME()}/breakpoints.json`;
  calls.answer("pick", () => file);
  await given("a = 1|\n");
  await press("breakpoint");
  await press("export-breakpoints");
  await ui.waitFor(async () => (await invoke("file_read_any", { path: file }).catch(() => "")).includes(GIVEN), 5000, "the breakpoints written");
  await press("breakpoint");
  assert.equal(breakpointsOf(GIVEN), null);
  await press("import-breakpoints");
  await ui.waitFor(() => breakpointsOf(GIVEN)?.has(0), 5000, "the breakpoints taken in");
  await press("breakpoint");
});

test("the window is put back as the file found it", putBackAsFound);
