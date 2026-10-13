// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Code seen without going to it. Peek Definition (Alt+F12, Ctrl+Shift+I), Peek Usages (Ctrl+Alt+F7)
// and Peek Callers show, over the editor under the cursor's line, the lines the language server
// finds, in an editor of their own: a file a tab holds shows as the tab has it, changes not saved and
// all, and takes edits as the tab does; one no tab holds shows as it was read, and takes none. No tab
// opens. Where there is more than one place, they stand in a list beside the lines, and the arrows go
// from one to the next. Enter, or Open, opens the place shown in a tab that is a preview until it is
// edited, and Escape, or the cursor's file changing, closes the peek.

import { editing, onShown, openAt, viewEditor, viewSession } from "./edit.js";
import { placesHere } from "./intel.js";
import { say } from "./statusbar.js";

const TITLES = { definition: "Definition of", usages: "Usages of", callers: "Callers of" };

const state = { panel: null, editor: null, sessions: new Map(), places: [], shown: -1, title: "" };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const where = (place) => `${place.path}:${place.line + 1}`;

// Closes the peek, and lets go of the views of the files it showed.
export function closePeek() {
  state.panel?.remove();
  state.sessions.forEach((session) => session.of && session.drop());
  Object.assign(state, { panel: null, editor: null, sessions: new Map(), places: [], shown: -1 });
}

// What the peek shows, for the tests: its title, its places, the one shown and the editor showing it.
export const peekShown = () => (state.panel ? { title: state.title, places: state.places, shown: state.shown, editor: state.editor, panel: state.panel } : null);

// Shows the place at `index` in the peek's editor, the place's line near its top.
export async function showPlace(index) {
  const place = state.places[index];
  if (!place || !state.editor) {
    return;
  }
  state.shown = index;
  state.panel.querySelectorAll(".code-peek-place").forEach((one, at) => one.setAttribute("aria-selected", String(at === index)));
  state.panel.querySelector(".code-peek-where").textContent = where(place);
  let session = state.sessions.get(place.path);
  if (!session) {
    try {
      session = await viewSession(place.path);
    } catch (error) {
      say(String(error), { failed: true });
      return;
    }
    if (!state.editor) {
      return;
    }
    state.sessions.set(place.path, session);
  }
  if (state.shown !== index) {
    return;
  }
  state.editor.show(session);
  const line = Math.max(0, Math.min(place.line - session.base, session.doc.count - 1));
  state.editor.goTo(line, place.col);
  state.editor.place(Math.max(0, line - 2) * state.editor.lineHeight);
}

// Opens the place shown in a preview tab, and closes the peek.
export async function openShown() {
  const place = state.places[state.shown];
  if (!place) {
    return;
  }
  closePeek();
  await openAt(place.path, place.line, place.col, { preview: true });
}

function step(by) {
  if (state.places.length > 1) {
    const next = (state.shown + by + state.places.length) % state.places.length;
    showPlace(next);
    state.panel.querySelectorAll(".code-peek-place")[next]?.focus();
  }
}

// Shows the places `places` under the title `title` in a peek over the editor under the line the
// cursor stands on, the first of them shown.
export function peekPlaces(title, places) {
  closePeek();
  const main = editing().editor;
  if (!main || !places.length) {
    return;
  }
  const button = (text, label, run) => {
    const made = element("button", { type: "button", className: "peek-button", textContent: text, title: label });
    made.addEventListener("click", run);
    return made;
  };
  const list = places.length > 1 ? element("ol", { className: "code-peek-list", role: "listbox", ariaLabel: title }) : null;
  places.forEach((place, at) => {
    const row = element("button", { type: "button", className: "code-peek-place", role: "option", title: where(place) }, element("span", { className: "code-peek-file", textContent: `${place.path.split("/").pop()}:${place.line + 1}` }), element("span", { className: "code-peek-text", textContent: (place.caller ? `${place.caller}: ` : "") + (place.text ?? "").trim() }));
    row.addEventListener("click", () => showPlace(at));
    row.addEventListener("dblclick", () => openShown());
    list?.append(element("li", {}, row));
  });
  const view = element("div", { className: "code-peek-editor" });
  const panel = element(
    "div",
    { className: "peek code-peek", role: "dialog", ariaLabel: title },
    element("div", { className: "peek-head" }, element("span", { className: "peek-title", textContent: title }), element("span", { className: "code-peek-where" }), button("Open", "Open in a preview tab (Enter)", () => openShown()), button("×", "Close (Escape)", () => {
      closePeek();
      main.focus();
    })),
    element("div", { className: "code-peek-body" }, list, view),
  );
  panel.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      closePeek();
      main.focus();
    } else if (event.target.closest(".code-peek-place") && (event.key === "ArrowDown" || event.key === "ArrowUp")) {
      event.preventDefault();
      step(event.key === "ArrowDown" ? 1 : -1);
    } else if (event.target.closest(".code-peek-place") && event.key === "Enter") {
      event.preventDefault();
      openShown();
    }
  });
  const host = document.getElementById("editor");
  host.append(panel);
  Object.assign(state, { panel, places, title, editor: viewEditor(view), shown: -1 });
  const box = host.getBoundingClientRect();
  const line = main.head()?.line ?? 0;
  const below = main.rectOf({ line, col: 0 }).bottom - box.top;
  const above = main.rectOf({ line, col: 0 }).top - box.top;
  panel.style.left = `${main.gutter.offsetWidth}px`;
  panel.style.top = `${below + panel.offsetHeight <= host.clientHeight ? below : Math.max(0, above - panel.offsetHeight)}px`;
  showPlace(0);
  (panel.querySelector(".code-peek-place") ?? panel.querySelector(".peek-button")).focus();
}

// Peek Definition, Peek Usages and Peek Callers: the places the language server finds for the symbol
// at the cursor, as `kind` asks, shown in a peek.
export async function peekHere(kind) {
  let found;
  try {
    found = await placesHere(kind);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (!found) {
    return;
  }
  if (!found.places.length) {
    say(`No ${kind === "definition" ? "definition" : kind} of ${found.name} found.`);
    return;
  }
  peekPlaces(`${TITLES[kind]} ${found.name}`, found.places);
}

export function startPeek() {
  onShown(() => state.panel && closePeek());
  // Escape in the editor closes the peek over it, where the editor has no use of its own for it.
  document.getElementById("editor").addEventListener("keydown", (event) => {
    if (event.key === "Escape" && state.panel && !event.defaultPrevented) {
      closePeek();
    }
  });
}
