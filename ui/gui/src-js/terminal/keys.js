// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a key sends a shell, as an xterm sends it.

const SPECIAL = {
  Enter: "\r",
  Backspace: "\x7f",
  Tab: "\t",
  Escape: "\x1b",
  Insert: "\x1b[2~",
  Delete: "\x1b[3~",
  PageUp: "\x1b[5~",
  PageDown: "\x1b[6~",
  F1: "\x1bOP",
  F2: "\x1bOQ",
  F3: "\x1bOR",
  F4: "\x1bOS",
  F5: "\x1b[15~",
  F6: "\x1b[17~",
  F7: "\x1b[18~",
  F8: "\x1b[19~",
  F9: "\x1b[20~",
  F10: "\x1b[21~",
  F11: "\x1b[23~",
  F12: "\x1b[24~",
};

const CURSOR_KEYS = { ArrowUp: "A", ArrowDown: "B", ArrowRight: "C", ArrowLeft: "D", Home: "H", End: "F" };

// What a key sends the shell, or null for a key the terminal leaves alone.
export function keyText(event, appCursor) {
  const modifiers = (event.shiftKey ? 1 : 0) + (event.altKey ? 2 : 0) + (event.ctrlKey ? 4 : 0);
  if (CURSOR_KEYS[event.key]) {
    const letter = CURSOR_KEYS[event.key];
    return modifiers ? `\x1b[1;${modifiers + 1}${letter}` : `${appCursor ? "\x1bO" : "\x1b["}${letter}`;
  }
  if (event.key === "Tab" && event.shiftKey) {
    return "\x1b[Z";
  }
  if (event.key === "Backspace" && event.ctrlKey) {
    return "\x08";
  }
  if (SPECIAL[event.key]) {
    return (event.altKey ? "\x1b" : "") + SPECIAL[event.key];
  }
  if (event.ctrlKey && !event.altKey && event.key.length === 1) {
    const code = event.key.toUpperCase().charCodeAt(0);
    if (code >= 64 && code <= 95) {
      return String.fromCharCode(code - 64);
    }
    return { " ": "\x00", "/": "\x1f", "?": "\x7f" }[event.key] ?? null;
  }
  if ([...event.key].length === 1 && !event.metaKey) {
    return (event.altKey && !event.ctrlKey ? "\x1b" : "") + event.key;
  }
  return null;
}
