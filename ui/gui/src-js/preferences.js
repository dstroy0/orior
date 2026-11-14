// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Preferences: the color theme of each scheme and every color of its palette, the zoom, the
// cores retained for the reader, the opacity of each part of the window, each item of the menus that is
// set on or off, the settings no menu lists, each setting a settings file sets with its value and
// the file it comes from, the settings carried to and from another machine, the patterns that retain
// the explorer, the search and the watching of files from files, the sets of files Find in Files
// searches in, and the keys: the set of keys, how a character's key is bound, each key more than one
// command holds, and each command's keys, set by pressing them. A color changed in the scheme's own
// theme starts a theme of the reader's from it, and every change shows at once; the palette holds a
// color for every type of token a grammar names, and each color whose contrast with the color under
// it is too low for text is marked. A theme positions an image behind the editor and gives files icons
// by patterns of their names. A theme copies out as text and reads back in from the clipboard.

import { displayed } from "./preferences/commands.js";
import { keysSection } from "./preferences/keys.js";
import { colorsSection, themeRow } from "./preferences/look.js";
import { carrySection, fontsSection, inForceSection, noticesSection, patternsSection, settingsSection } from "./preferences/sections.js";
import { readSettings, settingsInForce } from "./settings.js";

export { prefsCommands } from "./preferences/commands.js";

export const drawPreferences = () => displayed.draw();

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function showPreferences(sheet, { menus, runCommand, checks, more = [], openFile = null, reloadOwn = null }) {
  const body = element("div", { className: "sheet-prefs" });
  const state = { filter: "" };

  const said = element("p", { className: "prefs-said", ariaLive: "polite" });

  // The sheet each section is drawn in. `drawContrast` marks each color whose contrast with the one under it is too
  // low, once a color changes; set by the palette's section as it is drawn.
  const at = { body, state, said, draw, drawContrast: () => {}, menus, runCommand, checks, more, openFile, reloadOwn };

  function draw() {
    const focused = document.activeElement;
    const focusName = body.contains(focused) ? focused.closest(".prefs-color")?.dataset.name : null;
    // A field the reader was in, as a list stepped through with the keys, retains the keys once the
    // sheet is drawn again: found again by its label.
    const focusLabel = body.contains(focused) && !focusName ? focused.getAttribute("aria-label") : null;
    const scroll = body.querySelector(".prefs-colors")?.scrollTop ?? 0;
    const keysScroll = body.querySelector(".prefs-key-rows")?.scrollTop ?? 0;
    body.replaceChildren(element("h2", { textContent: "Preferences" }), settingsSection(at), inForceSection(at), noticesSection(at), carrySection(at), keysSection(at), fontsSection(), patternsSection(), themeRow(at), colorsSection(at));
    const keyRows = body.querySelector(".prefs-key-rows");
    if (keyRows) {
      keyRows.scrollTop = keysScroll;
    }
    const grid = body.querySelector(".prefs-colors");
    grid.scrollTop = scroll;
    if (focusName) {
      body.querySelector(`.prefs-color[data-name="${CSS.escape(focusName)}"] .prefs-value`)?.focus();
    } else if (focusLabel) {
      body.querySelector(`[aria-label="${CSS.escape(focusLabel)}"]`)?.focus();
    }
  }

  draw();
  sheet(body);
  displayed.draw = () => body.isConnected && draw();
  // The files are read again as Preferences opens, so that each setting retained since shows with its
  // file; the settings in force are drawn again where the reading changed them.
  const before = JSON.stringify(settingsInForce());
  readSettings().then((read) => {
    if (body.isConnected && JSON.stringify(read) !== before) {
      body.querySelector(".prefs-force")?.replaceWith(inForceSection(at));
    }
  });
}
