// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A protocol run's panel, shown from the window and the command line.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { showView } from "../../../gui/src-js/views.js";
import { loadRun } from "../../../gui/src-js/run.js";
import { press, PROTOCOL, PROTOCOL_READINGS, putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

test("the protocol's commands show the Run view, from the window and from the command line", async () => {
  await loadRun();
  for (const command of PROTOCOL) {
    showView("edit");
    await runCommand(command);
    assert.equal(document.body.dataset.view, "run", `${command} shows the Run view from the window`);
    showView("edit");
    if (PROTOCOL_READINGS.has(command)) {
      await assert.rejects(() => press(command), /exit code: 2/, `${command} with no record is refused at the command line`);
      continue;
    }
    await press(command);
    assert.equal(document.body.dataset.view, "run", `${command} shows the Run view`);
  }
});

test("the window is put back as the file found it", putBackAsFound);
