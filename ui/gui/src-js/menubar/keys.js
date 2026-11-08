// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { bindEditorKeys } from "../edit.js";
import { menuOpen } from "../menu.js";
import { keysOf as commandKeys, letterOf } from "../keys.js";
import { runCommand, state } from "../menubar.js";
import { NEEDS } from "./checks.js";

// The key names commands.json writes, as the page names the key: a letter or digit by its place on
// the keyboard, and the rest by name.
// The menus' names for keys a code names otherwise, the other way about from KEY_CODES.
export const KEY_NAMES = { Backquote: "`", Backslash: "\\", Slash: "/", ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right" };

const KEY_CODES = { "`": "Backquote", "\\": "Backslash", "/": "Slash", Up: "ArrowUp", Down: "ArrowDown", Left: "ArrowLeft", Right: "ArrowRight" };

// Whether an event is the keys an item lists. Two presses, as Ctrl+K Ctrl+0, are the editor's.
function pressed(keys, event) {
  if (!keys || keys.includes(" ")) {
    return false;
  }
  const parts = keys.split("+");
  const key = parts.pop() || "+";
  if (parts.includes("Ctrl") !== event.ctrlKey || parts.includes("Shift") !== event.shiftKey || parts.includes("Alt") !== event.altKey) {
    return false;
  }
  if (/^[A-Z]$/.test(key) || /^\d$/.test(key)) {
    return letterOf(event) === key;
  }
  return KEY_CODES[key] ? event.code === KEY_CODES[key] : event.key === key;
}

// Runs the command whose keys an event is, where nothing nearer the focus took the keys first. The
// keys of a command for the editor are the editor's own, and a text field or the run's output keeps
// them for itself. Of two commands with the same keys, the first that can act now runs, as F5
// continues a stopped program and otherwise starts the chosen job. A key bound here never reaches
// the web view, even where no command of it can act.
export function onShortcut(event) {
  if (event.defaultPrevented || menuOpen()) {
    return;
  }
  let bound = false;
  for (const menu of state.menus) {
    for (const item of menu.all) {
      if (item.needs !== "editor" && (pressed(item.keys, event) || pressed(item.also, event))) {
        bound = true;
        if (!item.needs || NEEDS[item.needs]?.()) {
          event.preventDefault();
          runCommand(item.command);
          return;
        }
      }
    }
  }
  if (bound) {
    event.preventDefault();
  }
}

// The keys of the commands `items`, each for one type of file, offered to the editor first: a key goes
// to the command whose type the file shown is, of those that share it, as Ctrl+Enter goes to Run
// Statement in an SQL file and to Send Request in a file of requests, and to the editor's own use of
// the key where the file is of none of their types.
export function firstKeys(items, needs = NEEDS, run = runCommand) {
  const byKeys = new Map();
  for (const item of items) {
    for (const keys of [item.keys, item.also].filter(Boolean)) {
      byKeys.set(keys, [...(byKeys.get(keys) ?? []), item]);
    }
  }
  return [...byKeys].map(([keys, commands]) => ({
    keys,
    first: true,
    run: () => {
      const found = commands.find((item) => needs[item.needs]?.());
      if (found) {
        run(found.command);
      }
      return Boolean(found);
    },
  }));
}

// Gives each command its keys, as the reader's keys, then the set chosen, then commands.json give them
// (keys.js), and binds those the editor takes first.
export function applyKeys() {
  for (const item of state.menus.flatMap((menu) => menu.all)) {
    item.base ??= [item.keys, item.also].filter(Boolean);
    const [keys = "", also = ""] = item.command ? commandKeys(item.command, item.base) : item.base;
    item.keys = keys;
    item.also = also;
  }
  bindEditorKeys(
    state.menus.flatMap((menu) =>
      menu.all.filter((item) => item.needs === "editor").flatMap((item) => [item.keys, item.also].filter(Boolean).map((keys) => ({ keys, run: () => runCommand(item.command), native: Boolean(item.native) }))),
    ).concat(
      firstKeys(state.menus.flatMap((menu) => menu.all.filter((item) => item.needs === "sql" || item.needs === "http"))),
    ),
  );
}

// Every command of the menus, each once, for Preferences' keys.
export const allCommands = () => state.menus.flatMap((menu) => menu.all.map((item) => ({ ...item, menu: menu.title, item })));

// Runs the command that holds `name`, a button of the mouse or the wheel turned with a key pressed, where
// one does and can act now; says whether one did.
export function runPressed(name) {
  if (!name) {
    return false;
  }
  for (const menu of state.menus ?? []) {
    for (const item of menu.all) {
      if ((item.keys === name || item.also === name) && (!item.needs || item.needs === "editor" || NEEDS[item.needs]?.())) {
        runCommand(item.command);
        return true;
      }
    }
  }
  return false;
}

