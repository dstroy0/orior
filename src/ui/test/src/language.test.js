// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window in the reader's language, from a language file in orior's own folder: the menus, the
// sheets drawn later and the fields' placeholders in its words, the files' names and the editor's
// text as they are, and English again once it is chosen. The file here is the pseudo-language
// qps-ploc, each English text in brackets.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { language, languages, setLanguage } from "../../src/language.js";
import { runCommand } from "../../src/menubar.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const WORDS = ["File", "Edit", "Preferences", "Search", "Language"];

const titled = (text) => [...document.querySelectorAll(".bar-title")].some((one) => one.textContent.trim() === text);

test("the window shows in the language a file gives, and the files' names and the editor's text stay as they are", async () => {
  const home = await invoke("home_folder");
  await invoke("file_write_any", { path: `${home}/languages/qps-ploc.json`, text: JSON.stringify(Object.fromEntries(WORDS.map((word) => [word, `[${word}]`]))) });
  assert.ok((await languages()).includes("qps-ploc"));
  await invoke("file_write", { path: "Edit", text: "File\n" });
  showView("edit");
  await openFile("Edit");
  await ui.waitFor(() => editing().active === "Edit", 5000);
  await setLanguage("qps-ploc");
  assert.equal(language(), "qps-ploc");
  assert.equal(document.documentElement.lang, "qps-ploc");
  await ui.waitFor(() => titled("[File]") && titled("[Edit]"), 3000);
  assert.ok(!titled("File"), "the menu's title is in the language's words");
  assert.equal([...document.querySelectorAll(".bar-title")].find((one) => one.textContent === "[File]").querySelector("u")?.textContent, "F", "its key's letter still underlined");
  assert.equal(editing().editor.s.doc.line(0), "File", "the editor's text is the reader's");
  assert.ok([...document.querySelectorAll("#files .name, .tab")].some((one) => one.textContent.includes("Edit") && !one.textContent.includes("[Edit]")), "a file's name is its own");
  runCommand("preferences");
  await ui.waitFor(() => document.querySelector(".sheet-prefs h2")?.textContent === "[Preferences]", 3000);
  assert.ok([...document.querySelectorAll(".sheet-prefs [placeholder]")].some((one) => one.getAttribute("placeholder") === "[Search]"), "a placeholder drawn later is in its words");
  document.querySelector("dialog.sheet")?.close();
  await setLanguage("en");
  await ui.waitFor(() => titled("File") && !titled("[File]"), 3000);
  assert.equal(document.documentElement.lang, "en");
  await setLanguage("");
});
