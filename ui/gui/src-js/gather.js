// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The task's code in one place. Gather Lines (Ctrl+Alt+G) adds to the task the lines selected, or,
// with none selected, the declaration the cursor stands in down to the next at its depth; Gathered
// Lines (Ctrl+Alt+Shift+G) shows the task's lines side by side over the editor, each in an editor of
// its own of its file's text: a file a tab holds shows as the tab has it, changes not saved and all,
// and takes edits as the tab does, and one no tab holds shows as it was last written. A piece's name
// opens its file at its lines. Each piece keeps the text of its first line, and where lines come or
// go above it, it is found again by that text. Tasks are named, one is the task gathered into, and
// each keeps its pieces, with the tree, from one run to the next.

import { editing, onShown, openAt, viewEditor, viewSession } from "./edit.js";
import { symbolsOf } from "./outline.js";
import { say } from "./statusbar.js";

// The most lines a declaration gathers, how many lines a piece shows before it scrolls, and how far
// from its line a piece's first line is looked for.
const MOST_LINES = 120;
const MOST_SHOWN = 18;
const LOOK = 2000;

const state = { board: null, editors: [], sessions: [], drawn: 0 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const key = () => `orior.gather.${document.getElementById("tree-path").textContent}`;

// The tree's tasks: the one gathered into, and each by its name with its pieces.
export function tasks() {
  try {
    const kept = JSON.parse(localStorage.getItem(key()) ?? "null");
    if (kept?.tasks && typeof kept.tasks === "object") {
      return kept;
    }
  } catch {
    // A kept value that does not read starts the tasks anew.
  }
  return { current: "Task 1", tasks: { "Task 1": [] } };
}

function keep(kept) {
  localStorage.setItem(key(), JSON.stringify(kept));
}

// The lines the editor's selection covers, or with none the declaration the cursor stands in down
// to the line before the next declaration at its depth, its blank lines at the end left off; a line
// alone where none holds the cursor.
export function linesHere(editor) {
  const s = editor.s;
  const doc = s.doc;
  const sel = s.selections[s.primary];
  const [a, b] = [sel.anchor, sel.head].sort((x, y) => x.line - y.line || x.col - y.col);
  if (a.line !== b.line || a.col !== b.col) {
    return [a.line, b.col === 0 && b.line > a.line ? b.line - 1 : b.line];
  }
  const line = sel.head.line;
  const symbols = symbolsOf(s.language?.id, doc).filter((one) => one.kind !== "region" && one.kind !== "heading");
  const holder = [...symbols].reverse().find((one) => one.line <= line);
  if (!holder) {
    return [line, line];
  }
  const next = symbols.find((one) => one.line > holder.line && one.depth <= holder.depth);
  let end = Math.min((next ? next.line : doc.count) - 1, holder.line + MOST_LINES - 1);
  while (end > holder.line && !doc.line(end).trim()) {
    end -= 1;
  }
  return line <= end ? [holder.line, end] : [line, line];
}

// Gather Lines: the lines here, added to the task gathered into.
export function gatherLines() {
  const { editor, active } = editing();
  if (!editor?.s || !active) {
    say("Gather Lines gathers lines of a file open in the editor", { failed: true });
    return;
  }
  const [from, to] = linesHere(editor).map((line) => line + editor.s.base);
  const kept = tasks();
  const pieces = (kept.tasks[kept.current] ??= []);
  if (!pieces.some((piece) => piece.path === active && piece.from === from && piece.to === to)) {
    pieces.push({ path: active, from, to, anchor: editor.s.doc.line(from - editor.s.base) });
  }
  keep(kept);
  say(`Lines ${from + 1} to ${to + 1} of ${active} gathered into ${kept.current}`);
  if (state.board) {
    drawBoard();
  }
}

// The line a piece's first line stands on now in `doc`: where it was, else the nearest line within
// LOOK of it that reads as it did.
export function placeOf(piece, doc, base = 0) {
  const at = piece.from - base;
  if (doc.line(at) === piece.anchor || piece.anchor === undefined) {
    return piece.from;
  }
  for (let step = 1; step <= LOOK; step += 1) {
    for (const line of [at - step, at + step]) {
      if (line >= 0 && line < doc.count && doc.line(line) === piece.anchor) {
        return line + base;
      }
    }
  }
  return piece.from;
}

function letGo() {
  state.sessions.forEach((session) => session.of && session.drop());
  Object.assign(state, { editors: [], sessions: [] });
}

// Closes Gathered Lines.
export function closeBoard() {
  letGo();
  state.board?.remove();
  state.board = null;
}

async function drawPiece(piece, at, holder, drawn) {
  const session = await viewSession(piece.path).catch((error) => {
    holder.replaceChildren(element("p", { className: "gather-gone", textContent: String(error) }));
    return null;
  });
  if (!session || !state.board || drawn !== state.drawn) {
    if (session?.of) {
      session.drop();
    }
    return;
  }
  state.sessions.push(session);
  const from = placeOf(piece, session.doc, session.base);
  if (from !== piece.from) {
    const kept = tasks();
    const found = kept.tasks[kept.current]?.[at];
    if (found) {
      found.to += from - found.from;
      found.from = from;
      keep(kept);
    }
    piece.to += from - piece.from;
    piece.from = from;
  }
  const editor = viewEditor(holder);
  state.editors.push(editor);
  holder.style.height = `calc(${Math.min(piece.to - piece.from + 1, MOST_SHOWN)} * var(--code-line) + 10px)`;
  editor.show(session);
  editor.goTo(piece.from - session.base, 0);
  editor.place((piece.from - session.base) * editor.lineHeight);
}

// Draws the task gathered into, a piece of it for each run of lines.
export function drawBoard() {
  if (!state.board) {
    return;
  }
  letGo();
  state.drawn += 1;
  const drawn = state.drawn;
  const kept = tasks();
  const pieces = kept.tasks[kept.current] ?? [];
  const select = element("select", { className: "gather-task", title: "The task gathered into" }, ...Object.keys(kept.tasks).map((name) => element("option", { value: name, textContent: name, selected: name === kept.current })));
  select.addEventListener("change", () => {
    const now = tasks();
    now.current = select.value;
    keep(now);
    drawBoard();
  });
  const name = element("input", { className: "gather-name", type: "text", placeholder: "New task's name", ariaLabel: "New task's name" });
  const add = element("button", { type: "button", className: "gather-button", textContent: "New Task" });
  const made = () => {
    const given = name.value.trim();
    if (!given) {
      name.focus();
      return;
    }
    const now = tasks();
    now.tasks[given] ??= [];
    now.current = given;
    keep(now);
    drawBoard();
  };
  add.addEventListener("click", made);
  name.addEventListener("keydown", (event) => event.key === "Enter" && made());
  const drop = element("button", { type: "button", className: "gather-button", textContent: "Remove Task", disabled: Object.keys(kept.tasks).length < 2 });
  drop.addEventListener("click", () => {
    const now = tasks();
    delete now.tasks[now.current];
    now.current = Object.keys(now.tasks)[0];
    keep(now);
    drawBoard();
  });
  const close = element("button", { type: "button", className: "gather-button", textContent: "×", title: "Close (Escape)" });
  close.addEventListener("click", () => closeBoard());
  const bar = element("div", { className: "gather-bar" }, select, name, add, drop, element("span", { className: "gather-count", textContent: `${pieces.length} piece${pieces.length === 1 ? "" : "s"}` }), close);
  const list = element("div", { className: "gather-pieces" });
  if (!pieces.length) {
    list.append(element("p", { className: "gather-empty", textContent: "No lines gathered yet. Gather Lines (Ctrl+Alt+G) adds the lines selected, or the declaration the cursor stands in, to the task." }));
  }
  pieces.forEach((piece, at) => {
    const go = element("button", { type: "button", className: "gather-go", textContent: `${piece.path}:${piece.from + 1}–${piece.to + 1}`, title: `Open ${piece.path} at line ${piece.from + 1}` });
    go.addEventListener("click", () => {
      closeBoard();
      openAt(piece.path, piece.from, 0);
    });
    const remove = element("button", { type: "button", className: "gather-button", textContent: "×", title: "Take these lines out of the task" });
    remove.addEventListener("click", () => {
      const now = tasks();
      now.tasks[now.current]?.splice(at, 1);
      keep(now);
      drawBoard();
    });
    const holder = element("div", { className: "gather-editor" });
    list.append(element("article", { className: "gather-piece" }, element("header", {}, go, remove), holder));
    drawPiece(piece, at, holder, drawn);
  });
  state.board.replaceChildren(bar, list);
}

// Gathered Lines: the task's lines shown over the editor, or put away where they show.
export function toggleBoard() {
  if (state.board) {
    closeBoard();
    return;
  }
  const board = element("section", { className: "gather", ariaLabel: "Gathered lines", tabIndex: -1 });
  board.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !event.defaultPrevented) {
      event.preventDefault();
      closeBoard();
      editing().editor?.focus();
    }
  });
  document.querySelector("#mode-edit .desk").append(board);
  state.board = board;
  drawBoard();
  board.focus();
}

// What the board shows, for the tests.
export const boardShown = () => (state.board ? { board: state.board, editors: state.editors } : null);

export function startGather() {
  onShown(() => state.board && drawBoard());
}
