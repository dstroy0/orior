// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { isWordChar, pos, same, wordAt } from "../document.js";
import { selectionPath } from "../shape.js";
import { status, write } from "../../status.js";
import { enter, leave } from "../../trace.js";
import { BREAK_STRIP, DRAWN_MOST, DRAWN_PAGED, empty, endOfSel, escapeHtml, historyChars, LINE, ORIGIN_ROWS, PAD, RTL, SELECTION_ROUND, startOf } from "../view.js";

// The most rows a paged file's view draws ahead of where it heads as it scrolls.
const LEAD_PAGED = 16;

// A frame drawn, it and each of its stages traced where the window is joined to a run.
export function paint() {
  const call = enter("paint");
  try {
    this.painted();
  } finally {
    leave(call);
  }
}

export function painted() {
  const began = performance.now();
  if (this.pending) {
    cancelAnimationFrame(this.pending);
    this.pending = 0;
  }
  const s = this.s;
  if (!s) {
    this.textLayer.clear();
    this.underLayer.clear();
    this.gutterLayer.clear();
    this.over.innerHTML = "";
    this.overHtml = "";
    this.status.replaceChildren();
    this.minimap.clear();
    this.sticky.hidden = true;
    this.stickyKey = "";
    return;
  }
  const doc = s.doc;
  const rows = this.rows();
  const cw = this.cw;
  const base = s.base;
  const total = this.linesInFile();
  // The gutter holds, left to right, the strip a breakpoint is set in, Line History's column where
  // it shows, the line numbers, and the fold arrows and change marks.
  // Where the view stands, read before this frame changes the page.
  // Where the view stands, as this frame's scroll event read it, or read now where none came: a read
  // after the page has changed lays it out again before the frame does.
  const known = this.scrolledNow;
  this.scrolledNow = null;
  const scrolledTop = known ? known.top : this.scroller.scrollTop;
  const scrolledLeft = known ? known.left : this.scroller.scrollLeft;
  this.leftNow = scrolledLeft;
  const historyWidth = s.history ? Math.round(historyChars() * cw) + 8 : 0;
  const gutterWidth = Math.round(String(total).length * cw + 36 + BREAK_STRIP) + historyWidth;
  if (gutterWidth !== this.gutterWidth) {
    this.gutterWidth = gutterWidth;
    this.gutter.style.width = `${gutterWidth}px`;
  }
  this.size();
  const top = scrolledTop - this.pad;
  const height = this.viewSize().height;
  // The rows past each edge of the screen, more of them the way the view heads the faster it
  // goes. The page's own scroll never shows a row before a frame draws it.
  // A file held as pages whose view reaches past the lines its window holds keeps its last frame where it was drawn
  // until the window comes to the view, so no row stands blank and no part of the view is torn from the rest; the
  // minimap goes on with the view.
  if (this.heldFrame === s && this.pastWindow(scrolledTop)) {
    this.minimap.paint(scrolledTop);
    this.mapTop = scrolledTop;
    return;
  }
  this.heldFrame = s;
  // A paged file's window holds the view and a margin about it, so it draws fewer rows ahead than a text read whole;
  // and none past its edges where its view has moved farther than its height since the frame before, as a drag of the
  // scrollbar moves it, the rows about a view that leaps never shown.
  const file = s.base + top / LINE;
  const leaps = s.window?.paged && this.drawnFileTop !== undefined && Math.abs(file - this.drawnFileTop) * LINE > height;
  this.drawnFileTop = file;
  const edge = leaps ? 0 : 2;
  const lead = edge + (leaps ? 0 : Math.min(s.window?.paged ? LEAD_PAGED : 80, Math.ceil((status.scroll.speed * 48) / LINE)));
  const first = Math.max(0, Math.floor(top / LINE) - (status.scroll.heading < 0 ? lead : edge));
  const last = Math.min(rows.size - 1, Math.ceil((top + height) / LINE) + (status.scroll.heading > 0 ? lead : edge));
  if (rows.size) {
    const shownCall = enter("shown");
    this.onShown(s, rows.lineOf(first), rows.lineOf(last));
    leave(shownCall);
  }
  // The origin moves only once the rows drawn have left the two spans of ORIGIN_ROWS below it. A
  // new origin moves every row, each layer placing its rows again where they stand from it.
  if (first < this.originRow || first >= this.originRow + 2 * ORIGIN_ROWS) {
    this.originRow = Math.floor(first / ORIGIN_ROWS) * ORIGIN_ROWS;
  }
  const spaceWidth = this.spaceWidth;
  const focused = this.hasFocus();
  const sels = s.selections;
  const primary = this.primary();
  const caretLines = new Set(sels.filter(empty).map((sel) => sel.head.line));
  const headLines = new Set(sels.map((sel) => sel.head.line));
  // The session's breakpoints, each file line mapped to whether it is bound to code, and the file
  // line a debugged program is stopped on, as the editor's owner gives them.
  const breaks = this.breakpointsOf?.(s) ?? null;
  const marked = this.bookmarksOf?.(s) ?? null;
  // Each test's last result by its line, and the lines the last run with coverage ran and missed.
  const tests = this.testsOf?.(s) ?? null;
  const covered = this.coverageOf?.(s) ?? null;
  const paused = this.pausedOf?.(s) ?? null;
  // The marks plugins lay, each line's from every plugin.
  const plugged = this.pluginMarksOf?.(s) ?? null;

  const text = new Map();
  const under = new Map();
  const over = [];
  const gutter = new Map();
  const band = (row, html) => under.set(row, (under.get(row) ?? "") + html);
  // A span's marks behind the text on each row it reaches on screen.
  const bandSpan = (name, line, from, to, past = false) => {
    for (const [row, html] of this.spans(name, line, from, to, past)) {
      if (row >= first && row <= last) {
        band(row, html);
      }
    }
  };
  // The row on screen that shows a place, or -1.
  const visibleAt = (p) => {
    const row = rows.rowAt(p.line, p.col);
    return row >= first && row <= last && rows.lineOf(row) === p.line ? row : -1;
  };
  const visible = (line) => {
    const row = rows.rowOf(line);
    return row >= first && row <= last && rows.lineOf(row) === line ? row : -1;
  };

  let occurrence = null;
  if (empty(primary)) {
    occurrence = wordAt(doc, primary.head)?.text ?? null;
  } else if (startOf(primary).line === endOfSel(primary).line) {
    const picked = doc.slice(startOf(primary), endOfSel(primary));
    occurrence = /^[\p{L}\p{N}_$]+$/u.test(picked) ? picked : null;
  }
  if (occurrence && occurrence.length > 120) {
    occurrence = null;
  }
  const matches = this.find.shown ? this.find.between(rows.lineOf(first), rows.lineOf(last)) : [];
  const current = this.find.shown ? this.find.current() : null;

  const rowsCall = enter("rows");
  for (let row = first; row <= last; row += 1) {
    const line = rows.lineOf(row);
    text.set(row, ["ed-row", this.rowCell(row), (node) => this.rowPatched(node, row)]);
    if (caretLines.has(line)) {
      band(row, this.box("ed-current", 0, this.yOf(row), spaceWidth));
    }
    if (paused === base + line) {
      band(row, this.box("ed-paused", 0, this.yOf(row), spaceWidth));
    }
    // A wrapped line's rows after its first carry no number and no guides; its marks are drawn once.
    if (rows.startOf(row) > 0) {
      gutter.set(row, ["ed-num", ""]);
      continue;
    }
    const levels = this.guides(line);
    for (let step = 0; step < levels; step += 1) {
      band(row, this.box("ed-guide", PAD + step * s.indent.size * cw, this.yOf(row), 1));
    }
    if (occurrence) {
      const lineText = doc.line(line);
      for (let at = lineText.indexOf(occurrence); at >= 0; at = lineText.indexOf(occurrence, at + 1)) {
        if (!isWordChar(lineText[at - 1]) && !isWordChar(lineText[at + occurrence.length])) {
          bandSpan("ed-same", line, at, at + occurrence.length);
        }
      }
    }
    const number = `<span class="ed-num-text">${base + line + 1}</span>`;
    const foldable = s.folded.has(line) || s.opens(line);
    const folded = foldable && s.folded.has(line);
    const mark = foldable ? `<span class="ed-fold${folded ? " shut" : ""}" data-fold="${line}">${folded ? "▸" : "▾"}</span>` : "";
    const stop = breaks?.get(base + line);
    const dot = stop === undefined ? "" : `<span class="ed-break${stop.bound === false ? " unbound" : ""}${stop.log ? " log" : stop.condition || stop.hits ? " cond" : ""}"></span>`;
    const here = paused === base + line ? '<span class="ed-pc"></span>' : "";
    const ribbon = marked?.has(base + line) ? '<span class="ed-bookmark"></span>' : "";
    const history = s.history ? this.historyMark(line) : "";
    const test = tests?.get(base + line);
    const testMark = test === undefined ? "" : `<span class="ed-test ${test}" data-test="${base + line}" title="Run or debug the test">▶</span>`;
    const cover = covered?.ran.has(base + line) ? '<span class="ed-cover ran"></span>' : covered?.missed.has(base + line) ? '<span class="ed-cover missed"></span>' : "";
    const laid = plugged?.get(base + line);
    const plugMark = laid ? `<span class="ed-plugin ${laid[0].kind}" title="${escapeHtml(laid.map((one) => one.title + (one.text ? `: ${one.text}` : "")).join("\n"))}"></span>` : "";
    if (laid?.some((one) => one.text)) {
      over.push(`<div class="ed-plugin-text ${laid[0].kind}" style="left:${this.xOf({ line, col: doc.line(line).length }) + 2 * this.cw}px;top:${this.yOf(rows.lastRowOf(line))}px;height:${LINE}px">${escapeHtml(laid.map((one) => one.text).filter(Boolean).join("  "))}</div>`);
    }
    gutter.set(row, [headLines.has(line) ? "ed-num on" : "ed-num", dot + here + ribbon + history + testMark + plugMark + number + mark + cover + this.changeMark(line)]);
  }
  leave(rowsCall);
  for (const match of matches) {
    for (let line = match.from.line; line <= match.to.line; line += 1) {
      const from = line === match.from.line ? match.from.col : 0;
      const to = line === match.to.line ? match.to.col : doc.line(line).length;
      const on = current && same(current.from, match.from);
      bandSpan(on ? "ed-match on" : "ed-match", line, from, to, line !== match.to.line);
    }
  }
  // A language server's diagnostics, each underlined in the color of its severity: 1 an error, 2 a
  // warning, 3 a note and 4 a hint. One that spans nothing marks the character it stands at.
  for (const diag of s.diagnostics ?? []) {
    for (let line = diag.from.line - s.base; line <= diag.to.line - s.base; line += 1) {
      if (line < 0 || line >= doc.count || visible(line) < 0) {
        continue;
      }
      const from = line === diag.from.line - s.base ? diag.from.col : 0;
      let to = line === diag.to.line - s.base ? diag.to.col : doc.line(line).length;
      if (to <= from) {
        to = from + 1;
      }
      bandSpan(`ed-diag s${Math.min(4, Math.max(1, diag.severity))}`, line, from, to);
    }
  }
  if (s.snippet) {
    for (const stop of s.snippet.stops) {
      if (stop.from.line === stop.to.line) {
        bandSpan("ed-stop", stop.from.line, stop.from.col, stop.to.col, same(stop.from, stop.to));
      }
    }
  }
  const rowTop = (row) => this.yOf(row);
  const textCall = enter("text");
  // The rows drawn: those in sight, and those drawn out from them since in the session and from the
  // origin they were drawn in, at most DRAWN_MOST of them, as many past the view on either side as
  // there are room for, and none past the text's end. A row drawn before an edit or a scroll across
  // is drawn again from the text as it stands as it comes into sight.
  const drawnFor = this.drawnFor();
  const was = this.drawn?.for === drawnFor && this.drawn.from <= last + 1 && this.drawn.to >= first - 1 ? this.drawn : null;
  const half = Math.floor(((s.window?.paged ? DRAWN_PAGED : DRAWN_MOST) - (last - first + 1)) / 2);
  this.drawn = { for: drawnFor, from: Math.max(Math.min(first, was?.from ?? first), first - half), to: Math.min(Math.max(last, was?.to ?? last), last + half, rows.size - 1) };
  const drawn = this.drawn;
  this.textLayer.draw(text, rowTop, (row) => row >= drawn.from && row <= drawn.to);
  leave(textCall);
  this.drawOutward();
  // Each selection covers its characters on each row it reaches, and a line's end inside it as
  // one blank character, the rows' spans drawn as one shape; on a line that holds text running
  // right to left, each run of its characters as the page draws them is a shape of its own.
  const shapes = [];
  // Read only for a line that runs right to left: a read here lays out the rows just written.
  let spaceLeft = null;
  for (const sel of sels) {
    if (empty(sel)) {
      continue;
    }
    const start = startOf(sel);
    const end = endOfSel(sel);
    const spans = [];
    for (let row = Math.max(first, rows.rowOf(start.line)); row <= Math.min(last, rows.rowAt(end.line, end.col)); row += 1) {
      const line = rows.lineOf(row);
      if (line < start.line || line > end.line) {
        continue;
      }
      const text = doc.line(line);
      const segStart = rows.startOf(row);
      const segEnd = rows.endOf(row);
      const lineFrom = line === start.line ? start.col : 0;
      const lineTo = line === end.line ? end.col : text.length;
      if (segStart > 0 || segEnd >= 0) {
        // A row of a wrapped line: the part of the selection the row holds.
        const from = Math.max(lineFrom, segStart);
        const to = segEnd < 0 ? lineTo : Math.min(lineTo, segEnd);
        if (to < from || (segEnd >= 0 && from >= segEnd)) {
          continue;
        }
        const shift = this.vcolOf(text, segStart, line);
        const ends = line < end.line && segEnd < 0;
        spans.push([row - this.originRow, PAD + (this.vcolOf(text, from, line) - shift) * cw, PAD + (this.vcolOf(text, to, line) - shift) * cw + (ends ? cw : 0)]);
        continue;
      }
      const from = lineFrom;
      const ends = line < end.line;
      const to = lineTo;
      const node = to > from && RTL.test(text) ? this.rowNode(line) : null;
      const head = node ? this.textAt(node, from) : null;
      const past = node ? (this.textAt(node, to) ?? this.textAt(node, to - 1)) : null;
      if (head && past) {
        spaceLeft ??= this.space.getBoundingClientRect().left;
        const range = document.createRange();
        range.setStart(head.text, head.offset);
        range.setEnd(past.text, to < text.length ? past.offset : past.offset + 1);
        for (const box of range.getClientRects()) {
          if (box.width > 0) {
            shapes.push(selectionPath([[row - this.originRow, box.left - spaceLeft, box.right - spaceLeft]], LINE, SELECTION_ROUND));
          }
        }
        if (ends) {
          const x = this.xOf(pos(line, text.length));
          shapes.push(selectionPath([[row - this.originRow, x, x + cw]], LINE, SELECTION_ROUND));
        }
        continue;
      }
      spans.push([row - this.originRow, PAD + this.vcolOf(text, from, line) * cw, PAD + this.vcolOf(text, to, line) * cw + (ends ? cw : 0)]);
    }
    shapes.push(selectionPath(spans, LINE, SELECTION_ROUND));
  }
  const picked = shapes.join("") ? `<path class="${focused ? "ed-sel" : "ed-sel idle"}" d="${shapes.join("")}"/>` : "";
  if (picked !== this.pickedHtml) {
    this.picked.innerHTML = picked;
    this.pickedHtml = picked;
  }
  for (const sel of sels) {
    const row = visibleAt(sel.head);
    if (row >= 0) {
      const block = this.vim?.block();
      over.push(this.box(sel === primary ? "ed-caret main" : "ed-caret", this.xOf(sel.head) + this.pastEnd(sel) * this.cw - (block ? 0 : 1), this.yOf(row), block ? this.cw : 2));
    }
  }
  const pairCall = enter("pair");
  const pair = this.bracketPair();
  leave(pairCall);
  if (pair) {
    for (const at of pair) {
      for (const [row, html] of this.spans("ed-pair", at.line, at.col, at.col + 1)) {
        if (row >= first && row <= last) {
          over.push(html);
        }
      }
    }
  }

  const layersCall = enter("layers");
  this.underLayer.draw(new Map([...under].map(([row, html]) => [row, ["ed-band", html]])));
  this.gutterLayer.draw(gutter, rowTop);
  leave(layersCall);
  const marks = over.join("");
  if (this.overHtml !== marks) {
    this.over.innerHTML = marks;
    this.overHtml = marks;
  }
  this.follow(scrolledTop, scrolledLeft);
  const headRow = rows.rowAt(primary.head.line, primary.head.col);
  this.input.style.left = `${this.xOf(primary.head)}px`;
  this.input.style.top = `${this.yOf(headRow)}px`;
  // Off the rows on screen: the map and the status line wait while input is active, except that a
  // map whose text has scrolled draws in the same frame as the text, its slider never behind it.
  const scrolled = this.mapTop !== scrolledTop;
  if (!status.input.active || scrolled) {
    const mapCall = enter("minimap");
    this.minimap.paint(scrolledTop);
    leave(mapCall);
    this.mapTop = scrolledTop;
  }
  if (!status.input.active) {
    const statusCall = enter("status");
    this.drawStatus();
    leave(statusCall);
  }
  const stickyCall = enter("sticky");
  this.drawSticky(rows, top, scrolledLeft);
  leave(stickyCall);
  this.suggest.place();
  write("frame", { spent: performance.now() - began });
}
