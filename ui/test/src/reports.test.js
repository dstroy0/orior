// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Reports of what went wrong: the form, and errors filed on their own.

import { isChecked, runCommand } from "../../gui/src-js/menubar.js";
import { calls, press, putBackAsFound, sheetShown } from "../menus.js";
import { invoke } from "../../gui/src-js/bridge.js";

const { test, assert } = window.__harness;

test("Help, Report a Problem shows its form", async () => {
  await runCommand("report");
  await sheetShown(".sheet-report");
});

test("Help, Auto Report keeps whether errors file on their own, from the command line and from the menu", async () => {
  const settings = `${window.__harness.servers.ORIOR_TEST_HOME}/settings.json`;
  const kept = async () => JSON.parse(await invoke("file_read_any", { path: settings }).catch(() => "{}")).auto_report;
  await press("auto-report", "on");
  assert.equal(await kept(), true, "the command line keeps it on");
  await press("auto-report", "off");
  assert.equal(await kept(), false, "and off");
  // The menu's press keeps it through a call the harness holds; the test answers it.
  const asked = [];
  calls.answer("report_auto_set", ({ on }) => asked.push(on));
  await runCommand("auto-report", ["on"]);
  assert.ok(isChecked("auto-report"), "the menu shows it on");
  await runCommand("auto-report", ["off"]);
  assert.ok(!isChecked("auto-report"));
  assert.deepEqual(asked, [true, false]);
});

test("the window is put back as the file found it", putBackAsFound);
