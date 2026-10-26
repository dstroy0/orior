// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger's menus, the tool windows' and the strip's, a tab group's, the marked files', the memory reading's
// and a kept macro's, each item from the command line.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { iconRemoved, sideOf } from "../../../gui/src-js/docks.js";
import { noticesHeld, noticesSaid } from "../../../gui/src-js/statusbar.js";
import { paneOpen } from "../../../gui/src-js/explorer.js";
import { breakpointsOf, canStepBack, debugging, isPaused, pausedLineOf } from "../../../gui/src-js/debug.js";
import { asFound, closeSheets, endSessions, made, press, putBackAsFound, sheetShown, told, VARS } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the debugger's menus, from the command line: breakpoints edited, an exception, a frame's code, and a variable kept, compared, watched and read", async () => {
  asFound();
  await invoke("file_write", { path: VARS, text: 'data = {"a": 1}\ncount = 3\ncount = 4\nprint(count)\n' });
  showView("edit");
  await openFile(VARS);
  await press("breakpoint-edit", VARS, "3", "count > 2");
  assert.equal(breakpointsOf(VARS)?.get(2)?.condition, "count > 2", "a breakpoint on the third line, with its condition");
  await press("breakpoint-message", VARS, "4", "count is {count}");
  assert.equal(breakpointsOf(VARS)?.get(3)?.log, "count is {count}", "one on the fourth that prints");
  await press("breakpoint-remove", VARS, "4");
  assert.ok(!breakpointsOf(VARS)?.has(3), "taken off");
  await press("breakpoint-edit", VARS, "3");
  assert.match((await sheetShown("h2")).textContent, /^Breakpoint at vars\.py:3$/);
  try {
    await press("debug");
    await ui.waitFor(() => isPaused() && pausedLineOf(VARS) === 2, 30000, "stopped where count is 3").catch((error) => {
      throw new Error(`${error.message}; the breakpoints ${JSON.stringify([...(breakpointsOf(VARS) ?? [])])}; the sessions started: ${JSON.stringify(made("debug_start").map((one) => one.args.start))}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesHeld()].slice(-3).map((one) => one.text))}; the debugger said: ${document.getElementById("debug-said").textContent}; ${document.getElementById("debug-out").textContent.slice(-400)}`);
    });
    await press("debug-exception", "uncaught", "off");
    assert.deepEqual(made("debug_exceptions").at(-1)?.args.on, [], "no exception stops it");
    await press("debug-exception", "uncaught", "on");
    assert.deepEqual(made("debug_exceptions").at(-1)?.args.on, ["uncaught"], "and uncaught ones again");
    await press("frame-code", "0");
    assert.match((await sheetShown("h2")).textContent, /^Bytecode of /);
    await press("variable-keep", "count");
    await ui.waitFor(() => told(/count is kept as it stands/), 5000, "count kept");
    await press("variable-compare", "count");
    assert.ok((await sheetShown(".debug-compare")).textContent.includes("3"), "the kept value beside the one now");
    await press("variable-memory", "count");
    await ui.waitFor(() => told(/gives count no place in memory|Memory at count/) || document.querySelector("dialog[open] pre.debug-memory"), 5000, "count's memory, or the debugger's word that it gives none");
    closeSheets();
    const watched = made("debug_watch_data").length;
    await press("variable-watch", "count");
    await ui.waitFor(() => made("debug_watch_data").length > watched, 5000, "the watch asked of the debugger");
    await press("stop-debug");
    await ui.waitFor(() => !debugging(), 10000, "the session ended");
    await press("record");
    await ui.waitFor(() => canStepBack(), 60000, "the run recorded");
    await press("variable-history", "count");
    assert.match((await sheetShown("h2")).textContent, /^count: \d+ values? through the run$/);
  } finally {
    await endSessions();
    editing().editor.goTo(2, 0);
    if (breakpointsOf(VARS)) {
      await runCommand("breakpoint");
    }
    asFound();
  }
}, { timeout: 150000 });

test("the tool windows' menus and the strip's, from the command line: a tool window docked, an icon taken off and put back, a pane opened", async () => {
  asFound();
  const side = sideOf("terminal");
  try {
    await press("tool-dock", "terminal", "right");
    assert.equal(sideOf("terminal"), "right");
  } finally {
    await press("tool-dock", "terminal", side);
  }
  await press("strip-icon", "profile", "off");
  assert.ok(iconRemoved("profile"), "the Profile icon off the strip");
  await press("strip-icon", "profile", "on");
  assert.ok(!iconRemoved("profile"), "and on it again");
  await press("pane-open", "timeline");
  assert.equal(document.body.dataset.view, "edit");
  assert.ok(paneOpen("timeline"), "the Timeline shown");
  asFound();
});

test("the window is put back as the file found it", putBackAsFound);
