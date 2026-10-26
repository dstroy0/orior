// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { pos } from "../document.js";
import { BRACKETS_KEY, COLUMN_KEY, COMMENTS_KEY, CONTINUATION_KEY, DECLARES, DOC_MARGIN_KEY, FLICK_KEY, HINT_KEYS, LINE, MARKS_KEY, OPERATOR_KEY, PAD, PAGED_PAD, PAST_KEY, RTL, runsRightToLeft, SHOWN, smoothScrolling, STICKY_KEY, WRAP_KEY } from "../view.js";

// Sizes the scrolling space to every row and the widest line.
export function size() {
  const rows = this.rows();
  // A file held as pages is scrolled as the whole file: the lines above the window and below it
  // stand as padding, and the scroll and the scrollbar stay where they are as the window slides.
  const s = this.s;
  if (s?.window?.paged) {
    this.pad = Math.min(s.base, PAGED_PAD) * LINE;
    this.below = Math.min(s.window.after, PAGED_PAD) * LINE;
  } else {
    this.pad = 0;
    this.below = 0;
  }
  const view = this.viewSize();
  this.spaceHeight = this.pad + rows.size * LINE + LINE + this.below;
  const lineWidth = PAD + (this.widest() + 4) * this.cw;
  this.spaceWidth = this.wrapOn ? view.width : Math.max(view.width, lineWidth);
  this.space.style.height = `${this.spaceHeight}px`;
  // the space as wide as the widest line and no wider, the stylesheet keeping it as wide as the view:
  // a view narrowing takes the space with it in the same frame, so lines shorter than it never scroll
  this.space.style.width = this.wrapOn ? "" : `${lineWidth}px`;
  this.sheet.style.width = `${view.width}px`;
  this.sheet.style.height = `${view.height}px`;
}

// The scroller's inner size, as the resize observer last gave it. A size read from the page
// after a change to it lays the whole page out first, and a frame that reads one after each
// change it makes lays it out again and again.
export function viewSize() {
  this.viewKept ??= { width: this.scroller.clientWidth, height: this.scroller.clientHeight };
  return this.viewKept;
}

// How far the view is scrolled from the first row read, in pixels.
export function scrollY() {
  return this.scroller.scrollTop - this.pad;
}

// How many rows the scrolling space stands for: those read, and while a file is still being
// read, those above and below them.
export function allRows() {
  return (this.pad + this.below) / LINE + this.rows().size;
}

// Moves the sheet and the gutter's numbers to where the view is scrolled, from the origin: to
// `top` and `left`, the scroller's own, where they were read before the page was changed.
export function follow(top = this.scroller.scrollTop, left = this.scroller.scrollLeft) {
  const down = this.originRow * LINE - (top - this.pad);
  this.layers.style.transform = `translate(${-left}px, ${down}px)`;
  this.gutterRows.style.transform = `translateY(${down}px)`;
  const code = this.s?.margin ?? null;
  const width = this.marginShown ? code : null;
  const at = width ? PAD + width * this.cw - left : null;
  const says = width ? `${this.s.marginBy ?? "The formatter"} keeps lines to ${width} columns` : "";
  if (says !== this.marginSays.textContent) {
    this.marginSays.textContent = says;
  }
  if (at !== this.marginAt) {
    this.marginAt = at;
    this.margin.hidden = at === null;
    if (at !== null) {
      this.margin.style.transform = `translateX(${at}px)`;
    }
  }
  const doc = this.docMargin && this.docMargin !== code ? PAD + this.docMargin * this.cw - left : null;
  if (doc !== this.docLineAt) {
    this.docLineAt = doc;
    this.docLine.hidden = doc === null;
    if (doc !== null) {
      this.docLine.style.transform = `translateX(${doc}px)`;
    }
  }
}

export function setContinuation(continuation) {
  this.continuation = continuation;
  localStorage.setItem(CONTINUATION_KEY, JSON.stringify(continuation));
}

export function setOperatorNext(on) {
  this.operatorNext = on;
  localStorage.setItem(OPERATOR_KEY, String(on));
}

export function setDocMargin(columns) {
  this.docMargin = columns || null;
  if (columns) {
    localStorage.setItem(DOC_MARGIN_KEY, String(columns));
  } else {
    localStorage.removeItem(DOC_MARGIN_KEY);
  }
  this.docLineAt = undefined;
  this.follow();
}

// The continuation indent of a new line inside the bracket at `at` of `line`: as the reader set it
// for the place the bracket stands, a declaration's parameters, a call's arguments or any other
// bracket, and one step of the indent where none is set.
export function continuationAt(line, at) {
  const before = this.doc.line(line).slice(0, at).trimEnd();
  const place = DECLARES.test(before) ? "declaration" : /[\w$)\]]$/.test(before) ? "call" : "expression";
  const columns = Number(this.continuation?.[place]);
  if (!Number.isFinite(columns) || columns <= 0) {
    return this.unit();
  }
  return this.s.indent.tabs ? "\t".repeat(Math.max(1, Math.round(columns / this.s.indent.size))) : " ".repeat(columns);
}

export function xOf(p) {
  if (this.wrapOn) {
    const rows = this.rows();
    const row = rows.rowAt(p.line, p.col);
    const start = rows.startOf(row);
    if (start > 0 || rows.endOf(row) >= 0) {
      const text = this.doc.line(p.line);
      return PAD + (this.vcolOf(text, p.col, p.line) - this.vcolOf(text, start, p.line)) * this.cw;
    }
  }
  if (RTL.test(this.doc.line(p.line))) {
    const x = this.measuredX(p.line, p.col);
    if (x !== null) {
      return x;
    }
  }
  return PAD + this.vcolOf(this.doc.line(p.line), p.col, p.line) * this.cw;
}

// The node the line `line` is drawn in, where its row is on screen.
export function rowNode(line) {
  return this.textLayer.held.get(this.rows().rowOf(line))?.node ?? null;
}

// The text node, and the place in it, that holds the col `col` of the line drawn in `node`, the
// hints' and a fold's marks left out.
export function textAt(node, col) {
  const walker = document.createTreeWalker(node, NodeFilter.SHOW_TEXT, { acceptNode: (text) => (text.parentElement?.closest(".ed-hint, .ed-folded") ? NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT) });
  let left = col;
  for (let text = walker.nextNode(); text; text = walker.nextNode()) {
    if (left < text.length) {
      return { text, offset: left };
    }
    left -= text.length;
  }
  return null;
}

// The box the page draws the character at `col` of the line drawn in `node` in, or null.
export function charBox(node, col) {
  const at = this.textAt(node, col);
  if (!at) {
    return null;
  }
  const range = document.createRange();
  range.setStart(at.text, at.offset);
  range.setEnd(at.text, at.offset + 1);
  const box = range.getBoundingClientRect();
  return box.width || box.height ? box : null;
}

// Where the caret before col `col` of the line `line` stands as the page draws the line, in the
// space's own pixels: at the edge of the character at `col` that comes first in the order it is
// read, its right edge where it runs right to left, and past the last character at the line's end.
// Null where the line is not drawn.
export function measuredX(line, col) {
  const node = this.rowNode(line);
  const text = this.doc.line(line);
  if (!node || !text.length) {
    return null;
  }
  const at = Math.min(col, text.length);
  const char = at < text.length ? at : at - 1;
  const box = this.charBox(node, char);
  if (!box) {
    return null;
  }
  const right = runsRightToLeft(text)[char];
  const edge = at < text.length ? (right ? box.right : box.left) : right ? box.left : box.right;
  return edge - this.space.getBoundingClientRect().left;
}

// The col a press at the page's `x` lands on in the line `line` as the page draws it: before the
// character nearest it where the press is on that character's first half in the order it is read,
// and after it where on its second. Null where the line is not drawn.
export function measuredCol(line, x) {
  const node = this.rowNode(line);
  const text = this.doc.line(line);
  if (!node) {
    return null;
  }
  const right = runsRightToLeft(text);
  let best = null;
  for (let at = 0; at < Math.min(text.length, SHOWN); at += 1) {
    const box = this.charBox(node, at);
    if (!box) {
      continue;
    }
    const off = x < box.left ? box.left - x : x > box.right ? x - box.right : 0;
    if (!best || off < best.off) {
      best = { at, box, off };
    }
  }
  if (!best) {
    return null;
  }
  const middle = (best.box.left + best.box.right) / 2;
  return right[best.at] === x > middle ? best.at : best.at + 1;
}

// The place under a pointer.
export function posAt(event) {
  const rect = this.space.getBoundingClientRect();
  const rows = this.rows();
  const row = Math.floor((event.clientY - rect.top - this.pad) / LINE);
  if (row < 0) {
    return pos(rows.lineOf(0), 0);
  }
  if (row >= rows.size) {
    const last = rows.lineOf(rows.size - 1);
    return pos(last, this.doc.line(last).length);
  }
  const line = rows.lineOf(row);
  const start = rows.startOf(row);
  const end = rows.endOf(row);
  if (start > 0 || end >= 0) {
    // A row of a wrapped line: the col under the pointer within the part of the line it holds.
    const text = this.doc.line(line);
    const col = this.colAtV(text, (event.clientX - rect.left - PAD) / this.cw + this.vcolOf(text, start, line), true, line);
    return pos(line, Math.max(start, end >= 0 ? Math.min(col, end - 1) : col));
  }
  if (RTL.test(this.doc.line(line))) {
    const col = this.measuredCol(line, event.clientX);
    if (col !== null) {
      return pos(line, col);
    }
  }
  return pos(line, this.colAtV(this.doc.line(line), (event.clientX - rect.left - PAD) / this.cw, true, line));
}

// The editor's settings as the page's storage gives them, read as the editor is made and again as
// the settings in force change.
export function readSettings() {
  this.stickyOn = localStorage.getItem(STICKY_KEY) !== "false";
  this.wrapOn = localStorage.getItem(WRAP_KEY) === "true";
  this.bracketsOn = localStorage.getItem(BRACKETS_KEY) !== "false";
  this.marksOn = localStorage.getItem(MARKS_KEY) === "true";
  this.commentsHidden = localStorage.getItem(COMMENTS_KEY) === "true";
  this.pastEnds = localStorage.getItem(PAST_KEY) === "true";
  if (this.commentsHidden) {
    this.host.dataset.comments = "hidden";
  } else {
    delete this.host.dataset.comments;
  }
  this.columnMode = localStorage.getItem(COLUMN_KEY) === "true";
  this.smoothOn = smoothScrolling();
  this.hintKinds = { type: localStorage.getItem(HINT_KEYS.type) !== "false", parameter: localStorage.getItem(HINT_KEYS.parameter) !== "false" };
  try {
    this.continuation = JSON.parse(localStorage.getItem(CONTINUATION_KEY) ?? "{}") ?? {};
  } catch {
    this.continuation = {};
  }
  this.operatorNext = localStorage.getItem(OPERATOR_KEY) === "true";
  this.docMargin = Number(localStorage.getItem(DOC_MARGIN_KEY)) || null;
  this.flickOn = localStorage.getItem(FLICK_KEY) !== "false";
}

// Reads the settings again, as the files in force changed them, and draws with them.
export function takeSettings() {
  this.readSettings();
  this.textLayer.clear();
  if (this.s) {
    this.size();
  }
  this.schedule();
}

// The colors the session shown is drawn in where its kind of file has a theme of its own, as
// `themeOf` gives them, set on the editor in place of those set before; the window's where it has
// none.
export function drawTheme() {
  const colors = this.themeOf?.(this.s) ?? null;
  if (!colors && !this.themed?.length) {
    return;
  }
  for (const name of this.themed ?? []) {
    this.host.style.removeProperty(name);
  }
  this.themed = Object.keys(colors ?? {});
  for (const [name, value] of Object.entries(colors ?? {})) {
    this.host.style.setProperty(name, value);
  }
  this.host.toggleAttribute("data-themed", Boolean(colors));
  this.refreshColors();
}
