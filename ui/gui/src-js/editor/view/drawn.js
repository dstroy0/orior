// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { indentOf } from "../folding.js";
import { calm, nextSlice, status } from "../../status.js";
import { authorsShown } from "../../authors.js";
import { BRACKETS_KEY, BRACKETS_LINES, BRACKETS_READ, BREAK_STRIP, COMMENTS_KEY, DRAW_SLICE, DRAWN_MOST, empty, escapeHtml, historyChars, LINE, marked, MARKS_KEY, PAD, PAST_KEY, REACH_STEP, ROWS_KEPT, SHOWN, UNBRACKETED } from "../view.js";

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
      if ((!canAbove && !canBelow) || drawn.to - drawn.from + 1 >= DRAWN_MOST) {
        return;
      }
      const goBelow = nextSlice({ canAbove, canBelow, moving: status.scroll.speed > 0, heading: status.scroll.heading, below }) === "below";
      below = !below;
      const from = goBelow ? drawn.to + 1 : Math.max(0, drawn.from - DRAW_SLICE);
      const to = goBelow ? Math.min(rows.size - 1, drawn.to + DRAW_SLICE) : drawn.from - 1;
      const parts = new Map();
      for (let row = from; row <= to; row += 1) {
        parts.set(row, ["ed-row", this.rowCell(row)]);
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

// Bracket pairs: each bracket outside a comment or a string colored by how deep it stands, the
// depth at the start of each line kept on the session until an edit reaches it.
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

export function setBrackets(on) {
  this.bracketsOn = on;
  localStorage.setItem(BRACKETS_KEY, String(on));
  this.textLayer.clear();
  this.schedule();
}

// The depth at the end of a line that starts at `depth`, a closing bracket never taking it below 0.
export function bracketEnd(line, depth) {
  const text = this.s.doc.line(line);
  const runs = this.s.highlight.runsNow(line);
  for (let index = 0; index < runs.length; index += 1) {
    const [start, name] = runs[index];
    if (UNBRACKETED.test(name)) {
      continue;
    }
    const end = runs[index + 1]?.[0] ?? text.length;
    for (let at = start; at < end; at += 1) {
      const char = text[at];
      if (char === "(" || char === "[" || char === "{") {
        depth += 1;
      } else if ((char === ")" || char === "]" || char === "}") && depth > 0) {
        depth -= 1;
      }
    }
  }
  return depth;
}

// The depth at the start of a line, or null where it is too far below what is read to read now.
export function depthAt(line) {
  const s = this.s;
  if (!s.depths) {
    s.depths = [0];
  }
  let at = s.depths.length - 1;
  if (line - at > BRACKETS_READ) {
    return null;
  }
  for (; at < line; at += 1) {
    s.depths[at + 1] = this.bracketEnd(at, s.depths[at]);
  }
  return s.depths[line];
}

// How many characters across a row is drawn to: as far as the view reaches, to the end of the step
// of REACH_STEP it reaches into.
export function reach() {
  if (this.wrapOn) {
    return Infinity;
  }
  const seen = Math.ceil(((this.leftNow ?? this.scroller.scrollLeft) + this.viewSize().width) / this.cw);
  return (Math.floor(seen / REACH_STEP) + 2) * REACH_STEP;
}

export function rowHtml(line) {
  const s = this.s;
  // A text shown in the colors its codes set draws each line as they made it.
  if (s.colored) {
    return (s.colored[line] ?? "") + (s.folded.has(line) && s.endOf(line) >= 0 ? `<span class="ed-folded" data-fold="${line}">⋯</span>` : "");
  }
  const text = s.doc.line(line);
  const runs = s.highlight.runsShown(line);
  const shown = Math.min(text.length, SHOWN, this.reach());
  let depth = this.bracketsOn && s.doc.count <= BRACKETS_LINES && s.language ? this.depthAt(line) : null;
  // A row's markup is kept while its text, its colors, its depth, its hints and its marks are the
  // ones it was made from, so that a scroll makes only the rows that come into view. A folded row
  // reads the line its fold closes at, and is made each time.
  const folded = s.folded.has(line);
  const made = [text, runs, depth, s.hints?.byLine.get(line) ?? null, this.marksOn, line + 1 < s.doc.count, this.hintKinds.parameter, this.hintKinds.type, shown];
  if (this.rowsKept?.session !== s || this.rowsKept.rows.size > ROWS_KEPT) {
    this.rowsKept = { session: s, rows: new Map() };
  }
  const kept = folded ? null : this.rowsKept.rows.get(line);
  if (kept && kept.made.every((part, at) => part === made[at])) {
    return kept.html;
  }
  const escaped = this.marksOn ? (part) => marked(escapeHtml(part)) : escapeHtml;
  // The hints drawn before each col, each before the text from its col on.
  const hints = this.hintsOn(line);
  let hinted = 0;
  const hintsTo = (col) => {
    let out = "";
    while (hints && hinted < hints.length && hints[hinted].col <= col) {
      out += this.hintHtml(hints[hinted]);
      hinted += 1;
    }
    return out;
  };
  const pieces = [];
  for (let index = 0; index < runs.length; index += 1) {
    const [start, name] = runs[index];
    const end = Math.min(runs[index + 1]?.[0] ?? shown, shown);
    let from = start;
    while (from < end) {
      const before = hintsTo(from);
      const cut = hints && hinted < hints.length && hints[hinted].col < end ? hints[hinted].col : end;
      pieces.push([from, cut, name, before]);
      from = cut;
    }
  }
  let html = "";
  for (const [start, end, name, before] of pieces) {
    html += before;
    let part;
    if (depth === null || UNBRACKETED.test(name)) {
      part = escaped(text.slice(start, end));
    } else {
      part = "";
      let from = start;
      for (let at = start; at < end; at += 1) {
        const char = text[at];
        const opens = char === "(" || char === "[" || char === "{";
        const closes = char === ")" || char === "]" || char === "}";
        if (!opens && !closes) {
          continue;
        }
        if (closes && depth > 0) {
          depth -= 1;
        }
        part += `${escaped(text.slice(from, at))}<span class="ed-br" style="--depth:${depth}">${char}</span>`;
        if (opens) {
          depth += 1;
        }
        from = at + 1;
      }
      part += escaped(text.slice(from, end));
    }
    html += name ? `<span class="${name}">${part}</span>` : part;
  }
  html += hintsTo(Infinity);
  if (this.marksOn && line + 1 < s.doc.count) {
    html += '<span class="ed-eol">¬</span>';
  }
  if (s.folded.has(line) && s.endOf(line) >= 0) {
    html += `<span class="ed-folded" data-fold="${line}">⋯</span>`;
    const end = s.folded.get(line);
    const closed = this.closedEnd(line, end);
    if (closed !== end) {
      html += escapeHtml(s.doc.line(closed).trim());
    }
  }
  if (!folded) {
    this.rowsKept.rows.set(line, { made, html });
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
