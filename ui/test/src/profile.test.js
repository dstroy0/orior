// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Profiling: a file profiled, stopped, profiled on a machine, and the profile's menu.

import { profileBars, profileZoom } from "../../gui/src-js/profile.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { showView } from "../../gui/src-js/views.js";
import { asFound, calls, made, press, putBackAsFound } from "../menus.js";
import { closeMenu } from "../../gui/src-js/menu.js";

const { test, assert, ui } = window.__harness;

test("Run, Profile profiles the file shown, Stop Profiling stops it, and Profile on a Machine profiles there", async () => {
  await invoke("file_write", { path: "profiled.py", text: "import time\nfor _ in range(200):\n    time.sleep(0.05)\n" });
  showView("edit");
  await openFile("profiled.py");
  await press("profile");
  assert.ok(!document.getElementById("profile").hidden, "the profile shown");
  assert.ok(made("profile_start").some((one) => one.args.path === "profiled.py" && !one.args.remote));
  await press("stop-profile");
  assert.ok(made("profile_stop").length);
  calls.answer("profile_start", () => null);
  await press("profile-remote", "me@machine:/tree");
  assert.ok(made("profile_start").some((one) => one.args.remote === "me@machine:/tree"));
  runCommand("profile-view");
});

test("the profile's menu, from the command line: a function opened, zoomed to, zoomed out of, and the whole run shown", async () => {
  asFound();
  const file = "profiled_calls.py";
  await invoke("file_write", { path: file, text: "import time\n\n\ndef slow():\n    time.sleep(0.02)\n\n\ndef outer():\n    for _ in range(150):\n        slow()\n\n\nouter()\n" });
  showView("edit");
  await openFile(file);
  await press("profile");
  try {
    await ui.waitFor(() => profileBars().some((bar) => bar.name === "slow"), 60000, "the profile's bars, slow among them");
    // The graph's own menu, opened on a bar, runs the same commands.
    const bar = profileBars().find((one) => one.name === "outer");
    const canvas = document.querySelector("#profile .profile-graph");
    const box = canvas.getBoundingClientRect();
    canvas.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, button: 2, clientX: box.left + bar.x + Math.min(10, bar.w / 2), clientY: box.top + bar.y + 9 }));
    await ui.waitFor(() => document.querySelector("body > .menu"), 3000, "the graph's menu");
    const items = [...document.querySelectorAll("body > .menu > .menu-item")].map((button) => button.item);
    closeMenu(false);
    assert.deepEqual(items.map((item) => [item.command, item.args.join(" ")]), [["profile-open", "outer"], ["profile-zoom", "outer"], ["profile-zoom-out", ""], ["profile-whole", ""]]);
    await press("profile-zoom", "outer");
    const once = profileZoom().length;
    assert.ok(once > 0, "zoomed to outer");
    await press("profile-zoom", "slow");
    assert.ok(profileZoom().length > once, "and on to slow");
    await press("profile-zoom-out");
    assert.equal(profileZoom().length, once, "out by one");
    await press("profile-whole");
    assert.deepEqual(profileZoom(), [], "the whole run");
    await openFile("hello.py");
    await press("profile-open", "slow");
    await ui.waitFor(() => editing().active === file && editing().editor.primary().head.line === 3, 5000, "slow open where it is written");
  } finally {
    await press("stop-profile");
    runCommand("profile-view");
  }
}, { timeout: 120000 });

test("the window is put back as the file found it", putBackAsFound);
