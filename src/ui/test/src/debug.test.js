// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger's sessions, one after another: a loop paused and started again, a run recorded, a
// Python file debugged with its C, and a program debugged at its address and left running, each
// ended, leave the next session free to start and stop at its breakpoint.

import { invoke } from "../../src/bridge.js";
import { attachAddress, breakpointsOf, canStepBack, debugging, debugMixed, debugFile, isPaused, pausedLineOf, recordFile, restartDebug, step, stopDebug, toggleBreakpoint } from "../../src/debug.js";
import { openFile } from "../../src/edit.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const said = () => `${document.getElementById("debug-said").textContent}; ${document.getElementById("debug-out").textContent.slice(-400)}`;

async function endAll() {
  for (let left = 5; debugging() && left > 0; left -= 1) {
    await stopDebug();
    await ui.rest(200);
  }
}

test("a Python file debugged with its C, ended, leaves the next session free to start and stop at its breakpoint", async () => {
  await invoke("file_write", { path: "after_mixed.py", text: "a = 1\nb = a + 1\nprint(b)\n" });
  showView("edit");
  await openFile("after_mixed.py");
  try {
    await invoke("file_write", { path: "looped_again.py", text: "import time\nwhile True:\n    time.sleep(0.05)\n" });
    await openFile("looped_again.py");
    await debugFile();
    await ui.waitFor(async () => {
      await step("pause");
      return isPaused();
    }, 30000, "the loop paused");
    await restartDebug();
    await ui.waitFor(() => debugging() && !isPaused(), 30000, "the loop running again");
    await endAll();
    await openFile("after_mixed.py");
    await recordFile();
    await ui.waitFor(() => canStepBack(), 60000, "the run recorded");
    await endAll();
    await debugMixed();
    await ui.waitFor(() => debugging(), 30000, "the first session");
    await ui.rest(2000);
    await endAll();
    await ui.waitFor(() => !debugging(), 10000, "every session ended");
    toggleBreakpoint("after_mixed.py", 1);
    await debugFile();
    await ui.waitFor(() => isPaused() && pausedLineOf("after_mixed.py") === 1, 30000, "the next session stopped at its breakpoint").catch((error) => {
      throw new Error(`${error.message}; the debugger said: ${said()}`);
    });
  } finally {
    await endAll();
    if (breakpointsOf("after_mixed.py")?.has(1)) {
      toggleBreakpoint("after_mixed.py", 1);
    }
  }
}, { timeout: 120000 });

test("a program debugged at its address and left running leaves the next session free to start and stop at its breakpoint", async () => {
  const tree = window.__harness.servers.ORIOR_TEST_TREE.replace(/\\/g, "/");
  await invoke("file_write", { path: "listening.py", text: "import time\nfor _ in range(600):\n    time.sleep(0.05)\n" });
  await invoke("file_write", { path: "after_attach.py", text: "a = 1\nb = a + 1\nprint(b)\n" });
  await ui.run("python", ["-m", "debugpy", "--listen", "127.0.0.1:5695", "--wait-for-client", `${tree}/listening.py`], { wait: false });
  try {
    await ui.waitFor(async () => {
      await attachAddress("127.0.0.1:5695");
      return debugging();
    }, 30000, "the program at the address debugged");
    await endAll();
    showView("edit");
    await openFile("after_attach.py");
    toggleBreakpoint("after_attach.py", 1);
    await debugFile();
    await ui.waitFor(() => isPaused() && pausedLineOf("after_attach.py") === 1, 30000, "the next session stopped at its breakpoint").catch((error) => {
      throw new Error(`${error.message}; the debugger said: ${said()}`);
    });
  } finally {
    await endAll();
    if (breakpointsOf("after_attach.py")?.has(1)) {
      toggleBreakpoint("after_attach.py", 1);
    }
  }
}, { timeout: 120000 });

