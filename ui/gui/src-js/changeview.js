// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A change reviewed by its structure (review.rs). Review Changes by Structure shows the tree's change
// against its last commit, or a commit's against its first parent, as the declarations it touches and
// not file by file: each piece its declaration's lines, those taken out and those put in marked, one
// added or taken out whole said to be, and one moved named with where it stood. The pieces that call
// one another stand together in a row, each caller before what it calls, and the names a piece calls
// under its name go to those pieces. A piece's name opens its file at its line. A commit is named with
// who made it, and no line is named with who wrote it.

import { invoke } from "./bridge.js";
import { openAt } from "./edit.js";
import { lineChanges } from "./editor/diff.js";
import { say } from "./statusbar.js";

const state = { view: null, read: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The rows a piece shows: each line as it is, or taken out, or put in, as " ", "-" and "+" mark them.
export function rowsOf(piece) {
  const then = piece.was ? piece.was.split("\n") : [];
  const now = piece.now ? piece.now.split("\n") : [];
  if (!then.length) {
    return now.map((text) => ["+", text]);
  }
  if (!now.length) {
    return then.map((text) => ["-", text]);
  }
  const found = lineChanges(then, now);
  if (!found) {
    return [...then.map((text) => ["-", text]), ...now.map((text) => ["+", text])];
  }
  const out = [];
  let at = 0;
  for (const hunk of found.hunks) {
    for (; at < hunk.now[0]; at += 1) {
      out.push([" ", now[at]]);
    }
    for (let line = hunk.then[0]; line < hunk.then[1]; line += 1) {
      out.push(["-", then[line]]);
    }
    for (let line = hunk.now[0]; line < hunk.now[1]; line += 1) {
      out.push(["+", now[line]]);
    }
    at = hunk.now[1];
  }
  for (; at < now.length; at += 1) {
    out.push([" ", now[at]]);
  }
  return out;
}

const HOW = { changed: "changed", added: "added", removed: "taken out", moved: "moved" };

export function closeChange() {
  state.view?.remove();
  state.view = null;
}

function card(piece, at, pieces) {
  const how = piece.how === "moved" && piece.from ? `moved from ${piece.from}` : HOW[piece.how];
  const name = element("button", { type: "button", className: "cv-name", textContent: piece.name, title: `Open ${piece.path} at line ${piece.line + 1}` });
  name.addEventListener("click", () => {
    closeChange();
    openAt(piece.path, piece.line, 0);
  });
  const calls = piece.calls.length
    ? element(
        "p",
        { className: "cv-calls" },
        "calls ",
        ...piece.calls.flatMap((to, index) => {
          const go = element("button", { type: "button", className: "cv-call", textContent: pieces[to].name });
          go.addEventListener("click", () => state.view.querySelector(`[data-piece="${to}"]`)?.scrollIntoView({ block: "nearest", inline: "nearest" }));
          return index ? [", ", go] : [go];
        }),
      )
    : null;
  const lines = element("pre", { className: "cv-lines" }, ...rowsOf(piece).map(([mark, text]) => element("div", { className: `cv-line${mark === "-" ? " out" : mark === "+" ? " in" : ""}`, textContent: `${mark} ${text}` })));
  return element(
    "article",
    { className: `cv-piece ${piece.how}` },
    element("header", {}, name, element("span", { className: "cv-kind", textContent: piece.kind }), element("span", { className: "cv-how", textContent: how }), element("span", { className: "cv-path", textContent: `${piece.path}:${piece.line + 1}` })),
    calls,
    lines,
  );
}

function draw(read, commit) {
  const title = commit ? `${commit.slice(0, 8)} by ${read.by ?? "no one named"}${read.subject ? `: ${read.subject}` : ""}` : "Changes since the last commit";
  const close = element("button", { type: "button", className: "cv-button", textContent: "×", title: "Close (Escape)" });
  close.addEventListener("click", closeChange);
  const count = `${read.pieces.length} declaration${read.pieces.length === 1 ? "" : "s"} in ${read.groups.length} group${read.groups.length === 1 ? "" : "s"}`;
  const bar = element("div", { className: "cv-bar" }, element("span", { className: "cv-title", textContent: title }), element("span", { className: "cv-count", textContent: count }), close);
  const body = element("div", { className: "cv-body" });
  if (!read.pieces.length) {
    body.append(element("p", { className: "cv-empty", textContent: "The change touches no declaration of a language read here." }));
  }
  read.groups.forEach((group) => {
    const row = element("section", { className: "cv-group" });
    group.forEach((at) => {
      const made = card(read.pieces[at], at, read.pieces);
      made.dataset.piece = String(at);
      row.append(made);
    });
    body.append(row);
  });
  if (read.other.length) {
    body.append(element("p", { className: "cv-other", textContent: `Other files changed, outside any declaration read here: ${read.other.join(", ")}` }));
  }
  state.view.replaceChildren(bar, body);
}

// Review Changes by Structure: the tree's change, or commit `commit`'s, shown over the editor as the
// declarations it touches.
export async function reviewChange(commit = null) {
  let read;
  try {
    read = await invoke("review_structure", { commit });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  closeChange();
  const view = element("section", { className: "cv", ariaLabel: "The change by its declarations", tabIndex: -1 });
  view.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !event.defaultPrevented) {
      event.preventDefault();
      closeChange();
    }
  });
  document.querySelector("#mode-edit .desk").append(view);
  state.view = view;
  state.read = read;
  draw(read, commit);
  view.focus();
}

// What the view shows, for the tests.
export const changeShown = () => (state.view ? { view: state.view, read: state.read } : null);
