// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Run menu, the debugger's commands among it, each command run as its menu runs it and as a command line runs
// it, and what it does looked for.

import { canStepBack, debugging, debugStarting, isPaused } from "../../../gui/src-js/debug/session.js";
import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke, listen } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { paletteOpen } from "../../../gui/src-js/palette.js";
import { noticesWaiting, noticesSaid } from "../../../gui/src-js/statusbar.js";
import { pausedLineOf } from "../../../gui/src-js/debug.js";
import { chosenJob, chosenLive, loadRun, showJob } from "../../../gui/src-js/run.js";
import { testsOf } from "../../../gui/src-js/tests.js";
import { calls, DEBUGGED, endSessions, made, press, putBackAsFound, QUICK, ran, SLOW, told, TREE } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Run, Start and Stop run the job chosen, and the command line lists, shows and starts the tree's jobs", async () => {
  await invoke("file_write", { path: "src/build_quick.sh", text: "#!/usr/bin/env bash\necho quick\n" });
  await invoke("file_write", { path: "src/build_slow.sh", text: "#!/usr/bin/env bash\nsleep 30\n" });
  await loadRun();
  assert.equal(await ran("list"), "ran");
  assert.equal(await ran("show", QUICK), "ran");
  assert.equal(await ran("start", QUICK), "ran", "the job run in the terminal, and ended well");
  await runCommand("list", ["slow"]);
  assert.equal(document.body.dataset.view, "run");
  assert.equal(document.getElementById("job-filter").value, "slow");
  showJob(SLOW);
  assert.equal(chosenJob()?.id, SLOW);
  const ends = [];
  await listen("run-end", ({ payload }) => ends.push(payload));
  await runCommand("start");
  await ui.waitFor(() => chosenLive(), 10000, "the job running");
  await press("stop");
  await ui.waitFor(() => !chosenLive(), 10000, "the job stopped").catch((error) => {
    throw new Error(`${error.message}; asked to stop: ${JSON.stringify(made("job_stop").map((one) => one.args))}; started: ${JSON.stringify(made("job_start").map((one) => one.args.job))}; ended: ${JSON.stringify(ends)}; the console: ${document.getElementById("lines")?.textContent.slice(0, 400)}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesWaiting()].slice(-3).map((one) => one.text))}`);
  });
}, { timeout: 60000 });

test("Run, Update Snapshots runs its line in the terminal", async () => {
  calls.answer("snapshots_line", () => ["", "echo snapshots-updated"]);
  await press("update-snapshots");
  await ui.waitFor(() => /snapshots-updated/.test(document.getElementById("term-views").textContent), 10000, "the line run");
});

test("Run, Send Request, Copy as curl and Private Environment File on a file of requests", async () => {
  await invoke("file_write", { path: "asked.http", text: "GET http://127.0.0.1:9/asked\n" });
  showView("edit");
  await openFile("asked.http");
  editing().editor.goTo(0, 0);
  calls.answer("requests_send", () => ({ status: 200, reason: "OK", headers: [], body: "", ms: 1, size: 0 }));
  await press("send-request");
  // the send reads the file's environments first, so it is made after the press has returned
  await ui.waitFor(() => made("requests_send").some((one) => one.args.path === "asked.http"), 5000, "the request sent");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("copy-as-curl");
  await ui.waitFor(() => copied.length, 5000, "the curl line copied");
  assert.match(copied[0], /^curl\b[\s\S]*127\.0\.0\.1:9\/asked/);
  await openFile("asked.http");
  await press("private-environment");
  await ui.waitFor(() => made("requests_private").length, 5000, "the private environment asked for");
});

test("Run, Find Tests lists the tree's tests, Run Tests runs them, Run Failed Tests the failed, and Stop Tests stops them", async () => {
  await invoke("file_write", { path: "test_menus.py", text: "def test_passes():\n    assert True\n\n\ndef test_fails():\n    assert False\n" });
  // The tree takes in a file as its watch tells of it.
  await ui.waitFor(async () => (await invoke("tests_found")).some((one) => one.file === "test_menus.py"), 10000, "the file's tests found");
  assert.equal(await ran("tests"), "ran");
  await runCommand("tests");
  try {
    await ui.waitFor(() => paletteOpen() && /tests?: type to filter/.test(document.querySelector(".quick-input").placeholder), 5000, "the tests listed");
  } finally {
    await ui.key("Escape");
  }
  await press("run-tests");
  await ui.waitFor(() => testsOf("test_menus.py")?.get(0) === "passed" && testsOf("test_menus.py")?.get(4) === "failed", 60000, "each test's result").catch((error) => {
    throw new Error(`${error.message}; the results: ${JSON.stringify([...(testsOf("test_menus.py") ?? [])])}; run: ${JSON.stringify(made("tests_run").map((one) => one.args))}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesWaiting()].slice(-3).map((one) => one.text))}`);
  });
  const asked = made("tests_run").length;
  await press("run-failed-tests");
  await ui.waitFor(() => made("tests_run").length > asked, 5000, "the failed test run again");
  assert.deepEqual(made("tests_run").at(-1).args.given, ["test_menus.py::test_fails"]);
  await press("cover-tests");
  await ui.waitFor(() => made("tests_run").at(-1).args.cover === true, 5000, "the tests run for their coverage");
  await press("stop-tests");
  assert.ok(made("tests_stop").length);
}, { timeout: 120000 });

test("Run, Debug stops at a breakpoint, and Step Into, Step Out, Step Over and Continue go on from it", async () => {
  await invoke("file_write", { path: DEBUGGED, text: "def inner():\n    x = 1\n    return x\n\n\ny = inner()\nprint(y)\n" });
  showView("edit");
  await openFile(DEBUGGED);
  editing().editor.goTo(5, 0);
  await press("breakpoint");
  const at = () => pausedLineOf(DEBUGGED);
  await press("debug");
  await ui.waitFor(() => isPaused() && at() === 5, 30000, "stopped at the breakpoint");
  await press("step-into");
  await ui.waitFor(() => at() === 1, 10000, "inside inner");
  await press("step-out");
  await ui.waitFor(() => at() === 5 || at() === 6, 10000, "back in the caller");
  if (at() === 5) {
    await press("step-over");
    await ui.waitFor(() => at() === 6, 10000, "the next line");
  }
  await press("continue");
  await ui.waitFor(() => !debugging(), 15000, "the program run to its end").finally(endSessions);
}, { timeout: 90000 });

test("Run, Pause stops a running program, Restart starts it again, and Stop ends it", async () => {
  await invoke("file_write", { path: "looped.py", text: "import time\nwhile True:\n    time.sleep(0.05)\n" });
  showView("edit");
  await openFile("looped.py");
  try {
    await press("debug");
    // Pause takes the thread the program runs on, which the debugger tells once the program runs.
    await ui.waitFor(async () => {
      await press("pause");
      return isPaused();
    }, 30000, "the program paused");
    await press("restart-debug");
    await ui.waitFor(() => debugging() && !isPaused(), 30000, "the program running again");
    await press("stop-debug");
    await ui.waitFor(() => !debugging(), 10000, "the program ended");
  } finally {
    await endSessions();
  }
}, { timeout: 90000 });

test("Run, Record and Debug records the run, and Step Back and Reverse Continue go back through it", async () => {
  showView("edit");
  await openFile(DEBUGGED);
  await press("record");
  await ui.waitFor(() => canStepBack(), 60000, "the run recorded and stopped").catch(async (error) => {
    await endSessions();
    throw error;
  });
  const from = pausedLineOf(DEBUGGED);
  await press("step-back");
  await ui.waitFor(() => isPaused() && pausedLineOf(DEBUGGED) !== from, 10000, "a step back");
  await press("reverse-continue");
  await ui.waitFor(() => isPaused(), 10000, "stopped going back");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 10000, "the session ended").finally(endSessions);
}, { timeout: 120000 });

test("Run, Debug File with Its C starts the Python stopped before its first line", async () => {
  showView("edit");
  await openFile(DEBUGGED);
  await press("debug-mixed");
  assert.ok(made("debug_start").some((one) => one.args.start.path === DEBUGGED && one.args.options.entry === true), "started at its entry");
  await ui.waitFor(() => debugging(), 30000, "the session");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 15000, "the sessions ended").finally(endSessions);
}, { timeout: 90000 });

test("Run, Attach to Process lists the processes, and Attach to Address debugs the program listening there", async () => {
  await press("attach-process");
  try {
    await ui.waitFor(() => paletteOpen() && /^Attach to a process/.test(document.querySelector(".quick-input").placeholder), 10000, "the processes listed");
  } finally {
    await ui.key("Escape");
  }
  await press("attach-address", "127.0.0.1:notaport");
  assert.ok(told(/is no address/), "an address with no port refused");
  // A program a detached session leaves running, which ends by itself once the test is done with it.
  await invoke("file_write", { path: "listened.py", text: "import time\nfor _ in range(200):\n    time.sleep(0.05)\n" });
  await ui.run("python", ["-m", "debugpy", "--listen", "127.0.0.1:5694", "--wait-for-client", `${TREE()}/listened.py`], { wait: false });
  await ui.waitFor(async () => {
    await press("attach-address", "127.0.0.1:5694");
    return debugging();
  }, 30000, "the program at the address debugged");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 10000, "the session ended").finally(endSessions);
  // Each ask of the address is answered, the one that started the session and each after it.
  await ui.waitFor(() => !debugStarting().length, 60000, "every start answered").catch((error) => {
    throw new Error(`${error.message}; still starting: ${debugStarting().join(", ")}; the debugger said: ${document.getElementById("debug-said").textContent}; ${document.getElementById("debug-out").textContent.slice(-400)}`);
  });
}, { timeout: 90000 });

test("the window is put back as the file found it", putBackAsFound);
