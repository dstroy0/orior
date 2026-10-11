// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Keys. Each command's keys are those commands.json gives it, then those of the set of keys the reader
// chose, orior's own or JetBrains', then the reader's own, set in Preferences, Keys: two at most for
// a command, each a press, two presses, a button of the mouse past its first three, or the wheel
// turned with a key held. A letter or a digit is bound by the character it types or by its place on
// the board, as the reader sets. By the character, Undo is the key that types z on every layout that
// types the Latin alphabet, and on a layout that types none of it, as a Greek or a Cyrillic one does,
// a letter is bound by its place. The window's own keys work on every layout either way.

const KEY = "orior.keys";

// The sets of keys to choose from: each command's keys where the set differs from orior's own, none
// for a command the set leaves without keys.
export const SETS = {
  orior: { name: "orior", keys: {} },
  jetbrains: {
    name: "JetBrains",
    keys: {
      "create-file": ["Alt+Insert"],
      "new-window": [],
      "close-editor": ["Ctrl+F4"],
      preferences: ["Ctrl+Alt+S"],
      redo: ["Ctrl+Shift+Z"],
      replace: ["Ctrl+R"],
      format: ["Ctrl+Alt+L"],
      rename: ["Shift+F6"],
      "quick-fix": ["Alt+Enter"],
      "parameter-info": ["Ctrl+P"],
      "quick-doc": ["Ctrl+Q"],
      "select-line": [],
      "copy-line-up": [],
      "copy-line-down": ["Ctrl+D"],
      "move-line-up": ["Shift+Alt+Up"],
      "move-line-down": ["Shift+Alt+Down"],
      "join-lines": ["Ctrl+Shift+J"],
      "next-occurrence": ["Alt+J"],
      "all-occurrences": ["Ctrl+Alt+Shift+J"],
      "expand-selection": ["Ctrl+W"],
      "shrink-selection": ["Ctrl+Shift+W"],
      palette: ["Ctrl+Shift+A"],
      "side-bar": ["Alt+1"],
      "terminal-view": ["Alt+F12"],
      "move-to-split": [],
      back: ["Ctrl+Alt+Left"],
      forward: ["Ctrl+Alt+Right"],
      file: ["Ctrl+Shift+N"],
      symbol: ["Ctrl+F12"],
      "symbol-tree": ["Ctrl+Alt+Shift+N"],
      bracket: ["Ctrl+Shift+M"],
      definition: ["Ctrl+B"],
      "peek-definition": ["Ctrl+Shift+I"],
      usages: ["Alt+F7"],
      "next-problem": ["F2"],
      "previous-problem": ["Shift+F2"],
      "next-change": ["Ctrl+Alt+Shift+Down"],
      "previous-change": ["Ctrl+Alt+Shift+Up"],
      bookmark: ["F11"],
      bookmarks: ["Shift+F11"],
      continue: ["F9"],
      start: ["Shift+F10"],
      "stop-debug": ["Ctrl+F2"],
      stop: ["Ctrl+F2"],
      "run-file": ["Ctrl+Shift+F10"],
      "restart-debug": ["Ctrl+F5"],
      "step-over": ["F8"],
      "step-into": ["F7"],
      "step-out": ["Shift+F8"],
      breakpoint: ["Ctrl+F8"],
    },
  },
};

const listeners = [];

// The reader's settings of keys: the set chosen, whether a letter is bound by its "character" or its
// "place", and the reader's own keys by command.
export function keySettings() {
  try {
    const kept = JSON.parse(localStorage.getItem(KEY) ?? "{}") ?? {};
    return { set: SETS[kept.set] ? kept.set : "orior", by: kept.by === "place" ? "place" : "character", own: kept.own && typeof kept.own === "object" ? kept.own : {} };
  } catch {
    return { set: "orior", by: "character", own: {} };
  }
}

export function setKeySettings(change) {
  localStorage.setItem(KEY, JSON.stringify({ ...keySettings(), ...change }));
  listeners.forEach((listener) => listener());
}

// Tells `listener` each time the keys change.
export function onKeys(listener) {
  listeners.push(listener);
}

// The keys of the command `command`, whose own keys in commands.json are `base`: the reader's, else
// the set's, else its own.
export function keysOf(command, base) {
  const { set, own } = keySettings();
  const keys = own[command] ?? SETS[set].keys[command] ?? base;
  return keys.filter(Boolean).slice(0, 2);
}

// The name of the key `event` presses for a letter or a digit, as the reader binds keys: the character
// it types where that is a letter of the Latin alphabet or a digit, else its place on the board.
export function letterOf(event, by = keySettings().by) {
  const place = (event.code ?? "").match(/^Key([A-Z])$/) ?? (event.code ?? "").match(/^Digit(\d)$/);
  if (by === "character" && /^[a-z0-9]$/i.test(event.key ?? "")) {
    return event.key.toUpperCase();
  }
  return place ? place[1] : null;
}

const NAMED = { Backquote: "`", Backslash: "\\", Slash: "/", ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right", Space: "Space", Minus: "-", Equal: "=", Comma: ",", Period: ".", Semicolon: ";", Quote: "'", BracketLeft: "[", BracketRight: "]" };

const modifiers = (event) => [event.ctrlKey && "Ctrl", event.altKey && "Alt", event.shiftKey && "Shift"].filter(Boolean);

// The name commands.json gives the press `event`, as Ctrl+Shift+K, or null for a modifier alone.
export function pressName(event) {
  if (["Control", "Shift", "Alt", "Meta"].includes(event.key)) {
    return null;
  }
  const key = letterOf(event) ?? NAMED[event.code] ?? (/^F\d{1,2}$/.test(event.key) ? event.key : event.key?.length === 1 ? event.key : event.key);
  return [...modifiers(event), key].join("+");
}

// The name of a press of the mouse's button `event` past its first three: Mouse4 back, Mouse5 forward.
export function buttonName(event) {
  const number = { 3: "Mouse4", 4: "Mouse5" }[event.button];
  return number ? [...modifiers(event), number].join("+") : null;
}

// The name of the wheel turned with a key held, as Ctrl+Wheel Up, or null with no key held.
export function wheelName(event) {
  const held = modifiers(event);
  return held.length && event.deltaY ? [...held, event.deltaY < 0 ? "Wheel Up" : "Wheel Down"].join("+") : null;
}

// Each key more than one of the commands `items` holds, with those commands.
export function sharedKeys(items) {
  const by = new Map();
  for (const item of items) {
    for (const key of [item.keys, item.also].filter(Boolean)) {
      if (!by.has(key)) {
        by.set(key, []);
      }
      if (!by.get(key).includes(item)) {
        by.get(key).push(item);
      }
    }
  }
  return new Map([...by].filter(([, held]) => held.length > 1));
}
