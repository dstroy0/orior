// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { indentOf } from "../folding.js";
import { calm, nextSlice, status } from "../../status.js";
import { authorsShown } from "../../authors.js";
import { BRACKETS_KEY, BREAK_STRIP, COMMENTS_KEY, DRAW_SLICE, DRAWN_MOST, DRAWN_PAGED, empty, escapeHtml, historyChars, LINE, marked, MARKS_KEY, PAD, PAST_KEY, REACH_STEP, ROWS_KEPT, SHOWN } from "../view.js";

// What the rows drawn were drawn in: the session and the origin. Drawn in any other, a row out of
// sight is let go.
export function drawnFor() {
  return `${this.sessionId(this.s)}|${this.originRow}`;
}

// Draws the rows out from those drawn, a slice at a time, the way nextSlice says, in each idle
// moment the view is not pressed in, as a large file is read: a scroll finds the rows it comes to
// drawn. It stops at DRAWN_MOST rows, at the ends of the text, and where what the rows are drawn
// from changes, the next frame drawing what is in sight and starting it again.
export async function drawOutward() {
  if (this.drawing) {
    return;
  }
  this.drawing = true;
  try {
    let below = true;
    for (;;) {
      await calm();
      const drawn = this.drawn;
      if (!this.s || !drawn || drawn.for !== this.drawnFor() || !this.host.isConnected) {
        return;
      }
      const rows = this.rows();
      const canAbove = drawn.from > 0;
      const canBelow = drawn.to < rows.size - 1;
      // a file held as pages draws out no more than DRAWN_PAGED rows, and none while its view moves
      const paged = Boolean(this.s.window?.paged);
      if ((!canAbove && !canBelow) || drawn.to - drawn.from + 1 >= (paged ? DRAWN_PAGED : DRAWN_MOST) || (paged && status.scroll.speed > 0)) {
        return;
      }
      const goBelow = nextSlice({ canAbove, canBelow, moving: status.scroll.speed > 0, heading: status.scroll.heading, below }) === "below";
      below = !below;
      const from = goBelow ? drawn.to + 1 : Math.max(0, drawn.from - DRAW_SLICE);
      const to = goBelow ? Math.min(rows.size - 1, drawn.to + DRAW_SLICE) : drawn.from - 1;
      const parts = new Map();
      for (let row = from; row <= to; row += 1) {
        parts.set(row, ["ed-row", this.rowCell(row), (node) => this.rowPatched(node, row)]);
      }
      this.textLayer.extend(parts, (row) => this.yOf(row));
      drawn.from = Math.min(drawn.from, from);
      drawn.to = Math.max(drawn.to, to);
    }
  } finally {
    this.drawing = false;
  }
}

export function schedule() {
  if (!this.pending) {
    this.pending = requestAnimationFrame(() => {
      this.pending = 0;
      this.paint();
    });
  }
}

// How many columns past its line's end a cursor stands: an empty selection at the end with its
// column beyond it, where the setting lets cursors past line ends or the selection is a column
// selection's.
export function pastEnd(sel) {
  if ((!this.pastEnds && !sel.column) || this.wrapOn || !empty(sel) || sel.goal === null || sel.goal === undefined) {
    return 0;
  }
  if (sel.head.col !== this.doc.line(sel.head.line).length) {
    return 0;
  }
  return Math.max(0, Math.round(sel.goal) - this.vcol(sel.head));
}

// Writes spaces out to each cursor past its line's end, for text typed or pasted where it stands.
export function fillPastEnds() {
  const edits = this.s.selections.flatMap((sel) => {
    const columns = this.pastEnd(sel);
    return columns ? [{ from: sel.head, to: sel.head, text: " ".repeat(columns) }] : [];
  });
  if (edits.length) {
    // Each cursor goes past the spaces written before it, and stays a cursor.
    this.change(edits, "type", (map) =>
      this.s.selections.map((sel) => {
        const head = map(sel.head, true);
        return { anchor: empty(sel) ? head : map(sel.anchor), head, goal: null };
      }),
    );
  }
}

// Cursors past line ends, let or not.
export function setPastEnds(on) {
  this.pastEnds = on;
  localStorage.setItem(PAST_KEY, String(on));
  this.schedule();
}

// Marks for spaces, tabs and line ends, on or off.
export function setMarks(on) {
  this.marksOn = on;
  localStorage.setItem(MARKS_KEY, String(on));
  this.textLayer.clear();
  this.schedule();
}

// Comments hidden or shown again: their text not drawn, and the lines that hold a comment alone
// out of sight, as a fold takes its lines.
export function setCommentsHidden(on) {
  this.commentsHidden = on;
  localStorage.setItem(COMMENTS_KEY, String(on));
  if (on) {
    this.host.dataset.comments = "hidden";
  } else {
    delete this.host.dataset.comments;
  }
  // A view with no file open has no rows to count again: the setting holds for the file it opens.
  if (!this.s) {
    return;
  }
  this.textLayer.clear();
  this.size();
  this.reveal();
  this.schedule();
}

// Bracket pairs colored by the depth orior's side names each at, or not: the stylesheet colors them where the host
// carries the setting.
export function setBrackets(on) {
  this.bracketsOn = on;
  localStorage.setItem(BRACKETS_KEY, String(on));
  if (on) {
    this.host.dataset.brackets = "";
  } else {
    delete this.host.dataset.brackets;
  }
}

// How many characters across a row is drawn to: as far as the view reaches, to the end of the step
// of REACH_STEP it reaches into; and as far with the view scrolled `left` pixels across.
export function reach() {
  return this.reachAt(this.leftNow ?? this.scroller.scrollLeft);
}

// How many cols of the line `text` its rows draw: where lines wrap the whole line, each row drawing its own cols, and
// else as far as the view reaches across, at most SHOWN.
export function shownOf(text) {
  return this.wrapOn ? text.length : Math.min(text.length, SHOWN, this.reach());
}

export function reachAt(left) {
  if (this.wrapOn) {
    return Infinity;
  }
  const seen = Math.ceil((left + this.viewSize().width) / this.cw);
  return (Math.floor(seen / REACH_STEP) + 1) * REACH_STEP;
}

// The pieces a line's row is drawn from, in order: runs of the line's text, `[from, to, key]`, from its col `from` to
// before `to` in the colors of the classes `key`, empty for none; and markup that stands between them, `[-1, -1,
// html]`: a hint before the text from its col on, the mark at the line's end, a fold and the text it closes at. The
// pieces of a line not folded are kept while what they are made from stays, as a wrapped line's rows read them one
// after the next.
export function rowPieces(line) {
  const s = this.s;
  const text = s.doc.line(line);
  const runs = s.highlight.runsOf(line);
  const shown = this.shownOf(text);
  const folded = s.folded.has(line);
  const made = [s, line, text, runs, s.hints?.byLine.get(line) ?? null, this.hintKinds.parameter, this.hintKinds.type, this.marksOn, line + 1 < s.doc.count, shown];
  const kept = this.piecesKept;
  if (!folded && kept?.made.every((part, at) => part === made[at])) {
    return kept.pieces;
  }
  const hints = this.hintsOn(line);
  let hinted = 0;
  const pieces = [];
  const hintsTo = (col) => {
    while (hints && hinted < hints.length && hints[hinted].col <= col) {
      pieces.push([-1, -1, this.hintHtml(hints[hinted])]);
      hinted += 1;
    }
  };
  for (let index = 0; index < runs.length; index += 1) {
    const [start, name] = runs[index];
    const end = Math.min(runs[index + 1]?.[0] ?? shown, shown);
    let from = start;
    while (from < end) {
      hintsTo(from);
      const cut = hints && hinted < hints.length && hints[hinted].col < end ? hints[hinted].col : end;
      pieces.push([from, cut, name]);
      from = cut;
    }
  }
  hintsTo(Infinity);
  if (this.marksOn && line + 1 < s.doc.count) {
    pieces.push([-1, -1, '<span class="ed-eol">¬</span>']);
  }
  if (s.folded.has(line) && s.endOf(line) >= 0) {
    const end = s.folded.get(line);
    pieces.push([-1, -1, `<span class="ed-folded" data-fold="${line}">⋯</span>${s.shows(line) ? escapeHtml(s.doc.line(end).trim()) : ""}`]);
  }
  this.piecesKept = folded ? null : { made, pieces };
  return pieces;
}

// The markup of a line's row, each run of its text with colors an element of its own. A row's markup is kept while its
// text, its colors, its hints and its marks are the ones it was made from, so that a scroll makes only the rows that
// come into view; a folded row reads the line its fold closes at, and is made each time.
export function rowHtml(line) {
  const s = this.s;
  // A text shown in the colors its codes set draws each line as they made it.
  if (s.colored) {
    return (s.colored[line] ?? "") + (s.folded.has(line) && s.endOf(line) >= 0 ? `<span class="ed-folded" data-fold="${line}">⋯</span>` : "");
  }
  const text = s.doc.line(line);
  const shown = this.shownOf(text);
  const folded = s.folded.has(line);
  const made = [text, s.highlight.runsOf(line), s.hints?.byLine.get(line) ?? null, this.marksOn, line + 1 < s.doc.count, this.hintKinds.parameter, this.hintKinds.type, shown];
  if (this.rowsKept?.session !== s || this.rowsKept.rows.size > ROWS_KEPT) {
    this.rowsKept = { session: s, rows: new Map() };
  }
  const kept = folded ? null : this.rowsKept.rows.get(line);
  if (kept && kept.made.every((part, at) => part === made[at])) {
    return kept.html;
  }
  const escaped = this.marksOn ? (part) => marked(escapeHtml(part)) : escapeHtml;
  let html = "";
  for (const [from, to, key] of this.rowPieces(line)) {
    html += from < 0 ? key : key ? `<span class="${key}">${escaped(text.slice(from, to))}</span>` : escaped(text.slice(from, to));
  }
  if (!folded) {
    this.rowsKept.rows.set(line, { made, html });
  }
  return html;
}

// The row `row` put into `node`, the one it is held in, in place: its runs as runsPut puts them, a wrapped line's row
// in its part as wide as the row's cols, the spans a row held in its part or out of one taken in or out with it; true
// where it was put, and false where its markup alone draws it: a wrapped line's row whose line holds a tab, a text in
// the colors its codes set, marks shown, or a hint, a mark or a fold among its runs.
export function rowPatched(node, row) {
  const rows = this.rows();
  const s = this.s;
  if (s.colored || this.marksOn) {
    return false;
  }
  const line = rows.lineOf(row);
  const pieces = this.rowPieces(line);
  if (pieces.some(([from]) => from < 0)) {
    return false;
  }
  const text = s.doc.line(line);
  const start = rows.startOf(row);
  const end = rows.endOf(row);
  const held = node.firstChild;
  const segment = held?.nodeType === Node.ELEMENT_NODE && held.className === "ed-seg" && !held.nextSibling ? held : null;
  if (start === 0 && end < 0) {
    if (segment) {
      node.replaceChildren(...segment.childNodes);
    }
    runsPut(node, text, pieces, 0, Infinity);
    return true;
  }
  if (text.includes("\t")) {
    return false;
  }
  let part = segment;
  if (!part) {
    part = document.createElement("span");
    part.className = "ed-seg";
    part.append(...node.childNodes);
    node.append(part);
  }
  const width = end < 0 ? "" : `${(this.vcolOf(text, end, line) - this.vcolOf(text, start, line)) * this.cw}px`;
  if (part.style.width !== width) {
    part.style.width = width;
  }
  const shown = this.shownOf(text);
  runsPut(part, text, pieces, start, end < 0 ? shown : Math.min(end, shown));
  return true;
}

// The runs of `pieces`, runs of a line's text alone in the order of their cols, that stand in its cols `from` to before
// `to`, each cut to them, put into `host` in place: each run with colors the span standing at its place, given its
// class and its text where they differ, each run without colors a text, and spans made or let go only where the runs
// come to more or fewer than `host` holds. A span is taken for a run only where its class is all it carries, so no
// hint's width or fold's line stays on a run's span.
function runsPut(host, text, pieces, from, to) {
  // the first run that reaches past `from`
  let low = 0;
  let high = pieces.length;
  while (low < high) {
    const middle = (low + high) >> 1;
    if (pieces[middle][1] <= from) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  let child = host.firstChild;
  for (let at = low; at < pieces.length && pieces[at][0] < to; at += 1) {
    const [start, end, key] = pieces[at];
    const part = text.slice(Math.max(start, from), Math.min(end, to));
    const spanned = child?.nodeType === Node.ELEMENT_NODE && child.attributes.length === 1 && child.firstChild?.nodeType === Node.TEXT_NODE && !child.firstChild.nextSibling;
    if (key && spanned) {
      if (child.className !== key) {
        child.className = key;
      }
      if (child.firstChild.data !== part) {
        child.firstChild.data = part;
      }
      child = child.nextSibling;
    } else if (!key && child?.nodeType === Node.TEXT_NODE) {
      if (child.data !== part) {
        child.data = part;
      }
      child = child.nextSibling;
    } else if (key) {
      const span = document.createElement("span");
      span.className = key;
      span.textContent = part;
      host.insertBefore(span, child);
    } else {
      host.insertBefore(document.createTextNode(part), child);
    }
  }
  while (child) {
    const next = child.nextSibling;
    child.remove();
    child = next;
  }
}

// The markup of the row of a wrapped line that holds its cols from `start` to before `end`, `end` below 0 for the
// line's end, in a cell as wide as those cols: the runs of its text that stand in them, each cut to them, and the
// markup that stands between runs there, the marks at the line's end with its last row. A line with a tab, or a text
// shown in the colors its codes set, is drawn whole in the row and moved left by the cols before it, its tabs standing
// where the line sets them. A row's markup is kept as a line's is.
export function partCell(line, start, end) {
  const s = this.s;
  const text = s.doc.line(line);
  const shown = this.shownOf(text);
  const folded = s.folded.has(line);
  const made = [text, s.highlight.runsOf(line), s.hints?.byLine.get(line) ?? null, this.marksOn, line + 1 < s.doc.count, this.hintKinds.parameter, this.hintKinds.type, shown, end, this.cw];
  if (this.rowsKept?.session !== s || this.rowsKept.rows.size > ROWS_KEPT) {
    this.rowsKept = { session: s, rows: new Map() };
  }
  const key = `${line}:${start}`;
  const kept = folded ? null : this.rowsKept.rows.get(key);
  if (kept && kept.made.every((part, at) => part === made[at])) {
    return kept.html;
  }
  let html;
  if (s.colored || text.includes("\t")) {
    const shift = this.vcolOf(text, start, line) * this.cw;
    const width = end < 0 ? null : this.vcolOf(text, end, line) * this.cw - shift;
    html = `<span class="ed-seg"${width === null ? "" : ` style="width:${width}px"`}><span style="margin-left:${-shift}px">${this.rowHtml(line)}</span></span>`;
  } else {
    const escaped = this.marksOn ? (part) => marked(escapeHtml(part)) : escapeHtml;
    const stop = end < 0 ? shown : Math.min(end, shown);
    // the col the text drawn so far reaches: markup between runs stands there
    let at = 0;
    let inside = "";
    for (const [from, to, key] of this.rowPieces(line)) {
      if (from < 0) {
        inside += at >= start && (at < stop || end < 0) ? key : "";
        continue;
      }
      at = to;
      const first = Math.max(from, start);
      const last = Math.min(to, stop);
      if (last > first) {
        inside += key ? `<span class="${key}">${escaped(text.slice(first, last))}</span>` : escaped(text.slice(first, last));
      }
    }
    const width = end < 0 ? null : (this.vcolOf(text, end, line) - this.vcolOf(text, start, line)) * this.cw;
    html = `<span class="ed-seg"${width === null ? "" : ` style="width:${width}px"`}>${inside}</span>`;
  }
  if (!folded) {
    this.rowsKept.rows.set(key, { made, html });
  }
  return html;
}

// The gutter's mark of how a line differs from the last commit: a bar for a line added or
// changed, and a wedge at the line's top edge where lines above it were taken out, at the last
// line's foot for lines taken out below it.
export function changeMark(line) {
  const changes = this.s.changes;
  if (!changes) {
    return "";
  }
  let mark = "";
  if (changes.added.has(line)) {
    mark += `<span class="ed-change added" data-change="${line}"></span>`;
  } else if (changes.changed.has(line)) {
    mark += `<span class="ed-change changed" data-change="${line}"></span>`;
  }
  if (changes.removed.has(line)) {
    mark += `<span class="ed-change removed" data-change="${line}"></span>`;
  }
  if (line === this.s.doc.count - 1 && changes.removed.has(line + 1)) {
    mark += `<span class="ed-change removed below" data-change="${line + 1}"></span>`;
  }
  return mark;
}

// Sets the marks of how a session's lines differ from the last commit, or takes them away.
export function setChanges(session, changes) {
  session.changes = changes;
  if (session === this.s) {
    this.schedule();
  }
}

// Line History's mark for a line: the date of the commit that last changed it, and its author
// where Git, Authors is on, on the first line of each run of lines that commit left, and the
// commit's id and subject under the pointer. A line no commit holds has none.
export function historyMark(line) {
  const history = this.s.history;
  const at = history.lines[line] ?? -1;
  const commit = history.commits[at];
  if (!commit) {
    return "";
  }
  const first = line === 0 || history.lines[line - 1] !== at;
  const date = new Date(commit.when * 1000);
  const day = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
  const authors = authorsShown();
  const title = escapeHtml(`${commit.id.slice(0, 8)}  ${authors ? `${commit.author}  ` : ""}${day}\n${commit.subject}`);
  const width = Math.round(historyChars() * this.cw);
  return `<span class="ed-history${first ? " first" : ""}" data-history="${line}" style="left:${BREAK_STRIP}px;width:${width}px" title="${title}">${first ? `${day}${authors ? ` ${escapeHtml(commit.author)}` : ""}` : ""}</span>`;
}

// Sets the commit that last changed each of a session's lines, or takes them away.
export function setLineHistory(session, history) {
  session.history = history;
  if (session === this.s) {
    this.schedule();
  }
}

// How many lines the session's file holds: every one once it is read whole, and while it is read
// a window at a time, as many as were counted as it opened.
export function linesInFile() {
  const s = this.s;
  if (!s) {
    return 0;
  }
  if (!s.window) {
    return s.base + s.doc.count;
  }
  return s.base + s.doc.count + s.window.after;
}

// Where a row stands in the sheet, from the origin.
export function yOf(row) {
  return (row - this.originRow) * LINE;
}

export function box(name, x, y, width, height = LINE) {
  return `<div class="${name}" style="left:${x}px;top:${y}px;width:${Math.max(0, width)}px;height:${height}px"></div>`;
}

// The marks behind the text on one row for a span of a line, from col to col.
export function span(name, line, row, from, to, past = false) {
  const text = this.doc.line(line);
  const x = PAD + this.vcolOf(text, from, line) * this.cw;
  const width = (this.vcolOf(text, to, line) - this.vcolOf(text, from, line)) * this.cw + (past ? this.cw * 0.6 : 0);
  return this.box(name, x, this.yOf(row), width);
}

// The marks behind the text for a span of a line, from col to col, each [row, markup] on every row
// of the line it reaches: one where the line is not wrapped. None where the line is folded away.
export function spans(name, line, from, to, past = false) {
  const rows = this.rows();
  const first = rows.rowOf(line);
  if (rows.lineOf(first) !== line) {
    return [];
  }
  const text = this.doc.line(line);
  const out = [];
  for (let row = first; row < rows.size && rows.lineOf(row) === line; row += 1) {
    const start = rows.startOf(row);
    const end = rows.endOf(row);
    const stop = end < 0 ? Infinity : end;
    const point = from === to && rows.rowAt(line, from) === row;
    if (!point && (from >= stop || to <= start)) {
      continue;
    }
    const a = Math.max(from, start);
    const b = Math.min(to, stop);
    const shift = this.vcolOf(text, start, line);
    const x = PAD + (this.vcolOf(text, a, line) - shift) * this.cw;
    const width = (this.vcolOf(text, b, line) - this.vcolOf(text, a, line)) * this.cw + (past && end < 0 ? this.cw * 0.6 : 0);
    out.push([row, this.box(name, x, this.yOf(row), width)]);
  }
  return out;
}

export function guides(line) {
  const doc = this.doc;
  const size = this.s.indent.size;
  let width = indentOf(doc.line(line), size);
  if (width < 0) {
    let before = -1;
    let after = -1;
    for (let at = line - 1; at >= Math.max(0, line - 200) && before < 0; at -= 1) {
      before = indentOf(doc.line(at), size);
    }
    for (let at = line + 1; at < Math.min(doc.count, line + 200) && after < 0; at += 1) {
      after = indentOf(doc.line(at), size);
    }
    width = Math.max(0, Math.min(before, after));
  }
  return Math.floor(width / size);
}
