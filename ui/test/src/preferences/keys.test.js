// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences' keys: a command's keys set by pressing them.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { keySettings, setKeySettings } from "../../../gui/src-js/keys.js";
import { allCommands, runCommand } from "../../../gui/src-js/menubar.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const fresh = () => setKeySettings({ set: "orior", by: "character", own: {} });
const commandOf = (name) => allCommands().find((one) => one.command === name).item;

test("a command's keys are set in Preferences by pressing them, and the key pressed twice is shown", async () => {
  fresh();
  await invoke("file_write", { path: "keys.txt", text: "one\ntwo\nthree\n" });
  showView("edit");
  await openFile("keys.txt");
  await ui.waitFor(() => editing().active === "keys.txt", 5000);
  runCommand("preferences");
  const row = () => document.querySelector('.prefs-key-row[data-command="line"]');
  await ui.waitFor(() => row(), 5000);
  const filter = document.querySelector(".prefs-keys-filter");
  await ui.click(filter);
  await ui.type("Go to Line");
  await ui.waitFor(() => row()?.checkVisibility(), 2000);
  await ui.click(row().querySelector(".prefs-key-set"));
  await ui.key("Ctrl+Alt+L");
  await ui.waitFor(() => row()?.querySelector("kbd")?.textContent === "Ctrl+Alt+L", 2000);
  assert.deepEqual(keySettings().own.line, ["Ctrl+Alt+L", "Ctrl+G"]);
  assert.equal(commandOf("line").keys, "Ctrl+Alt+L");
  await ui.click(row().querySelector(".prefs-key-set"));
  await ui.key("Ctrl+P");
  await ui.waitFor(() => [...document.querySelectorAll(".prefs-shared li")].some((one) => one.textContent.startsWith("Ctrl+P ")), 2000);
  assert.match([...document.querySelectorAll(".prefs-shared li")].find((one) => one.textContent.startsWith("Ctrl+P ")).textContent, /Go to File/);
  await ui.click(row().querySelector(".prefs-button:not(.prefs-key-set)"));
  await ui.waitFor(() => commandOf("line").keys === "Ctrl+G", 2000);
  setKeySettings({ own: { line: ["Ctrl+Alt+L"] } });
  await ui.key("Escape");
  editing().editor.focus();
  await ui.key("Ctrl+Alt+L");
  await ui.waitFor(() => !document.querySelector(".ed-goto")?.hidden, 2000);
  await ui.key("Escape");
  fresh();
});
