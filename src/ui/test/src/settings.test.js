// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Settings by file: a tree's .orior/settings.json standing over the reader's own, read again as it
// changes, a part for one system alone, a file that takes another's, values that name other values
// and the system's, a theme for a kind of file, and Preferences showing each setting's value and the
// file it comes from.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { readSettings, settingsInForce, TREE_FILE } from "../../src/settings.js";
import { noticesHeld, noticesSaid } from "../../src/statusbar.js";
import { deleteTheme, saveTheme } from "../../src/themes.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

// Writes the tree's settings file, made with its folder where it is not there.
async function treeSettings(text) {
  await invoke("folder_create", { path: ".orior" }).catch(() => {});
  await invoke("file_write", { path: TREE_FILE, text });
}

async function opened(path, text) {
  await invoke("file_write", { path, text });
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path, 5000);
  return editing().editor;
}

const told = (start) => [...noticesSaid(), ...noticesHeld()].some((one) => one.text.startsWith(start));

test("a tree's settings file stands over the reader's own as it is saved, and Preferences shows each setting's value and its file", async () => {
  runCommand("whitespace", ["off"]);
  // The reader's own is kept in settings.json before the files are read.
  await new Promise((done) => setTimeout(done, 600));
  const editor = await opened("settings_a.txt", "one two\n");
  assert.equal(editor.marksOn, false);
  await treeSettings('{\n  // the tree\'s own\n  "whitespace": "true",\n  "checkers": "ruff",\n}\n');
  await ui.waitFor(() => editor.marksOn === true, 5000);
  assert.equal(localStorage.getItem("orior.whitespace"), "true");
  const from = settingsInForce().from.whitespace;
  assert.equal(from[0].file, TREE_FILE);
  assert.match(from[1].file, /settings\.json$/, "the reader's own, under the tree's");
  assert.equal(from[1].text, "false");
  runCommand("whitespace", ["off"]);
  await ui.waitFor(() => told("whitespace stays as .orior/settings.json sets it"), 3000);
  assert.equal(localStorage.getItem("orior.whitespace"), "true", "the tree's stays in force");
  await ui.waitFor(() => editor.marksOn === true, 2000);
  runCommand("preferences");
  const row = () => document.querySelector('.prefs-force-row[data-key="whitespace"]');
  await ui.waitFor(() => row(), 5000);
  assert.equal(row().querySelector(".prefs-force-value").textContent, "true");
  assert.equal(row().querySelector(".prefs-file").textContent, TREE_FILE);
  assert.match(row().querySelector(".prefs-force-under").textContent, /settings\.json/);
  assert.ok([...document.querySelectorAll(".prefs-force-problems li")].some((one) => /checkers is the reader's own to set/.test(one.textContent)), "a tree's file does not name the checkers");
  row().scrollIntoView({ block: "center" });
  await ui.click(row().querySelector(".prefs-file"));
  await ui.waitFor(() => editing().active === TREE_FILE, 5000);
  assert.ok(!document.querySelector("dialog.sheet"), "Preferences closed for the file it opened");
  await treeSettings("{}\n");
  await ui.waitFor(() => editor.marksOn === false, 5000);
});

test("a file takes another's and changes some of it, a part sets one system's alone, and a value names others and the system", async () => {
  await invoke("file_write", { path: ".orior/base.json", text: '{ "sticky": "false", "doc-margin": 60 }' });
  await treeSettings(
    JSON.stringify({
      takes: "base.json",
      "doc-margin": "${setting:wide}",
      wide: 88,
      made: "${system} ${env:ORIOR_HOME}",
      windows: { mark: "w" },
      linux: { mark: "l" },
      macos: { mark: "m" },
    }),
  );
  const read = await readSettings();
  const editor = await opened("settings_b.txt", "text\n");
  assert.equal(editor.stickyOn, false, "the file taken sets it");
  assert.equal(editor.docMargin, 88, "the file's own stands over the one it takes, and names another value");
  assert.deepEqual(read.from["doc-margin"].map((one) => one.file), [TREE_FILE, ".orior/base.json"]);
  assert.match(localStorage.getItem("orior.made"), new RegExp(`^${read.system} \\S`), "the system and a variable of its environment");
  assert.equal(localStorage.getItem("orior.mark"), { windows: "w", linux: "l", macos: "m" }[read.system]);
  assert.equal(read.from.mark[0].part, read.system);
  await treeSettings("{}\n");
  await readSettings();
  assert.equal(editor.stickyOn, true);
  assert.equal(localStorage.getItem("orior.made"), null);
});

test("a kind of file is shown in a theme of its own while the rest keep the window's", async () => {
  saveTheme({ name: "Paper for Settings", base: "light", colors: { "--ed-bg": "#fdf6e3" } });
  await treeSettings('{ "[markdown]": { "theme.dark": "Paper for Settings", "theme.light": "Paper for Settings" } }');
  await readSettings();
  const editor = await opened("settings_c.md", "# A page\n\nText.\n");
  await ui.waitFor(() => editor.host.hasAttribute("data-themed"), 3000);
  assert.equal(getComputedStyle(editor.host).getPropertyValue("--ed-bg").trim(), "#fdf6e3");
  assert.equal(getComputedStyle(editor.host).backgroundColor, "rgb(253, 246, 227)");
  await opened("settings_c.txt", "plain\n");
  await ui.waitFor(() => !editor.host.hasAttribute("data-themed"), 3000);
  assert.notEqual(getComputedStyle(editor.host).getPropertyValue("--ed-bg").trim(), "#fdf6e3");
  await treeSettings("{}\n");
  await readSettings();
  deleteTheme("Paper for Settings");
});
