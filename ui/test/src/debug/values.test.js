// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Values read from a stopped program: bytecode, memory, watched memory and instructions.

import { debugging, isPaused } from "../../../gui/src-js/debug/session.js";
import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { closeMenu } from "../../../gui/src-js/menu.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { noticesHeld, noticesSaid } from "../../../gui/src-js/statusbar.js";
import { breakpointsOf, pausedLineOf } from "../../../gui/src-js/debug.js";
import { closeSheets, endSessions, given, made, press, putBackAsFound, sheetShown, told } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Run, Show Bytecode reads a Python line's bytecode, and Memory, Watch Memory and Instructions read a stopped C program", async () => {
  await endSessions();
  await given("a = 1 + 2|\n");
  await press("machine-code");
  assert.match((await sheetShown("h2")).textContent, /^Bytecode of /);
  await invoke("file_write", { path: "mem.c", text: "#include <stdio.h>\nint main(void)\n{\n    volatile int count = 0x11223344;\n    int other = 5;\n    other += 1;\n    count = 7;\n    printf(\"%d %d\\n\", count, other);\n    return 0;\n}\n" });
  showView("edit");
  await openFile("mem.c");
  editing().editor.goTo(4, 0);
  await press("breakpoint");
  try {
    await press("debug");
    await ui.waitFor(() => isPaused() && pausedLineOf("mem.c") === 4, 60000, "the C program stopped").catch((error) => {
      throw new Error(`${error.message}; the debugger said: ${document.getElementById("debug-said").textContent}; ${document.getElementById("debug-out").textContent.slice(-600)}`);
    });
    const [session] = (await invoke("debug_sessions")).slice(-1);
    let frames = [];
    for (const thread of await invoke("debug_threads", { session: session.id })) {
      frames = await invoke("debug_stack", { session: session.id, thread: thread.id }).catch(() => []);
      if (frames[0]?.path?.endsWith("mem.c")) {
        break;
      }
    }
    const scopes = await invoke("debug_scopes", { session: session.id, frame: frames[0].id });
    const count = (await invoke("debug_variables", { session: session.id, reference: scopes[0].reference })).find((one) => one.name === "count");
    // Where the debugger gives the variable no memory reference, its address read from `&count` is one.
    let reference = count.memory;
    if (!reference) {
      const found = await invoke("debug_evaluate", { session: session.id, expression: "&count", frame: frames[0].id, context: "watch" });
      reference = found.memory ?? found.value.match(/0x[0-9a-f]+/i)?.[0];
    }
    await press("show-memory", reference);
    const shown = await ui.waitFor(() => [...document.querySelectorAll("dialog[open] pre.debug-memory")].find((pre) => /44 33 22 11/.test(pre.textContent)), 10000, "count's bytes").catch((error) => {
      throw new Error(`${error.message}; the reference ${reference}; the sheet shows ${[...document.querySelectorAll("dialog[open] pre")].map((pre) => pre.textContent.slice(0, 200)).join(" | ")}`);
    });
    assert.ok(shown);
    closeSheets();
    // A debugger stops where memory changes as the machine's own watchpoints let it: the window says
    // the watch set, or the debugger's refusal.
    const said = noticesSaid().length + noticesHeld().length;
    await press("watch-memory", "count");
    await ui.waitFor(() => (made("debug_watch_data").some((one) => one.args.name === "count") || told(/cannot stop where memory changes/)) && noticesSaid().length + noticesHeld().length > said, 10000, "the watch asked of the debugger and its answer said");
    await press("machine-code");
    await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the frame's menu of its instructions");
    closeMenu(false);
    await press("stop-debug");
    await ui.waitFor(() => !debugging(), 15000, "the session ended");
  } finally {
    await endSessions();
    editing().editor.goTo(4, 0);
    if (breakpointsOf("mem.c")) {
      await runCommand("breakpoint");
    }
  }
}, { timeout: 120000 });

test("the window is put back as the file found it", putBackAsFound);
