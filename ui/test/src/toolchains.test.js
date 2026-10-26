// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The toolchains sheet, beside Preferences, the plugins and the reader's own files.

import { runCommand } from "../../gui/src-js/menubar.js";
import { invoke } from "../../gui/src-js/bridge.js";
import { calls, HOME, made, press, putBackAsFound, ran, sheetShown } from "../menus.js";

const { test, assert } = window.__harness;

test("File, Preferences, Plugins, New Language Plugin, Toolchains and the reader's own files", async () => {
  await press("preferences");
  await sheetShown(".sheet-prefs");
  assert.equal(await ran("user-css"), "ran");
  assert.ok(await invoke("file_read_any", { path: `${HOME()}/user.css` }).then(() => true, () => false), "the command line made user.css in orior's home");
  const revealed = [];
  calls.answer("home_reveal", ({ what }) => revealed.push(what));
  await runCommand("user-css");
  await press("reader-menus");
  assert.deepEqual(revealed, ["user-css", "menus"]);
  assert.equal(await ran("plugins"), "ran");
  await runCommand("plugins");
  await sheetShown(".sheet-plugins");
  assert.equal(await ran("new-plugin", "madelang", "--ext", "madel"), "ran");
  assert.ok(await invoke("file_read_any", { path: `${HOME()}/plugins/madelang/plugin.json` }).then(() => true, () => false), "the plugin made in orior's home");
  await runCommand("new-plugin");
  await sheetShown(".sheet-generator");
  const before = made("plugins_read").length;
  await press("reload-plugins");
  assert.ok(made("plugins_read").length > before, "the plugins read again");
  assert.equal(await ran("toolchains"), "ran");
  await runCommand("toolchains");
  await sheetShown("*");
});

test("the window is put back as the file found it", putBackAsFound);
