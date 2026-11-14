// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal: a panel under both views with shells in it, opened and closed from the Terminal menu
// or by Ctrl+`. Each shell is a tab of the panel's strip, named by the folder it is in; New
// Terminal opens another beside the rest, and Split shows two side by side. A shell starts the first
// time its tab shows, in the tree's top folder, and closing the panel leaves every shell running. A
// shell that ends says so, and the next key starts another in its tab; a tab's × ends its shell and
// closes it.
//
// Keys go to the shell last pressed in, as an xterm sends them. Ctrl+C copies where text is chosen
// in the panel and interrupts where none is; Ctrl+Shift+C copies and Ctrl+Shift+V pastes. Every
// other key the panel takes stays out of the rest of the app while the panel holds the keys. The
// panel's edge drags to make it taller or shorter, and its size is retained between visits.
//
// Its place is retained as well: whether the panel is open, each shell's folder, which a shell says as
// OSC 7 and Git's bash is set to say before each prompt, and its last PLACE_LINES lines, the tab displayed
// and the one beside it, retained a moment after a shell writes and as the window closes. The window
// started again shows those lines and opens the panel as it was, each shell in its folder.

import { invoke, listen } from "./bridge.js";
import { clipText, copyText, menuOn } from "./menu.js";
import { keyText } from "./terminal/keys.js";
import { PLACE, addTerm, chosen, early, element, fitShown, focusedTerm, heard, retainPlace, retainSoon, openShell, parts, send, showing, state } from "./terminal/shells.js";

// How tall the panel may be dragged is the stylesheet's to say, in its min-height and max-height.
const HEIGHT = "orior.terminal.height";

// A tab's name: the last folder of the place its shell is in, after its number.
function nameOf(term, at) {
  const place = term.at ? decodeURIComponent(term.at.replace(/^file:\/\/[^/]*/, "")).replace(/[\\/]+$/, "") : "";
  return `${at + 1} ${place.split(/[\\/]/).pop() || "shell"}`;
}

// Draws the strip: a tab for each shell, then New Terminal and Split.
export function drawStrip() {
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

// Shows the displayed tab and the one beside it, each started, and the keys with the one they go to.
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

export function focusTerm(term) {
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
      state.displayed = term.key;
    }
  }
  toggle(true, false);
  focusTerm(term);
  retainSoon();
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
  if (state.displayed === term.key) {
    state.displayed = state.beside ?? state.terms.at(-1)?.key ?? null;
    if (state.beside === state.displayed) {
      state.beside = null;
    }
  }
  if (state.focused === term.key) {
    state.focused = state.displayed;
  }
  if (!state.terms.length) {
    toggle(false);
    const next = addTerm();
    state.displayed = state.focused = next.key;
  }
  drawViews();
  retainSoon();
}


// Opens the panel or closes it. An open panel takes the keys where `focus` is set, and closing it
// passes them back to whatever retained them before it opened.
function toggle(open = parts.panel.hidden, focus = true) {
  const was = !parts.panel.hidden;
  parts.panel.hidden = !open;
  retainSoon();
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

// Terminal, New Terminal: another shell in a tab of its own, displayed where the keys were.
export function newTerminal() {
  const term = addTerm();
  showTerm(term);
  return term;
}

// Terminal, Split Terminal: a second shell displayed beside the one displayed, a new one where every other
// is displayed already; pressed again, the panel shows one shell.
export function splitTerminal() {
  if (state.beside !== null) {
    state.beside = null;
    state.focused = state.displayed;
    drawViews();
    retainSoon();
    return;
  }
  toggle(true, false);
  const other = state.terms.find((term) => term.key !== state.displayed) ?? addTerm();
  state.beside = other.key;
  focusTerm(other);
  retainSoon();
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
    { label: "Copy", keys: "Ctrl+Shift+C", command: "terminal-copy", args: [], disabled: !text, run: () => copyText(text) },
    { label: "Paste", keys: "Ctrl+Shift+V", command: "terminal-paste", args: [], run: () => terminalCommands["terminal-paste"]([]) },
    "-",
    { label: "Clear", keys: "Ctrl+L", command: "clear", args: [], run: () => send("\x0c") },
    { label: "New Terminal", keys: "Ctrl+Shift+`", command: "new", args: [], run: () => newTerminal() },
    { label: "Split Terminal", command: "split", args: [], checked: state.beside !== null, run: () => splitTerminal() },
    { label: "Close", keys: "Ctrl+`", command: "terminal-close", args: [], run: () => toggle(false) },
  ];
}

// The terminal's menu as commands, by the names commands.json gives those the Terminal menu has not:
// the text chosen on the screen copied, the clipboard's or the words given pasted into the shell the
// keys go to, and the panel closed.
export const terminalCommands = {
  "terminal-copy": () => chosen() && copyText(chosen()),
  "terminal-paste": async (words) => pasteText(words.length ? words.join(" ") : await clipText()),
  "terminal-close": () => toggle(false),
};

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
      retainSoon();
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
  window.addEventListener("beforeunload", retainPlace);
  // The place retained from before the window started: each shell's lines and folder, the tabs displayed, and
  // the panel open as it was. A place retained with one shell, before there were tabs, is that shell's.
  let retained = null;
  try {
    retained = JSON.parse(localStorage.getItem(PLACE) ?? "null");
  } catch {
    retained = null;
  }
  const terms = Array.isArray(retained?.terms) && retained.terms.length ? retained.terms : [{ at: retained?.at, lines: retained?.lines }];
  terms.forEach((one) => addTerm(one ?? {}));
  const keyAt = (at) => (Number.isInteger(at) && state.terms[at] ? state.terms[at].key : null);
  state.displayed = keyAt(retained?.displayed) ?? state.terms[0].key;
  state.beside = keyAt(retained?.beside);
  if (state.beside === state.displayed) {
    state.beside = null;
  }
  state.focused = state.displayed;
  drawViews();
  if (retained?.open) {
    toggle(true, false);
  }
}
