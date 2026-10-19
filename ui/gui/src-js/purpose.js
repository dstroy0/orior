// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a file is for, before it is opened (purpose.rs). The pointer resting on a file in the
// explorer for half a second shows, beside it, what the file's first comment says and then the files
// of the tree that use it, each opening at a press. The card goes when the pointer leaves both the
// file and the card, when a key is pressed, and when the explorer scrolls.

import { invoke } from "./bridge.js";
import { openFile } from "./edit.js";
import { say } from "./statusbar.js";

// How long the pointer rests on a file before the card shows, and how long it may take to cross from
// the file into the card, in milliseconds.
const REST = 500;
const CROSS = 200;

const state = { row: null, card: null, timer: 0, leaving: 0, asked: 0 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// Takes the card away, and gives its file back the name the pointer shows over it.
export function hideCard() {
  window.clearTimeout(state.timer);
  window.clearTimeout(state.leaving);
  state.card?.remove();
  if (state.row?.dataset.title !== undefined) {
    state.row.title = state.row.dataset.title;
    delete state.row.dataset.title;
  }
  Object.assign(state, { row: null, card: null, asked: state.asked + 1 });
}

// The card shown, for the tests.
export const shownCard = () => state.card;

function place(card, row) {
  const box = row.getBoundingClientRect();
  const width = card.offsetWidth;
  const left = box.right + width + 8 <= window.innerWidth ? box.right + 8 : Math.max(8, box.left - width - 8);
  card.style.left = `${left}px`;
  card.style.top = `${Math.max(8, Math.min(box.top, window.innerHeight - card.offsetHeight - 8))}px`;
}

// Shows the card of the file the row `row` stands for.
export async function showCard(row) {
  const path = row.dataset.key;
  hideCard();
  state.row = row;
  row.dataset.title = row.title;
  row.removeAttribute("title");
  const asked = state.asked;
  const said = element("p", { className: "file-card-said", textContent: "" });
  const users = element("div", { className: "file-card-users" }, element("p", { className: "file-card-wait", textContent: "Finding what uses it…" }));
  const card = element("div", { className: "file-card", role: "tooltip" }, element("p", { className: "file-card-path", textContent: path }), said, users);
  card.addEventListener("pointerenter", () => window.clearTimeout(state.leaving));
  card.addEventListener("pointerleave", () => leave());
  document.body.append(card);
  state.card = card;
  place(card, row);
  const comment = await invoke("file_said", { path }).catch(() => "");
  if (asked !== state.asked) {
    return;
  }
  said.textContent = comment || "No comment at its head says what it is for.";
  said.classList.toggle("none", !comment);
  place(card, row);
  const found = await invoke("file_users", { path }).catch((error) => ({ users: [], all: 0, error: String(error) }));
  if (asked !== state.asked) {
    return;
  }
  const list = element("ul", {}, ...found.users.map((user) => {
    const open = element("button", { type: "button", className: "file-card-user", textContent: user, title: `Open ${user}` });
    open.addEventListener("click", () => {
      hideCard();
      openFile(user).catch((error) => say(String(error), { failed: true }));
    });
    return element("li", {}, open);
  }));
  const head = found.error ? found.error : found.all === 0 ? "No file of the tree uses it." : `Used by ${found.all} file${found.all === 1 ? "" : "s"}${found.all > found.users.length ? `, the first ${found.users.length} of them` : ""}:`;
  users.replaceChildren(element("p", { className: "file-card-head", textContent: head }), ...(found.users.length ? [list] : []));
  place(card, row);
}

function leave() {
  window.clearTimeout(state.timer);
  window.clearTimeout(state.leaving);
  state.leaving = window.setTimeout(() => hideCard(), CROSS);
}

export function startPurpose() {
  const list = document.getElementById("files");
  list.addEventListener("pointerover", (event) => {
    const row = event.target.closest?.(".node:not(.dir)");
    if (!row || !row.dataset.key || row.dataset.key.startsWith("@")) {
      return;
    }
    window.clearTimeout(state.leaving);
    if (row === state.row && state.card) {
      return;
    }
    window.clearTimeout(state.timer);
    state.timer = window.setTimeout(() => showCard(row), REST);
  });
  list.addEventListener("pointerout", (event) => {
    const from = event.target.closest?.(".node");
    const into = event.relatedTarget;
    if (into && ((from && from.contains(into)) || state.card?.contains(into))) {
      return;
    }
    leave();
  });
  list.addEventListener("scroll", () => hideCard(), { passive: true });
  document.addEventListener("keydown", () => state.card && hideCard(), true);
}
