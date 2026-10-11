// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Themes that reach further: a color for every kind of token a grammar names, an image behind the
// editor, file icons matched to names by patterns, and Preferences marking each color whose contrast
// with the color under it is too low.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { tokenKinds } from "../../src/editor/tokens.js";
import { iconOf } from "../../src/explorer.js";
import { runCommand } from "../../src/menubar.js";
import { scheme } from "../../src/scheme.js";
import { choose, contrastOf, deleteTheme, paletteOf, readTheme, saveTheme, themeText } from "../../src/themes.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

// Makes the theme `name` over the scheme shown, from `parts`, and chooses it.
function useTheme(name, parts) {
  saveTheme({ name, base: scheme(), colors: {}, ...parts });
  choose(scheme(), name);
}

function leave(name) {
  choose(scheme(), null);
  deleteTheme(name);
}

async function opened(path, text) {
  await invoke("file_write", { path, text });
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path, 5000);
  return editing().editor;
}

test("a theme colors a kind of token the stylesheet has no color of its own for, and the rest keep theirs", async () => {
  await opened("themes_a.js", "const said = `one\\ntwo`;\n");
  assert.ok(tokenKinds().includes("string.escape"), "a kind a grammar names");
  assert.ok(paletteOf(scheme()).has("--t-string-escape"), "and its color in the palette");
  const editor = editing().editor;
  const escape = () => editor.host.querySelector(".t-string-escape");
  await ui.waitFor(() => escape(), 5000);
  const string = getComputedStyle(editor.host.querySelector(".t-string:not(.t-string-escape)")).color;
  assert.equal(getComputedStyle(escape()).color, string, "a kind no theme colors takes the color of the kind it is part of");
  useTheme("Escapes for Themes", { colors: { "--t-string-escape": "#ff0000" } });
  await ui.waitFor(() => escape() && getComputedStyle(escape()).color === "rgb(255, 0, 0)", 2000);
  assert.equal(getComputedStyle(editor.host.querySelector(".t-string:not(.t-string-escape)")).color, string);
  leave("Escapes for Themes");
  await ui.waitFor(() => escape() && getComputedStyle(escape()).color === string, 2000);
});

test("a theme lays an image behind the editor's text at the share it sets, read from a picture", async () => {
  const root = document.getElementById("tree-path").textContent.replace(/[\\/]+$/, "");
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><rect width="8" height="8" fill="#6fdcb4"/></svg>';
  await invoke("file_write", { path: "themes_picture.svg", text: svg });
  const src = await invoke("image_data", { path: `${root}/themes_picture.svg` });
  assert.ok(src.startsWith("data:image/svg+xml;base64,"));
  assert.equal(atob(src.slice(src.indexOf(",") + 1)), svg);
  await invoke("file_write", { path: "themes_not_picture.txt", text: "words" });
  const refused = await invoke("image_data", { path: `${root}/themes_not_picture.txt` }).catch((error) => String(error));
  assert.match(refused, /not a picture of a kind a theme takes/);
  const editor = await opened("themes_b.txt", "text over a picture\n");
  useTheme("Picture for Themes", { image: { src, opacity: 0.3 } });
  const behind = () => getComputedStyle(editor.host, "::before");
  await ui.waitFor(() => behind().backgroundImage.includes("data:image/svg+xml"), 2000);
  assert.equal(behind().opacity, "0.3");
  assert.equal(Number(behind().zIndex), -1, "under the text");
  runCommand("preferences");
  await ui.waitFor(() => document.querySelector(".prefs-image-share"), 5000);
  const share = document.querySelector(".prefs-image-share");
  share.value = "0.5";
  share.dispatchEvent(new Event("change"));
  await ui.waitFor(() => behind().opacity === "0.5", 2000);
  await ui.click([...document.querySelectorAll(".prefs-image button")].find((one) => one.textContent === "Remove"));
  await ui.waitFor(() => behind().backgroundImage === "none", 2000);
  document.querySelector("dialog.sheet")?.close();
  leave("Picture for Themes");
});

test("a file takes the icon of the first of the theme's patterns its name matches", async () => {
  useTheme("Icons for Themes", {
    icons: [
      { pattern: "*.test.js", mark: "T", color: "--t2" },
      { pattern: "*.js", mark: "J", color: "#123456" },
    ],
  });
  assert.equal(iconOf("a.test.js").textContent, "T");
  assert.equal(iconOf("a.test.js").style.color, "var(--t2)");
  assert.equal(iconOf("B.JS").textContent, "J", "a pattern matches a name as the file system does, any case");
  assert.equal(iconOf("b.js").style.color, "rgb(18, 52, 86)");
  assert.equal(iconOf("c.py").textContent, "py", "a name no pattern matches keeps its type's");
  await opened("themes_c.test.js", "test();\n");
  await ui.waitFor(() => [...document.querySelectorAll(".icon")].some((one) => one.textContent === "T" && one.parentElement.textContent.includes("themes_c.test.js")), 3000);
  runCommand("preferences");
  await ui.waitFor(() => document.querySelectorAll(".prefs-icon-row").length === 2, 5000);
  await ui.click([...document.querySelectorAll(".prefs-icons button")].find((one) => one.textContent === "Add"));
  await ui.waitFor(() => document.querySelectorAll(".prefs-icon-row").length === 3, 2000);
  const row = () => [...document.querySelectorAll(".prefs-icon-row")].at(-1);
  row().scrollIntoView({ block: "center" });
  await ui.click(row().querySelector(".prefs-icon-pattern"));
  await ui.type("*.py");
  await ui.click(row().querySelector(".prefs-icon-mark"));
  await ui.type("P");
  await ui.key("Tab");
  await ui.waitFor(() => iconOf("c.py").textContent === "P", 2000);
  document.querySelector("dialog.sheet")?.close();
  const copied = readTheme(themeText({ name: "Icons Copied", base: scheme(), colors: { "--t-string-escape": "#00ff00" }, icons: [{ pattern: "*.md", mark: "M", color: "--t4" }] }));
  assert.deepEqual(copied.icons, [{ pattern: "*.md", mark: "M", color: "--t4" }], "a theme copied out and read back keeps its icons");
  assert.equal(copied.colors["--t-string-escape"], "#00ff00", "and its colors of kinds of token");
  leave("Icons for Themes");
});

test("Preferences marks each color whose contrast with the color under it is too low for text", async () => {
  assert.ok(Math.abs(contrastOf("#000000", "#ffffff") - 21) < 0.01);
  assert.ok(Math.abs(contrastOf("#777777", "#777777") - 1) < 0.01);
  assert.ok(contrastOf("rgba(0, 0, 0, 0)", "#ffffff") < 1.01, "a color with alpha laid over the one under it");
  const under = paletteOf(scheme()).get("--ed-bg");
  useTheme("Faint for Themes", { colors: { "--t-comment": under } });
  runCommand("preferences");
  const row = () => document.querySelector('.prefs-color[data-name="--t-comment"]');
  await ui.waitFor(() => row(), 5000);
  assert.ok(row().classList.contains("low"));
  assert.match(row().querySelector(".prefs-contrast").textContent, /^1\.0:1 on --ed-bg$/);
  assert.match(document.querySelector(".prefs-contrasts").textContent, /--t-comment on --ed-bg, 1\.0:1/);
  await ui.click(".prefs-palette .filter");
  await ui.type("--t-comment");
  await ui.waitFor(() => [...document.querySelectorAll(".prefs-color")].every((one) => one.dataset.name.startsWith("--t-comment")), 2000);
  await ui.click(row().querySelector(".prefs-value"));
  const text = row().querySelector(".prefs-value");
  text.select();
  await ui.type(scheme() === "dark" ? "#ffffff" : "#000000");
  await ui.key("Enter");
  await ui.waitFor(() => !row().classList.contains("low"), 2000);
  document.querySelector("dialog.sheet")?.close();
  leave("Faint for Themes");
});
