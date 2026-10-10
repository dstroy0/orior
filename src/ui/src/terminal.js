// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal: a panel under both views with shells in it, opened and closed from the Terminal menu
// or by Ctrl+`. Each shell is a tab of the panel's strip, named by the folder it stands in; New
// Terminal opens another beside the rest, and Split shows two side by side. A shell starts the first
// time its tab shows, in the tree's top folder, and closing the panel leaves every shell running. A
// shell that ends says so, and the next key starts another in its tab; a tab's × ends its shell and
// closes it.
//
// Keys go to the shell last pressed in, as an xterm sends them. Ctrl+C copies where text is chosen
// in the panel and interrupts where none is; Ctrl+Shift+C copies and Ctrl+Shift+V pastes. Every
// other key the panel takes stays out of the rest of the app while the panel holds the keys. The
// panel's edge drags to make it taller or shorter, and its size is kept between visits.
//
// Its place is kept as well: whether the panel is open, each shell's folder, which a shell says as
// OSC 7 and Git's bash is set to say before each prompt, and its last PLACE_LINES lines, the tab shown
// and the one beside it, kept a moment after a shell writes and as the window closes. The window
// started again shows those lines and opens the panel as it was, each shell in its folder.

import { invoke, listen } from "./bridge.js";
import { clipText, copyText, menuOn } from "./menu.js";
import { Screen } from "./screen.js";

// How tall the panel may be dragged is the stylesheet's to say, in its min-height and max-height.
const HEIGHT = "orior.terminal.height";
const PLACE = "orior.terminal.place";
const PLACE_LINES = 300;
const PLACE_REST = 800;
// How long a shell starting stays quiet before the text typed while it started goes to it.
const SETTLE = 300;

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

// The shells, each a tab: its number in the app, its shell's number once started, its screen and
// the view that shows it, the text typed while it started, the folder it last said it stands in,
// and the lines kept from before the window started. `shown` is the tab shown, `beside` the one
// beside it where the panel is split, and `focused` the one the keys go to.
const state = { terms: [], next: 1, shown: null, beside: null, focused: null, before: null, keeping: 0 };

// What a shell wrote before its tab knew its number, by that number. The system's terminal asks where
// the cursor is before anything else and waits for the answer, which the tab gives once it hears
// the question.
const early = new Map();
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const termOf = (key) => state.terms.find((term) => term.key === key) ?? null;
const focusedTerm = () => termOf(state.focused) ?? termOf(state.shown) ?? state.terms[0] ?? null;
const showing = (term) => term.key === state.shown || term.key === state.beside;

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

// Sends text to a shell, the one the keys go to where none is named. Text typed while the shell
// starts waits until the shell has written and then been quiet for SETTLE: the system's terminal
// reads its first input as the answer to where the cursor is, and the shell takes keys once its
// prompt is up.
function send(text, term = focusedTerm()) {
  if (!term) {
    return;
  }
  if (term.id === null || !term.ready) {
    term.waiting += text;
    openShell(term);
    return;
  }
  write(term, text);
}

function write(term, text) {
  if (term.id !== null) {
    invoke("term_write", { id: term.id, text }).catch(() => {});
  }
}

// The text chosen inside the panel, or nothing.
function chosen() {
  const selection = window.getSelection();
  return selection && !selection.isCollapsed && parts.views.contains(selection.anchorNode) ? selection.toString() : "";
}

// How many characters fit across a shell's view and how many lines down it.
function measure(term) {
  const probe = Object.assign(document.createElement("div"), { className: "term-probe", textContent: "M".repeat(10) });
  term.view.appendChild(probe);
  const box = probe.getBoundingClientRect();
  probe.remove();
  const shape = getComputedStyle(term.view);
  const across = term.view.clientWidth - parseFloat(shape.paddingLeft) - parseFloat(shape.paddingRight);
  const down = term.view.clientHeight - parseFloat(shape.paddingTop) - parseFloat(shape.paddingBottom);
  return { cols: Math.max(2, Math.floor(across / (box.width / 10))), rows: Math.max(2, Math.floor(down / box.height)) };
}

// Fits a shell's screen to its view, and tells the shell its new size.
function fit(term) {
  if (parts.panel.hidden || term.view.hidden) {
    return;
  }
  const { cols, rows } = measure(term);
  if (!term.screen) {
    // What the screen answers the shell goes at once, ahead of anything typed.
    term.screen = new Screen(term.view, cols, rows, (text) => write(term, text), scrollback());
    term.screen.onPlace = (url) => {
      term.at = url;
      drawStrip();
      keepSoon();
    };
    if (term.restore?.length) {
      term.screen.write(`${term.restore.join("\r\n")}\r\n`);
    }
    term.restore = null;
    term.screen.setFocus(term.key === state.focused && document.activeElement === parts.keys);
    return;
  }
  if (cols === term.screen.cols && rows === term.screen.rowCount) {
    return;
  }
  term.screen.resize(cols, rows);
  if (term.id !== null) {
    invoke("term_resize", { id: term.id, cols, rows }).catch(() => {});
  }
}

const fitShown = () => state.terms.filter(showing).forEach(fit);

// The scrollbacks a page loaded before this one left, let go before this page makes any.
const fresh = invoke("scrollback_reset").catch(() => {});

// Where the lines that scroll off a screen go: a file the app holds for that screen, which the
// screen reads back from as the reader scrolls up.
function scrollback() {
  const opened = fresh.then(() => invoke("scrollback_open"));
  return {
    keep: async (lines) => invoke("scrollback_keep", { id: await opened, lines }),
    read: async (from, count) => invoke("scrollback_read", { id: await opened, from, count }),
  };
}

function openShell(term) {
  if (term.id !== null || term.opening || !term.screen) {
    return;
  }
  const { cols, rowCount: rows } = term.screen;
  term.exited = false;
  term.ready = false;
  term.opening = invoke("term_open", { cols, rows, at: term.at })
    .then((id) => {
      term.id = id;
      for (const text of early.get(id) ?? []) {
        heard(term, text);
      }
      early.delete(id);
    })
    .catch((error) => term.screen.write(`\r\n${error}\r\n`))
    .finally(() => {
      term.opening = null;
      drawStrip();
    });
}

// Writes what a shell said to its tab's screen. A shell starting is ready for what was typed once it
// has been quiet for SETTLE.
function heard(term, text) {
  term.screen.write(text);
  if (!term.ready) {
    window.clearTimeout(term.settle);
    term.settle = window.setTimeout(() => {
      term.ready = true;
      const waiting = term.waiting;
      term.waiting = "";
      if (waiting) {
        write(term, waiting);
      }
    }, SETTLE);
  }
}

// A new tab, its shell not yet started, and the place it was left where one is given.
function addTerm(kept = {}) {
  const view = element("div", { className: "term-view", hidden: true });
  const term = { key: state.next++, id: null, opening: null, screen: null, waiting: "", at: typeof kept.at === "string" ? kept.at : null, restore: Array.isArray(kept.lines) ? kept.lines.map(String) : null, view, exited: false };
  if (!parts.views.querySelector("#term-view")) {
    view.id = "term-view";
  }
  view.addEventListener("mouseup", (event) => {
    if (event.button === 0 && !chosen()) {
      focusTerm(term);
    }
  });
  parts.views.append(view);
  state.terms.push(term);
  return term;
}

// A tab's name: the last folder of the place its shell stands in, after its number.
function nameOf(term, at) {
  const place = term.at ? decodeURIComponent(term.at.replace(/^file:\/\/[^/]*/, "")).replace(/[\\/]+$/, "") : "";
  return `${at + 1} ${place.split(/[\\/]/).pop() || "shell"}`;
}

// Draws the strip: a tab for each shell, then New Terminal and Split.
function drawStrip() {
  if (!parts.strip) {
    return;
  }
  const tabs = state.terms.map((term, at) => {
    const close = element("span", { className: "close", textContent: "×", title: "End the shell and close its tab" });
    const tab = element("button", { className: `term-tab${term.exited ? " exited" : ""}`, type: "button", role: "tab", title: term.at ?? "shell" }, element("span", { textContent: nameOf(term, at) }), close);
    tab.setAttribute("aria-selected", String(showing(term)));
    tab.addEventListener("click", (event) => (event.target === close ? closeTerm(term) : showTerm(term)));
    tab.addEventListener("auxclick", (event) => event.button === 1 && closeTerm(term));
    return tab;
  });
  const tool = (label, path, run, on = false) => {
    const button = element("button", { className: `term-tool${on ? " on" : ""}`, type: "button", title: label });
    button.setAttribute("aria-label", label);
    button.innerHTML = `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="${path}"/></svg>`;
    button.addEventListener("click", run);
    return button;
  };
  parts.strip.replaceChildren(
    element("div", { className: "term-tabs", role: "tablist" }, ...tabs),
    tool("New Terminal (Ctrl+Shift+`)", "M8 3.5v9M3.5 8h9", () => newTerminal()),
    tool("Split Terminal", "M2.5 3.5h11v9h-11zM8 3.5v9", () => splitTerminal(), state.beside !== null),
  );
}

// Shows the shown tab and the one beside it, each started, and the keys with the one they go to.
function drawViews() {
  for (const term of state.terms) {
    term.view.hidden = !showing(term);
    term.view.classList.toggle("term-chosen", state.beside !== null && term.key === state.focused);
  }
  parts.views.dataset.split = String(state.beside !== null);
  drawStrip();
  if (!parts.panel.hidden) {
    requestAnimationFrame(() => {
      fitShown();
      state.terms.filter(showing).forEach(openShell);
    });
  }
}

function focusTerm(term) {
  state.focused = term.key;
  for (const one of state.terms) {
    one.screen?.setFocus(one === term && document.activeElement === parts.keys);
  }
  drawViews();
  parts.keys.focus();
}

// Shows a tab: in the place of the one the keys go to where the panel is split, else alone.
function showTerm(term) {
  if (!showing(term)) {
    if (state.beside !== null && state.focused === state.beside) {
      state.beside = term.key;
    } else {
      state.shown = term.key;
    }
  }
  toggle(true, false);
  focusTerm(term);
  keepSoon();
}

// Ends a tab's shell and closes the tab. The last one closed closes the panel, and the next opening
// starts a new one.
function closeTerm(term) {
  if (term.id !== null) {
    invoke("term_close", { id: term.id }).catch(() => {});
  }
  term.view.remove();
  state.terms = state.terms.filter((one) => one !== term);
  if (state.beside === term.key) {
    state.beside = null;
  }
  if (state.shown === term.key) {
    state.shown = state.beside ?? state.terms.at(-1)?.key ?? null;
    if (state.beside === state.shown) {
      state.beside = null;
    }
  }
  if (state.focused === term.key) {
    state.focused = state.shown;
  }
  if (!state.terms.length) {
    toggle(false);
    const next = addTerm();
    state.shown = state.focused = next.key;
  }
  drawViews();
  keepSoon();
}

// Keeps the terminal's place now: whether the panel is open, and each shell's folder and last lines,
// with the tab shown and the one beside it.
function keepPlace() {
  window.clearTimeout(state.keeping);
  const terms = state.terms.map((term) => ({
    at: term.at,
    lines: term.screen ? term.view.innerText.replace(/\s+$/, "").split("\n").slice(-PLACE_LINES) : (term.restore ?? []),
  }));
  const index = (key) => {
    const at = state.terms.findIndex((term) => term.key === key);
    return at < 0 ? null : at;
  };
  localStorage.setItem(PLACE, JSON.stringify({ open: !parts.panel.hidden, terms, shown: index(state.shown), beside: index(state.beside) }));
}

function keepSoon() {
  window.clearTimeout(state.keeping);
  state.keeping = window.setTimeout(keepPlace, PLACE_REST);
}

// Opens the panel or closes it. An open panel takes the keys where `focus` is set, and closing it
// hands them back to whatever held them before it opened.
function toggle(open = parts.panel.hidden, focus = true) {
  const was = !parts.panel.hidden;
  parts.panel.hidden = !open;
  keepSoon();
  if (open) {
    const holder = document.activeElement;
    if (!was) {
      state.before = holder && holder !== document.body && holder !== parts.keys ? holder : null;
    }
    drawViews();
    if (focus) {
      parts.keys.focus();
    }
  } else if (state.before?.isConnected) {
    state.before.focus();
  } else {
    parts.keys.blur();
  }
}

function onKey(event) {
  if (event.isComposing || (event.ctrlKey && !event.shiftKey && event.code === "Backquote")) {
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
  const text = keyText(event, focusedTerm()?.screen?.modes.appCursor);
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
    send(focusedTerm()?.screen?.modes.paste ? `\x1b[200~${text}\x1b[201~` : text);
  }
}

function onPaste(event) {
  event.preventDefault();
  pasteText(event.clipboardData.getData("text/plain"));
}

export function toggleTerminal(open) {
  toggle(open);
}

// Terminal, New Terminal: another shell in a tab of its own, shown where the keys were.
export function newTerminal() {
  const term = addTerm();
  showTerm(term);
  return term;
}

// Terminal, Split Terminal: a second shell shown beside the one shown, a new one where every other
// is shown already; pressed again, the panel shows one shell.
export function splitTerminal() {
  if (state.beside !== null) {
    state.beside = null;
    state.focused = state.shown;
    drawViews();
    keepSoon();
    return;
  }
  toggle(true, false);
  const other = state.terms.find((term) => term.key !== state.shown) ?? addTerm();
  state.beside = other.key;
  focusTerm(other);
  keepSoon();
}

export const terminalSplit = () => state.beside !== null;

// Gives the keys to the shell whose view is `view`, as a press in it does.
export function focusTerminalView(view) {
  const term = state.terms.find((one) => one.view === view);
  if (term && !parts.panel.hidden) {
    focusTerm(term);
  }
}

// Terminal, Kill Terminal: ends the shell the keys go to and closes its tab.
export function killTerminal() {
  const term = focusedTerm();
  if (term) {
    closeTerm(term);
  }
}

export function clearTerminal() {
  if (focusedTerm()?.id !== null) {
    send("\x0c");
  }
}

// Opens the panel with the shell the keys go to in `folder`, a path under the tree's top folder.
export function terminalAt(folder) {
  toggle(true);
  const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
  const where = folder ? `${top}/${folder}` : top;
  send(`cd -- '${where.replace(/'/g, "'\\''")}'\r`);
}

// Opens the panel and runs `line` in the shell the keys go to, as though typed there.
export function runInTerminal(line) {
  toggle(true);
  send(`${line}\r`);
}

// The terminal's menu: copy what is chosen in it, paste, clear the screen, a new shell, split, or
// close the panel.
function terminalItems() {
  const text = chosen();
  return [
    { label: "Copy", keys: "Ctrl+Shift+C", disabled: !text, run: () => copyText(text) },
    { label: "Paste", keys: "Ctrl+Shift+V", run: async () => pasteText(await clipText()) },
    "-",
    { label: "Clear", keys: "Ctrl+L", run: () => send("\x0c") },
    { label: "New Terminal", keys: "Ctrl+Shift+`", run: () => newTerminal() },
    { label: "Split Terminal", checked: state.beside !== null, run: () => splitTerminal() },
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
    ["strip", "term-strip"],
    ["views", "term-views"],
    ["keys", "term-keys"],
    ["grip", "term-grip"],
  ]) {
    parts[name] = document.getElementById(id);
  }
  const height = localStorage.getItem(HEIGHT);
  if (height) {
    parts.panel.style.height = height;
  }
  await listen("term-out", ({ payload }) => {
    const term = state.terms.find((one) => one.id === payload.id);
    if (term?.screen) {
      heard(term, payload.text);
      keepSoon();
    } else if (!term) {
      early.set(payload.id, [...(early.get(payload.id) ?? []), payload.text].slice(-64));
    }
  });
  await listen("term-exit", ({ payload }) => {
    const term = state.terms.find((one) => one.id === payload.id);
    if (!term) {
      return;
    }
    term.id = null;
    term.waiting = "";
    term.exited = true;
    term.screen?.write(`\r\n[exited ${payload.code ?? ""}]\r\n`);
    drawStrip();
  });
  window.addEventListener(
    "keydown",
    (event) => {
      if (event.ctrlKey && !event.shiftKey && event.code === "Backquote") {
        event.preventDefault();
        toggle();
      }
    },
    true,
  );
  parts.keys.addEventListener("keydown", onKey);
  parts.keys.addEventListener("paste", onPaste);
  // Text an input method composes reaches the hidden field without a key the terminal sends.
  parts.keys.addEventListener("input", (event) => {
    if (!event.isComposing && parts.keys.value) {
      send(parts.keys.value);
      parts.keys.value = "";
    }
  });
  parts.keys.addEventListener("focus", () => focusedTerm()?.screen?.setFocus(true));
  parts.keys.addEventListener("blur", () => state.terms.forEach((term) => term.screen?.setFocus(false)));
  menuOn(parts.panel, terminalItems);
  parts.grip.addEventListener("pointerdown", grip);
  new ResizeObserver(() => requestAnimationFrame(fitShown)).observe(parts.views);
  window.addEventListener("beforeunload", keepPlace);
  // The place kept from before the window started: each shell's lines and folder, the tabs shown, and
  // the panel open as it was. A place kept with one shell, before there were tabs, is that shell's.
  let kept = null;
  try {
    kept = JSON.parse(localStorage.getItem(PLACE) ?? "null");
  } catch {
    kept = null;
  }
  const terms = Array.isArray(kept?.terms) && kept.terms.length ? kept.terms : [{ at: kept?.at, lines: kept?.lines }];
  terms.forEach((one) => addTerm(one ?? {}));
  const keyAt = (at) => (Number.isInteger(at) && state.terms[at] ? state.terms[at].key : null);
  state.shown = keyAt(kept?.shown) ?? state.terms[0].key;
  state.beside = keyAt(kept?.beside);
  if (state.beside === state.shown) {
    state.beside = null;
  }
  state.focused = state.shown;
  drawViews();
  if (kept?.open) {
    toggle(true, false);
  }
}
