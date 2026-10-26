// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { say } from "../statusbar.js";
import { icon } from "../icons.js";
import { drawMenubar, jobsIn, runCommand, state } from "../menubar.js";
import { NEEDS } from "./checks.js";

// The menus the bar has a title for: each of commands, and each of jobs that has a job in the tree.
export function shownMenus() {
  return laidOut().filter((menu) => !menu.strip && !menu.popup && (menu.items?.length || jobsIn(menu.groups ?? []).length));
}

// Menus and a toolbar of the reader's, from menus.json in orior's own folder. Each item names a
// command of orior's menus, with a label of the reader's where it gives one, or a line, or a list of
// items under a label of its own. A menu of the reader's named as one of orior's stands in its place,
// the others stand before Help, and "hide" takes orior's menus away by name. The keys of every
// command stay as orior's menus bind them, wherever the reader's menus put it. The file is read as
// the window starts and each time it comes back to the front.

// One of the reader's items, made an item of the menus: a command's own, under the reader's label
// where it gives one; one that names no command is drawn but cannot be chosen.
function readerItem(given, commands) {
  if (given === "-") {
    return "-";
  }
  const asked = typeof given === "string" ? { command: given } : given && typeof given === "object" ? given : {};
  if (Array.isArray(asked.items)) {
    return { label: String(asked.label ?? "…"), items: asked.items.map((one) => readerItem(one, commands)) };
  }
  const known = commands.get(asked.command);
  if (!known) {
    return { label: `${asked.label ?? asked.command ?? "?"}: no such command`, command: "", needs: "never" };
  }
  return { ...known, label: typeof asked.label === "string" ? asked.label : known.label };
}

const commandsOf = () => new Map(state.menus.flatMap((menu) => menu.all ?? []).map((item) => [item.command, item]));

// The menus as the bar draws them: orior's, with the reader's laid over them.
function laidOut() {
  const reader = state.reader;
  if (!reader) {
    return state.menus;
  }
  const commands = commandsOf();
  const own = (Array.isArray(reader.menus) ? reader.menus : []).filter((menu) => typeof menu?.title === "string").map((menu) => ({ title: menu.title, items: (Array.isArray(menu.items) ? menu.items : []).map((one) => readerItem(one, commands)) }));
  const hidden = new Set((Array.isArray(reader.hide) ? reader.hide : []).map(String));
  const laid = state.menus.map((menu) => own.find((one) => one.title === menu.title) ?? menu).filter((menu) => !hidden.has(menu.title));
  const added = own.filter((one) => !state.menus.some((menu) => menu.title === one.title));
  const help = laid.findIndex((menu) => menu.title === "Help");
  laid.splice(help < 0 ? laid.length : help, 0, ...added);
  return laid;
}

const TOOLBAR = "orior.toolbar";
export const toolbarShown = () => localStorage.getItem(TOOLBAR) !== "false";

export function setToolbarShown(on = !toolbarShown()) {
  localStorage.setItem(TOOLBAR, String(on));
  drawToolbar();
}

// The toolbar under the menus: a button for each of the reader's toolbar items, its icon the one it
// names or its label's first letter, and a line for each "-".
function drawToolbar() {
  const bar = document.getElementById("toolbar");
  const list = Array.isArray(state.reader?.toolbar) ? state.reader.toolbar : [];
  bar.hidden = !list.length || !toolbarShown();
  const commands = commandsOf();
  bar.replaceChildren(
    ...list.map((given) => {
      if (given === "-") {
        return Object.assign(document.createElement("span"), { className: "toolbar-line" });
      }
      const item = readerItem(given, commands);
      const glyph = typeof given?.icon === "string" ? given.icon : (item.label ?? "?").trim()[0] ?? "?";
      const button = Object.assign(document.createElement("button"), { className: "toolbar-button", type: "button", title: item.keys ? `${item.label} (${item.keys})` : item.label });
      button.setAttribute("aria-label", item.label);
      button.append(icon(glyph.length === 1 ? glyph.toUpperCase() : glyph));
      button.dataset.needs = item.needs ?? "";
      button.addEventListener("click", () => item.command && runCommand(item.command));
      return button;
    }),
  );
  freshenToolbar();
}

// Marks each button of the toolbar that cannot act now.
export function freshenToolbar() {
  for (const button of document.querySelectorAll("#toolbar .toolbar-button")) {
    const need = button.dataset.needs;
    button.disabled = need === "never" || Boolean(need && !NEEDS[need]?.());
  }
}

// Reads menus.json again, and lays the menus and the toolbar out again where it changed.
export async function loadReaderMenus() {
  const text = await invoke("reader_menus_read").catch(() => "");
  if (text === state.readerText) {
    return;
  }
  state.readerText = text;
  try {
    state.reader = text.trim() ? JSON.parse(text) : null;
  } catch (error) {
    state.reader = null;
    say(`menus.json is not JSON: ${error.message}`, { failed: true });
  }
  drawMenubar();
  drawToolbar();
}

// File, Menus and Toolbar: opens menus.json, made first where it is not there, in the program the
// system opens it with.
export async function openReaderMenus() {
  await invoke("home_reveal", { what: "menus" }).catch((error) => say(String(error), { failed: true }));
  await loadReaderMenus();
}

