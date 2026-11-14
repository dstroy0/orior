// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a command needs and whether a setting is on, and the macros' items.

import { isChecked } from "../../../gui/src-js/menubar/checks.js";
import { forgetMacro, retainedMacros } from "../../../gui/src-js/macros.js";
import { given, press, putBackAsFound, SETTINGS } from "../../menus.js";
import { allCommands } from "../../../gui/src-js/menubar.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("a retained macro's menu, from the command line: keys given and the macro forgotten", async () => {
  const view = await given("|");
  await press("macro-record");
  view.focus();
  await ui.type("q");
  await press("macro-record");
  await press("macro-retain", "keyed macro");
  try {
    await press("macro-keys", "keyed macro", "Ctrl+Alt+9");
    assert.equal(retainedMacros().find((one) => one.name === "keyed macro")?.keys, "Ctrl+Alt+9");
    await press("macro-forget", "keyed macro");
    assert.ok(!retainedMacros().some((one) => one.name === "keyed macro"), "forgotten");
  } finally {
    forgetMacro("keyed macro");
  }
});

test("each setting of the menus turns on and off as the command line's words say, and its menu shows it", async () => {
  showView("edit");
  await given("a|b");
  const checks = new Map(allCommands().filter((item) => item.checks).map((item) => [item.command, item.checks]));
  const wrong = [];
  for (const [command, [on, off]] of Object.entries(SETTINGS)) {
    const name = checks.get(command);
    assert.ok(name, `${command} has a check in its menu`);
    const was = isChecked(name);
    try {
      await press(command, on);
      if (!isChecked(name)) {
        wrong.push(`${command} ${on} left it off`);
      }
      await press(command, off);
      if (isChecked(name)) {
        wrong.push(`${command} ${off} left it on`);
      }
    } finally {
      await press(command, was ? on : off);
    }
  }
  assert.deepEqual(wrong, []);
}, { timeout: 120000 });

test("the window is put back as the file found it", putBackAsFound);
