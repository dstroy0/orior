// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal's shells, each a tab: each started, written to and heard, its screen fitted to its view, and the
// terminal's place retained.

import { invoke } from "../bridge.js";
import { Screen } from "../screen.js";
import { drawStrip, focusTerm } from "../terminal.js";

export const PLACE = "orior.terminal.place";
const PLACE_LINES = 300;
const PLACE_REST = 800;
// How long a shell starting stays quiet before the text typed while it started goes to it.
const SETTLE = 300;

// The shells, each a tab: its number in the app, its shell's number once started, its screen and
// the view that shows it, the text typed while it started, the folder it last said it is in,
// and the lines retained from before the window started. `displayed` is the tab displayed, `beside` the one
// beside it where the panel is split, and `focused` the one the keys go to.
export const state = { terms: [], next: 1, displayed: null, beside: null, focused: null, before: null, retention: 0 };

// What a shell wrote before its tab knew its number, by that number. The system's terminal asks where
// the cursor is before anything else and waits for the answer, which the tab gives once it hears
// the question.
export const early = new Map();
export const parts = {};

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const termOf = (key) => state.terms.find((term) => term.key === key) ?? null;
export const focusedTerm = () => termOf(state.focused) ?? termOf(state.displayed) ?? state.terms[0] ?? null;
export const showing = (term) => term.key === state.displayed || term.key === state.beside;

// Sends text to a shell, the one the keys go to where none is named. Text typed while the shell
// starts waits until the shell has written and then been quiet for SETTLE: the system's terminal
// reads its first input as the answer to where the cursor is, and the shell takes keys once its
// prompt is up.
export function send(text, term = focusedTerm()) {
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
export function chosen() {
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
      retainSoon();
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

export const fitShown = () => state.terms.filter(showing).forEach(fit);

// The scrollbacks a page loaded before this one left, let go before this page makes any.
const fresh = invoke("scrollback_reset").catch(() => {});

// Where the lines that scroll off a screen go: a file the app holds for that screen, which the
// screen reads back from as the reader scrolls up.
function scrollback() {
  const opened = fresh.then(() => invoke("scrollback_open"));
  return {
    retain: async (lines) => invoke("scrollback_retain", { id: await opened, lines }),
    read: async (from, count) => invoke("scrollback_read", { id: await opened, from, count }),
  };
}

export function openShell(term) {
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
export function heard(term, text) {
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
export function addTerm(retained = {}) {
  const view = element("div", { className: "term-view", hidden: true });
  const term = { key: state.next++, id: null, opening: null, screen: null, waiting: "", at: typeof retained.at === "string" ? retained.at : null, restore: Array.isArray(retained.lines) ? retained.lines.map(String) : null, view, exited: false };
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

// Retains the terminal's place now: whether the panel is open, and each shell's folder and last lines,
// with the tab displayed and the one beside it.
export function retainPlace() {
  window.clearTimeout(state.retention);
  const terms = state.terms.map((term) => ({
    at: term.at,
    lines: term.screen ? term.view.innerText.replace(/\s+$/, "").split("\n").slice(-PLACE_LINES) : (term.restore ?? []),
  }));
  const index = (key) => {
    const at = state.terms.findIndex((term) => term.key === key);
    return at < 0 ? null : at;
  };
  localStorage.setItem(PLACE, JSON.stringify({ open: !parts.panel.hidden, terms, displayed: index(state.displayed), beside: index(state.beside) }));
}

export function retainSoon() {
  window.clearTimeout(state.retention);
  state.retention = window.setTimeout(retainPlace, PLACE_REST);
}
