// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { Rows, joinSpans, wrapBreaks } from "../folding.js";
import { COMMENT_LINES, escapeHtml, HINT_KEYS, LINE, PAD, SHOWN, WRAP_KEY, WRAP_LEAST, WRAPS_KEPT } from "../view.js";

export function unit() {
  return this.s.indent.tabs ? "\t" : " ".repeat(this.s.indent.size);
}

// The visual column of a col of a line's text: its tabs reaching each stop, and, given the line,
// the hints drawn before it.
export function vcolOf(text, col, line = -1) {
  const size = this.s.indent.size;
  // each col before the line's first tab is a column of its own
  const tab = text.indexOf("\t");
  const plain = Math.min(col, text.length, tab < 0 ? Infinity : tab);
  let v = plain;
  for (let index = plain; index < col && index < text.length; index += 1) {
    v += text[index] === "\t" ? size - (v % size) : 1;
  }
  return line < 0 ? v : v + this.hintShift(line, col);
}

// The hints a line's server gives that the types on show, in the order of their cols.
export function hintsOn(line) {
  const all = this.s?.hints?.byLine.get(line);
  if (!all) {
    return null;
  }
  const shown = all.filter((hint) => (hint.kind === 2 ? this.hintKinds.parameter : this.hintKinds.type));
  return shown.length ? shown : null;
}

// How many columns the hints before a col of a line take. A hint at the col itself stands before
// it where it belongs to the text after it, as a parameter's name does, and after the text before
// it, as a type does.
export function hintShift(line, col) {
  const hints = this.hintsOn(line);
  let width = 0;
  for (const hint of hints ?? []) {
    if (hint.col < col || (hint.col === col && hint.kind === 2)) {
      width += hint.width;
    }
  }
  return width;
}

export function setHints(type, on) {
  this.hintKinds[type] = on;
  localStorage.setItem(HINT_KEYS[type], String(on));
  this.schedule();
}

export function hintHtml(hint) {
  return `<span class="ed-hint${hint.kind === 2 ? " parameter" : ""}" style="width:${hint.width * this.cw}px">${escapeHtml(hint.label)}</span>`;
}

export function vcol(p) {
  return this.vcolOf(this.doc.line(p.line), p.col);
}

// The col whose left edge is nearest a visual column, or with `round` off the col it falls in;
// given the line, a column a hint covers falls to the col the hint stands at.
export function colAtV(text, v, round = true, line = -1) {
  const size = this.s.indent.size;
  const hints = line < 0 ? null : this.hintsOn(line);
  let next = 0;
  let x = 0;
  for (let index = 0; index < text.length; index += 1) {
    while (hints && next < hints.length && hints[next].col <= index) {
      if (v < x + hints[next].width) {
        return index;
      }
      x += hints[next].width;
      next += 1;
    }
    const width = text[index] === "\t" ? size - (x % size) : 1;
    if (round ? x + width / 2 > v : x + width > v) {
      return index;
    }
    x += width;
  }
  return text.length;
}

export function rows() {
  const s = this.s;
  const columns = this.wrapOn ? this.wrapColumns() : 0;
  const key = `${s.doc.id}:${s.foldings}:${s.doc.count}:${this.commentsHidden}:${columns}`;
  if (key !== this.rowsKey) {
    const folds = s.folded.size ? this.foldSpans() : [];
    this.rowsCache = new Rows(s.doc.count, this.commentsHidden ? joinSpans(folds, this.commentSpans()) : folds, columns ? (line) => this.breaksOf(line, columns) : null);
    this.rowsKey = key;
  }
  return this.rowsCache;
}

// Word wrap: each line longer than the view is wide is shown as rows, each ending after the last
// space that fits in it. The columns a row holds: as many as stand between the gutter and the
// view's right edge, less two, and never fewer than WRAP_LEAST.
export function wrapColumns() {
  return Math.max(WRAP_LEAST, Math.floor((this.viewSize().width - PAD) / this.cw) - 2);
}

// The cols a line's rows after its first start at, kept by the line's text while the columns and the
// tab's width stay as they are.
export function breaksOf(line, columns) {
  const size = this.s.indent.size;
  if (this.wraps?.columns !== columns || this.wraps.size !== size || this.wraps.byText.size > WRAPS_KEPT) {
    this.wraps = { columns, size, byText: new Map() };
  }
  const text = this.s.doc.line(line);
  if (text.length <= columns && !text.includes("\t")) {
    return null;
  }
  let breaks = this.wraps.byText.get(text);
  if (breaks === undefined) {
    breaks = wrapBreaks(text, columns, size);
    this.wraps.byText.set(text, breaks);
  }
  return breaks;
}

// Turns word wrap on or off, and keeps the choice.
export function setWrap(on) {
  this.wrapOn = on;
  localStorage.setItem(WRAP_KEY, String(on));
  this.rowsKey = "";
  for (const layer of [this.textLayer, this.underLayer, this.gutterLayer]) {
    layer.clear();
  }
  if (this.s) {
    this.size();
    this.reveal();
  }
  this.schedule();
}

// The markup of one row: its line's, and where the line is wrapped, the part of it the row holds.
export function rowCell(row) {
  const rows = this.rows();
  const line = rows.lineOf(row);
  const start = rows.startOf(row);
  const end = rows.endOf(row);
  return start === 0 && end < 0 ? this.rowHtml(line) : this.partCell(line, start, end);
}

// The runs of lines that hold a comment alone, by the language's own marks: a line that
// starts with its line comment's mark, and every line of a block comment that opens a line and
// closes at the end of one.
export function commentSpans() {
  const s = this.s;
  const comments = s.language?.comments ?? {};
  if (s.doc.count > COMMENT_LINES || (!comments.line && !comments.block)) {
    return [];
  }
  const [open, close] = comments.block ?? [null, null];
  const spans = [];
  let block = null;
  for (let line = 0; line < s.doc.count; line += 1) {
    const text = s.doc.line(line).trim();
    let alone = false;
    if (block !== null) {
      if (text.includes(close)) {
        alone = text.endsWith(close);
        if (alone) {
          spans.push([block, line]);
        }
        block = null;
      }
      continue;
    }
    if (comments.line && text.startsWith(comments.line)) {
      alone = true;
    } else if (open && text.startsWith(open)) {
      const rest = text.slice(open.length);
      if (!rest.includes(close)) {
        block = line;
        continue;
      }
      alone = rest.endsWith(close);
    }
    if (alone) {
      const last = spans.at(-1);
      if (last && last[1] === line - 1) {
        last[1] = line;
      } else {
        spans.push([line, line]);
      }
    }
  }
  // A text of comments alone keeps its last line in sight.
  const last = spans.at(-1);
  if (last && spans[0][0] === 0 && spans.length === 1 && last[1] === s.doc.count - 1) {
    last[1] -= 1;
  }
  return spans;
}

export function widthOf(lines) {
  let widest = 0;
  for (const text of lines) {
    if (text.length > widest || (text.includes("\t") && text.length * this.s.indent.size > widest)) {
      widest = Math.max(widest, this.vcolOf(text, Math.min(text.length, SHOWN)));
    }
  }
  return widest;
}

// The widest line, which sizes the space across. Reading every line costs a pass over the text.
// An edit widens it by the lines the cursors are on, and the full pass waits for a pause.
export function widest() {
  const doc = this.doc;
  if (this.widestOf !== doc) {
    this.widestOf = doc;
    this.widestCache = this.widthOf(doc.lines);
    this.widestAt = doc.id;
  } else if (this.widestAt !== doc.id) {
    this.widestAt = doc.id;
    this.widestCache = Math.max(this.widestCache, this.widthOf(this.s.selections.map((sel) => doc.line(sel.head.line))));
    window.clearTimeout(this.widestWait);
    this.widestWait = window.setTimeout(() => {
      this.later("widest", () => {
        if (this.s?.doc === doc) {
          // a file stored as pages keeps the widest any of its windows kept, so the space never narrows under the view
          const width = this.widthOf(doc.lines);
          this.widestCache = this.s.window?.paged ? Math.max(this.widestCache, width) : width;
          this.schedule();
        }
      });
    }, 700);
  }
  return this.widestCache;
}

// Lines read into a session from its file. Lines above the window push everything down, and the
// view scrolls by as many rows: what is on screen stays put.
export function grown(session, added, lines) {
  if (session !== this.s) {
    return;
  }
  this.widestCache = Math.max(this.widestCache, this.widthOf(lines));
  this.rowsKey = "";
  this.size();
  if (added) {
    // The rows on screen take new numbers, and the origin moves with them: each stays where it
    // was drawn, and the next frame has nothing to draw again.
    this.originRow += added;
    this.textLayer.shift(added);
    this.underLayer.shift(added);
    this.gutterLayer.shift(added);
  }
  this.keepLine();
  if (this.find.shown) {
    window.clearTimeout(this.findWait);
    this.findWait = window.setTimeout(() => this.find.refresh(), 400);
  }
  this.schedule();
}

// Lines of a paged file's window let go above it or below it: the rows on screen keep their
// places, as when lines are read in.
export function shrunk(session, count, above) {
  if (session !== this.s) {
    return;
  }
  this.rowsKey = "";
  this.size();
  if (above) {
    this.originRow -= count;
    this.textLayer.shift(-count);
    this.underLayer.shift(-count);
    this.gutterLayer.shift(-count);
  }
  this.keepLine();
  this.schedule();
}

// A paged file's window put elsewhere in the file: the view shows it afresh. Where `holdLine` read the line the view
// stands on, as a window put where a moving view went, the view stays on that line and the rows drawn stay as they
// are, each drawn again from the window's lines as the next frame reaches it, and none cleared before then: a view gone
// on past the window keeps its last frame until the window comes to it.
export function replaced(session) {
  if (session !== this.s) {
    return;
  }
  // the markup and the breaks kept for the lines the window kept go with them
  this.rowsKept = null;
  this.wraps = null;
  const kept = this.anchorLine;
  this.anchorLine = undefined;
  // the space across as wide as the widest line any window of the file has kept, so it neither widens nor narrows
  // as the window moves, a window of cut lines measured by their whole lengths
  const lengths = session.window?.lengths;
  const width = lengths ? Math.min(SHOWN, lengths.reduce((most, length) => Math.max(most, length), 0)) : this.widthOf(session.doc.lines);
  this.widestCache = Math.max(this.widestCache ?? 0, width);
  if (kept !== undefined) {
    session.top = kept - session.base;
    this.rowsKey = "";
    this.widestOf = session.doc;
    this.widestAt = session.doc.id;
    this.drawn = null;
    this.size();
    // the scroll stands where the view stood unless the padding above the window changed by other than the lines read
    const want = this.pad + session.top * LINE;
    if (Math.abs(want - this.scroller.scrollTop) >= 1) {
      this.place(session.top * LINE);
    }
    this.schedule();
    return;
  }
  for (const layer of [this.textLayer, this.underLayer, this.gutterLayer]) {
    layer.clear();
  }
  this.s = null;
  session.view = null;
  this.show(session);
}

// Reads the line of the file at the top of the view, before a paged file's window moves under it.
export function holdLine(session) {
  if (session === this.s && session.window?.paged) {
    this.anchorLine = session.base + this.scrollY() / LINE;
  }
}

// Puts the line `holdLine` read back at the top of the view once the window has moved, a glide
// under way carried with it, and the scroll that does it not counted as speed.
export function keepLine() {
  if (this.anchorLine === undefined || !this.s) {
    return;
  }
  const line = this.anchorLine;
  this.anchorLine = undefined;
  this.size();
  const want = this.pad + (line - this.s.base) * LINE;
  const by = want - this.scroller.scrollTop;
  if (by) {
    this.scroller.scrollTop = want;
    this.placedTop = this.scroller.scrollTop;
    this.lastTop = this.scroller.scrollTop;
    if (this.glide) {
      this.glide.to += by;
      this.glide.at += by;
    }
  }
}
