// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cmp, isWordChar, least, most, pos, same } from "../document.js";
import { status, write } from "../../status.js";
import { caret, COLUMN_KEY, empty, endOfSel, INPUT_REST, LINE, startOf } from "../view.js";

// Showing a session.

export function show(session) {
  if (this.s) {
    this.s.top = this.scrollY() / LINE;
    this.s.left = this.scroller.scrollLeft;
    this.s.view = null;
  }
  this.s = session;
  this.hover.hide();
  this.suggest.close();
  this.host.dataset.empty = String(!session);
  this.rowsKey = "";
  this.widestOf = null;
  this.statusSaid = "";
  if (session) {
    session.view = this;
    this.text.style.tabSize = String(session.indent.size);
    this.size();
    this.place(session.top * LINE);
    this.scroller.scrollLeft = session.left;
    this.find.refresh();
  }
  this.drawTheme();
  this.paint();
  this.onCursor();
}

export function focus() {
  this.input.focus({ preventScroll: true });
}

export function hasFocus() {
  return document.activeElement === this.input;
}

export function primary() {
  return this.s.selections[this.s.primary];
}

export function head() {
  return this.s ? this.primary().head : null;
}

// Selections.

export function copySelections() {
  return this.s.selections.map((sel) => ({ anchor: { ...sel.anchor }, head: { ...sel.head }, goal: sel.goal, column: sel.column }));
}

// Sets the selections, in order and with any that overlap merged. `chosen` is the primary.
export function setSelections(list, chosen = list.at(-1)) {
  const doc = this.doc;
  const items = list.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: sel.goal ?? null, column: sel.column ?? false, chosen: sel === chosen }));
  items.sort((a, b) => cmp(startOf(a), startOf(b)));
  const out = [];
  for (const sel of items) {
    const prev = out.at(-1);
    const touching = prev && same(startOf(sel), endOfSel(prev)) && (empty(sel) || empty(prev));
    if (prev && (cmp(startOf(sel), endOfSel(prev)) < 0 || touching)) {
      const start = least(startOf(prev), startOf(sel));
      const end = most(endOfSel(prev), endOfSel(sel));
      const forward = cmp(prev.head, prev.anchor) >= 0;
      prev.anchor = forward ? start : end;
      prev.head = forward ? end : start;
      prev.chosen = prev.chosen || sel.chosen;
      continue;
    }
    out.push(sel);
  }
  let index = out.findIndex((sel) => sel.chosen);
  if (index < 0) {
    index = out.length - 1;
  }
  out.forEach((sel) => delete sel.chosen);
  this.s.selections = out;
  this.s.primary = index;
}

export function select(list, chosen) {
  this.setSelections(list, chosen);
  this.doc.seal();
  this.moved();
  this.reveal();
}

// After the selections move: the frame, the panels that follow the cursor, and the snippet,
// which ends once its cursor leaves it.
export function moved() {
  const snippet = this.s?.snippet;
  if (snippet) {
    const head = this.primary().head;
    const inside = this.s.selections.length === 1 && snippet.stops.some((stop) => cmp(stop.from, head) <= 0 && cmp(head, stop.to) <= 0);
    if (!inside) {
      this.s.snippet = null;
    }
  }
  this.schedule();
  this.later("cursor", () => this.onCursor());
}

// Input first. A key or a press marks the status block's input active, and until it has rested
// INPUT_REST every piece of work off the rows on screen is kept here, one of each type, and done
// once input stops.
//
// The caret holds still while input is active. A caret that blinks starts its blink again each
// time a key draws it, and that start costs the key a frame.
export function inputting() {
  if (!status.input.active) {
    write("input", { active: true });
    this.host.dataset.typing = "true";
  }
  window.clearTimeout(this.inputWait);
  this.inputWait = window.setTimeout(() => {
    write("input", { active: false });
    delete this.host.dataset.typing;
    const deferred = this.deferred;
    this.deferred = new Map();
    deferred.forEach((run) => run());
    this.paint();
  }, INPUT_REST);
}

export function later(type, run) {
  if (status.input.active) {
    this.deferred.set(type, run);
  } else {
    run();
  }
}

// Folds open where they hide the primary cursor, and the view scrolls to it.
export function reveal(center = false) {
  const s = this.s;
  if (!s) {
    return;
  }
  const head = this.primary().head;
  if (s.folded.size) {
    let opened = false;
    for (const [start, end] of [...s.folded]) {
      if (start < head.line && head.line <= this.closedEnd(start, end)) {
        s.folded.delete(start);
        opened = true;
      }
    }
    if (opened) {
      s.foldings += 1;
    }
  }
  this.size();
  const y = this.rows().rowAt(head.line, head.col) * LINE;
  const top = this.headingY();
  const height = this.scroller.clientHeight;
  if (center && (y < top || y + LINE > top + height)) {
    this.scrollToY(y - height / 2);
  } else if (y < top) {
    this.scrollToY(y);
  } else if (y + LINE > top + height) {
    this.scrollToY(y + LINE - height);
  }
  const x = this.xOf(head);
  const left = this.scroller.scrollLeft;
  const width = this.scroller.clientWidth;
  if (x - 4 * this.cw < left) {
    this.scroller.scrollLeft = Math.max(0, x - 8 * this.cw);
  } else if (x + 4 * this.cw > left + width) {
    this.scroller.scrollLeft = x + 8 * this.cw - width;
  }
  this.schedule();
}

// Selections from keys.

export function charLeft(p) {
  if (p.col > 0) {
    const line = this.doc.line(p.line);
    return pos(p.line, p.col - (p.col >= 2 && /[\uDC00-\uDFFF]/.test(line[p.col - 1]) ? 2 : 1));
  }
  const rows = this.rows();
  const row = rows.rowOf(p.line);
  if (row === 0) {
    return p;
  }
  const line = rows.lineOf(row - 1);
  return pos(line, this.doc.line(line).length);
}

export function charRight(p) {
  const text = this.doc.line(p.line);
  if (p.col < text.length) {
    return pos(p.line, p.col + (/[\uD800-\uDBFF]/.test(text[p.col]) ? 2 : 1));
  }
  const rows = this.rows();
  const row = rows.lastRowOf(p.line);
  return row + 1 >= rows.size ? p : pos(rows.lineOf(row + 1), 0);
}

export function wordLeft(p) {
  if (p.col === 0) {
    return this.charLeft(p);
  }
  const text = this.doc.line(p.line);
  let at = p.col;
  while (at > 0 && /\s/.test(text[at - 1])) {
    at -= 1;
  }
  const type = isWordChar(text[at - 1]);
  while (at > 0 && !/\s/.test(text[at - 1]) && isWordChar(text[at - 1]) === type) {
    at -= 1;
  }
  return pos(p.line, at);
}

export function wordRight(p) {
  const text = this.doc.line(p.line);
  if (p.col >= text.length) {
    return this.charRight(p);
  }
  let at = p.col;
  while (at < text.length && /\s/.test(text[at])) {
    at += 1;
  }
  const type = isWordChar(text[at]);
  while (at < text.length && !/\s/.test(text[at]) && isWordChar(text[at]) === type) {
    at += 1;
  }
  return pos(p.line, at);
}

export function vertical(sel, rowsBy) {
  const rows = this.rows();
  const here = rows.rowAt(sel.head.line, sel.head.col);
  // Where lines wrap, the goal is the column within the row, and a move goes a row at a time.
  const shift = this.vcolOf(this.doc.line(sel.head.line), rows.startOf(here));
  const goal = sel.goal ?? this.vcol(sel.head) - shift;
  const row = here + rowsBy;
  if (row < 0) {
    return { p: pos(rows.lineOf(0), 0), goal };
  }
  if (row >= rows.size) {
    const last = rows.lineOf(rows.size - 1);
    return { p: pos(last, this.doc.line(last).length), goal };
  }
  const line = rows.lineOf(row);
  const text = this.doc.line(line);
  const start = rows.startOf(row);
  const end = rows.endOf(row);
  const col = this.colAtV(text, this.vcolOf(text, start) + goal);
  return { p: pos(line, Math.max(start, end >= 0 ? Math.min(col, end - 1) : col)), goal };
}

// Moves every selection's head; with `extend` the anchors stay.
export function moveBy(step, extend) {
  const next = this.s.selections.map((sel) => {
    const found = step(sel, extend);
    const p = found.p ?? found;
    return extend ? { anchor: sel.anchor, head: p, goal: found.goal ?? null } : caret(p, found.goal ?? null);
  });
  this.suggest.close();
  this.select(next, next[this.s.primary]);
}

export function page(direction, extend) {
  const by = Math.max(1, Math.floor(this.scroller.clientHeight / LINE) - 1);
  const top = this.headingY() + direction * by * LINE;
  if (this.smoothOn) {
    this.glideTo(top + this.pad);
  } else {
    this.place(top);
  }
  this.moveBy((sel) => this.vertical(sel, direction * by), extend);
}

export function home(sel) {
  const text = this.doc.line(sel.head.line);
  const lead = text.match(/^[ \t]*/)[0].length;
  return pos(sel.head.line, sel.head.col === lead ? 0 : lead);
}

// Puts the cursor on a line of the text, from 0, and the line in the middle of the screen.
export function goTo(line, col = 0) {
  const p = this.doc.clamp(pos(line, col));
  this.select([caret(p)]);
  this.reveal(true);
}

export function setColumnMode(on) {
  this.columnMode = on;
  localStorage.setItem(COLUMN_KEY, String(on));
}
