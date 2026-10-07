// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal: a panel under both views with a shell in it, opened and closed by the bar's button
// or Ctrl+`. The shell starts the first time the panel opens, in the tree's top folder, and closing
// the panel leaves it running. A shell that ends says so, and the next key starts another.
//
// Keys go to the shell as an xterm sends them. Ctrl+C copies where text is chosen in the panel and
// interrupts where none is; Ctrl+Shift+C copies and Ctrl+Shift+V pastes. Every other key the panel
// takes stays out of the rest of the app while the panel holds the keys. The panel's top edge drags
// to make it taller or shorter, and its height is kept between visits.

import { invoke, listen } from "./bridge.js";
import { clipText, copyText, menuOn } from "./menu.js";
import { Screen } from "./screen.js";

// How tall the panel may be dragged is the stylesheet's to say, in its min-height and max-height.
const HEIGHT = "orior.terminal.height";

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

const state = { id: null, opening: null, screen: null, waiting: "", before: null };
const parts = {};

// What a key sends the shell, or null for a key the terminal leaves alone.
function keyText(event, appCursor) {
  const held = (event.shiftKey ? 1 : 0) + (event.altKey ? 2 : 0) + (event.ctrlKey ? 4 : 0);
  if (CURSOR_KEYS[event.key]) {
    const letter = CURSOR_KEYS[event.key];
    return held ? `\x1b[1;${held + 1}${letter}` : `${appCursor ? "\x1bO" : "\x1b["}${letter}`;
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

// Sends text to the shell. Text typed while the shell starts waits for it.
function send(text) {
  if (state.id === null) {
    state.waiting += text;
    openShell();
    return;
  }
  invoke("term_write", { id: state.id, text }).catch(() => {});
}

// The text chosen inside the panel, or nothing.
function chosen() {
  const selection = window.getSelection();
  return selection && !selection.isCollapsed && parts.view.contains(selection.anchorNode) ? selection.toString() : "";
}

// How many characters fit across the panel and how many lines down it.
function measure() {
  const probe = Object.assign(document.createElement("div"), { className: "term-probe", textContent: "M".repeat(10) });
  parts.view.appendChild(probe);
  const box = probe.getBoundingClientRect();
  probe.remove();
  const shape = getComputedStyle(parts.view);
  const across = parts.view.clientWidth - parseFloat(shape.paddingLeft) - parseFloat(shape.paddingRight);
  const down = parts.view.clientHeight - parseFloat(shape.paddingTop) - parseFloat(shape.paddingBottom);
  return { cols: Math.max(2, Math.floor(across / (box.width / 10))), rows: Math.max(2, Math.floor(down / box.height)) };
}

// Fits the screen to the panel, and tells the shell its new size.
function fit() {
  if (parts.panel.hidden) {
    return;
  }
  const { cols, rows } = measure();
  if (!state.screen) {
    state.screen = new Screen(parts.view, cols, rows, send);
    return;
  }
  if (cols === state.screen.cols && rows === state.screen.rowCount) {
    return;
  }
  state.screen.resize(cols, rows);
  if (state.id !== null) {
    invoke("term_resize", { id: state.id, cols, rows }).catch(() => {});
  }
}

function openShell() {
  if (state.id !== null || state.opening) {
    return;
  }
  const { cols, rowCount: rows } = state.screen;
  state.opening = invoke("term_open", { cols, rows })
    .then((id) => {
      state.id = id;
      const waiting = state.waiting;
      state.waiting = "";
      if (waiting) {
        send(waiting);
      }
    })
    .catch((error) => state.screen.write(`\r\n${error}\r\n`))
    .finally(() => (state.opening = null));
}

// Opens the panel or closes it. An open panel takes the keys, and closing it hands them back to
// whatever held them before it opened.
function toggle(open = parts.panel.hidden) {
  parts.panel.hidden = !open;
  parts.button.setAttribute("aria-pressed", String(open));
  if (open) {
    const holder = document.activeElement;
    state.before = holder && holder !== document.body && holder !== parts.keys ? holder : null;
    fit();
    openShell();
    parts.keys.focus();
  } else if (state.before?.isConnected) {
    state.before.focus();
  } else {
    parts.keys.blur();
  }
}

function onKey(event) {
  if (event.isComposing || (event.ctrlKey && event.code === "Backquote")) {
    return;
  }
  const copy = event.ctrlKey && event.code === "KeyC" && (event.shiftKey || chosen());
  if (copy) {
    event.preventDefault();
    event.stopPropagation();
    const text = chosen();
    if (text) {
      copyText(text);
      window.getSelection().removeAllRanges();
    }
    return;
  }
  // Ctrl+Shift+V pastes into the hidden field, and its paste event sends the text on.
  if (event.ctrlKey && event.shiftKey && event.code === "KeyV") {
    event.stopPropagation();
    return;
  }
  const text = keyText(event, state.screen?.modes.appCursor);
  if (text === null) {
    return;
  }
  event.preventDefault();
  event.stopPropagation();
  send(text);
}

// Sends pasted text as the shell takes a paste: lines ended as Enter ends them, and marked as a paste
// where the program in the terminal asked for that.
function pasteText(given) {
  const text = given.replace(/\r?\n/g, "\r");
  if (text) {
    send(state.screen?.modes.paste ? `\x1b[200~${text}\x1b[201~` : text);
  }
}

function onPaste(event) {
  event.preventDefault();
  pasteText(event.clipboardData.getData("text/plain"));
}

// Opens the panel with its shell in `folder`, a path under the tree's top folder.
export function terminalAt(folder) {
  toggle(true);
  const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
  const where = folder ? `${top}/${folder}` : top;
  send(`cd -- '${where.replace(/'/g, "'\\''")}'\r`);
}

// The terminal's menu: copy what is chosen in it, paste, clear the screen, or close the panel.
function terminalItems() {
  const text = chosen();
  return [
    { label: "Copy", keys: "Ctrl+Shift+C", disabled: !text, run: () => copyText(text) },
    { label: "Paste", keys: "Ctrl+Shift+V", run: async () => pasteText(await clipText()) },
    "-",
    { label: "Clear", keys: "Ctrl+L", run: () => send("\x0c") },
    { label: "Close", keys: "Ctrl+`", run: () => toggle(false) },
  ];
}

// Drags the panel's top edge.
function grip(event) {
  event.preventDefault();
  parts.grip.setPointerCapture(event.pointerId);
  const foot = parts.panel.getBoundingClientRect().bottom;
  const shape = getComputedStyle(parts.panel);
  const move = (moved) => {
    const height = Math.max(parseFloat(shape.minHeight), Math.min(parseFloat(shape.maxHeight), foot - moved.clientY));
    parts.panel.style.height = `${height}px`;
  };
  const end = () => {
    parts.grip.removeEventListener("pointermove", move);
    localStorage.setItem(HEIGHT, parts.panel.style.height);
  };
  parts.grip.addEventListener("pointermove", move);
  parts.grip.addEventListener("pointerup", end, { once: true });
}

export async function startTerminal() {
  for (const [name, id] of [
    ["panel", "term"],
    ["view", "term-view"],
    ["keys", "term-keys"],
    ["grip", "term-grip"],
    ["button", "term-toggle"],
  ]) {
    parts[name] = document.getElementById(id);
  }
  const height = localStorage.getItem(HEIGHT);
  if (height) {
    parts.panel.style.height = height;
  }
  await listen("term-out", ({ payload }) => payload.id === state.id && state.screen?.write(payload.text));
  await listen("term-exit", ({ payload }) => {
    if (payload.id !== state.id) {
      return;
    }
    state.id = null;
    state.waiting = "";
    state.screen?.write(`\r\n[exited ${payload.code ?? ""}]\r\n`);
  });
  window.addEventListener(
    "keydown",
    (event) => {
      if (event.ctrlKey && event.code === "Backquote") {
        event.preventDefault();
        toggle();
      }
    },
    true,
  );
  parts.button.addEventListener("click", () => toggle());
  parts.keys.addEventListener("keydown", onKey);
  parts.keys.addEventListener("paste", onPaste);
  // Text an input method composes reaches the hidden field without a key the terminal sends.
  parts.keys.addEventListener("input", (event) => {
    if (!event.isComposing && parts.keys.value) {
      send(parts.keys.value);
      parts.keys.value = "";
    }
  });
  parts.keys.addEventListener("focus", () => state.screen?.setFocus(true));
  parts.keys.addEventListener("blur", () => state.screen?.setFocus(false));
  parts.view.addEventListener("mouseup", (event) => event.button === 0 && !chosen() && parts.keys.focus());
  menuOn(parts.panel, terminalItems);
  parts.grip.addEventListener("pointerdown", grip);
  new ResizeObserver(() => requestAnimationFrame(fit)).observe(parts.view);
}
