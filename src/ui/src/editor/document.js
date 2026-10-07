// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A text as the editor holds it: its lines, the edits written to it, and the steps that undo them.
//
// A place in the text is { line, col }, both from 0, col counted in UTF-16 units as a JS string
// counts them. An edit is { from, to, text }: the span it removes and what it writes there. The
// edits of one change never overlap and each is placed against the text as it stood before any.

export const pos = (line, col) => ({ line, col });

export function cmp(a, b) {
  return a.line - b.line || a.col - b.col;
}

export const same = (a, b) => cmp(a, b) === 0;
export const least = (a, b) => (cmp(a, b) <= 0 ? a : b);
export const most = (a, b) => (cmp(a, b) >= 0 ? a : b);

// Where a text written from a place ends.
export function endOf(from, text) {
  const cut = text.lastIndexOf("\n");
  if (cut < 0) {
    return pos(from.line, from.col + text.length);
  }
  let lines = 0;
  for (let at = text.indexOf("\n"); at >= 0; at = text.indexOf("\n", at + 1)) {
    lines += 1;
  }
  return pos(from.line + lines, text.length - cut - 1);
}

// Where a place lands once an edit is written. A place at the very start of an edit stays before
// what the edit writes unless `after` is set, and a place inside the removed span goes to its start
// or, with `after`, to the end of what is written.
export function mapPos(p, edit, after = false) {
  const atStart = cmp(p, edit.from);
  if (atStart < 0 || (atStart === 0 && !after)) {
    return p;
  }
  const end = endOf(edit.from, edit.text);
  if (cmp(p, edit.to) < 0) {
    return after ? end : edit.from;
  }
  if (p.line === edit.to.line) {
    return pos(end.line, end.col + p.col - edit.to.col);
  }
  return pos(p.line + end.line - edit.to.line, p.col);
}

// A place carried through every edit of one change. The edits go last first, because each lies
// wholly before the ones after it and so keeps its places while those are written.
export function mapThrough(p, edits, after = false) {
  let at = p;
  for (let index = edits.length - 1; index >= 0; index -= 1) {
    at = mapPos(at, edits[index], after);
  }
  return at;
}

const WORD_CHAR = /[\p{L}\p{N}_$]/u;

export function isWordChar(char) {
  return char !== undefined && WORD_CHAR.test(char);
}

// The word a place touches, inside it or at either end, or null.
export function wordAt(doc, p) {
  const text = doc.line(p.line);
  let start = p.col;
  let end = p.col;
  while (start > 0 && isWordChar(text[start - 1])) {
    start -= 1;
  }
  while (end < text.length && isWordChar(text[end])) {
    end += 1;
  }
  return start === end ? null : { text: text.slice(start, end), from: pos(p.line, start), to: pos(p.line, end) };
}

// The part of a word that lies before a place.
export function wordBefore(doc, p) {
  const text = doc.line(p.line);
  let start = p.col;
  while (start > 0 && isWordChar(text[start - 1])) {
    start -= 1;
  }
  return { text: text.slice(start, p.col), from: pos(p.line, start), to: p };
}

// Edits of one kind that follow each other inside this many milliseconds undo as one step.
const JOINED = 1200;

export class Doc {
  constructor(text) {
    this.eol = text.includes("\r\n") ? "\r\n" : "\n";
    this.lines = text.split(/\r?\n/);
    // Every state the text has been in has an id, and undo returns to the id it left. A tab is
    // dirty while the id differs from the one it was saved at.
    this.id = 0;
    this.ids = 0;
    this.done = [];
    this.undone = [];
    this.watchers = new Set();
  }

  get count() {
    return this.lines.length;
  }

  line(n) {
    return this.lines[n] ?? "";
  }

  text() {
    return this.lines.join(this.eol);
  }

  end() {
    return pos(this.lines.length - 1, this.lines.at(-1).length);
  }

  clamp(p) {
    const line = Math.max(0, Math.min(this.lines.length - 1, p.line));
    return pos(line, Math.max(0, Math.min(this.lines[line].length, p.col)));
  }

  slice(from, to) {
    if (from.line === to.line) {
      return this.lines[from.line].slice(from.col, to.col);
    }
    const parts = [this.lines[from.line].slice(from.col)];
    for (let line = from.line + 1; line < to.line; line += 1) {
      parts.push(this.lines[line]);
    }
    parts.push(this.lines[to.line].slice(0, to.col));
    return parts.join("\n");
  }

  watch(watcher) {
    this.watchers.add(watcher);
    return () => this.watchers.delete(watcher);
  }

  // Writes a change and answers { edits, undo }: the edits in order with any that overlap an
  // earlier one dropped, and the edits that take the text back, placed in the text as it now is.
  write(given) {
    const sorted = [...given].map((e) => ({ from: this.clamp(e.from), to: this.clamp(e.to), text: e.text })).sort((a, b) => cmp(a.from, b.from));
    const edits = [];
    for (const edit of sorted) {
      if (cmp(edit.to, edit.from) < 0) {
        [edit.from, edit.to] = [edit.to, edit.from];
      }
      if (edits.length && cmp(edit.from, edits.at(-1).to) < 0) {
        continue;
      }
      if (same(edit.from, edit.to) && !edit.text) {
        continue;
      }
      edits.push(edit);
    }
    if (!edits.length) {
      return { edits, undo: [] };
    }
    const removed = edits.map((e) => this.slice(e.from, e.to));
    if (edits.length <= 8) {
      for (let index = edits.length - 1; index >= 0; index -= 1) {
        this.splice(edits[index]);
      }
    } else {
      this.rebuild(edits);
    }
    const undo = [];
    let lineShift = 0;
    let shiftedLine = -1;
    let colShift = 0;
    edits.forEach((edit, index) => {
      const col = edit.from.line === shiftedLine ? edit.from.col + colShift : edit.from.col;
      const from = pos(edit.from.line + lineShift, col);
      const end = endOf(from, edit.text);
      undo.push({ from, to: end, text: removed[index] });
      lineShift += end.line - from.line - (edit.to.line - edit.from.line);
      shiftedLine = edit.to.line;
      colShift = end.col - edit.to.col;
    });
    const first = edits[0].from.line;
    this.watchers.forEach((watcher) => watcher({ edits, first }));
    return { edits, undo };
  }

  splice(edit) {
    const head = this.lines[edit.from.line].slice(0, edit.from.col);
    const tail = this.lines[edit.to.line].slice(edit.to.col);
    const parts = edit.text.split("\n");
    parts[0] = head + parts[0];
    parts[parts.length - 1] += tail;
    this.lines.splice(edit.from.line, edit.to.line - edit.from.line + 1, ...parts);
  }

  // Writes many edits in one pass over the lines. Splicing each in turn costs the whole text per edit.
  rebuild(edits) {
    const out = [];
    let buffer = "";
    let at = pos(0, 0);
    const copy = (to) => {
      if (at.line === to.line) {
        buffer += this.lines[at.line].slice(at.col, to.col);
        return;
      }
      out.push(buffer + this.lines[at.line].slice(at.col));
      for (let line = at.line + 1; line < to.line; line += 1) {
        out.push(this.lines[line]);
      }
      buffer = this.lines[to.line].slice(0, to.col);
    };
    for (const edit of edits) {
      copy(edit.from);
      const parts = edit.text.split("\n");
      buffer += parts[0];
      for (let index = 1; index < parts.length; index += 1) {
        out.push(buffer);
        buffer = parts[index];
      }
      at = edit.to;
    }
    copy(this.end());
    out.push(buffer);
    this.lines = out;
  }

  // Writes a change the undo history keeps. `before` is what the caller restores on undo, its
  // selections, and `kind` joins this change to the last one where both are typing of one kind.
  change(edits, kind, before) {
    const { edits: written, undo } = this.write(edits);
    if (!written.length) {
      return { edits: written, undo };
    }
    this.undone = [];
    const from = this.id;
    this.ids += 1;
    this.id = this.ids;
    const last = this.done.at(-1);
    const now = performance.now();
    if (kind && last && !last.sealed && last.kind === kind && now - last.at < JOINED) {
      last.steps.push(undo);
      last.to = this.id;
      last.at = now;
    } else {
      this.done.push({ kind, steps: [undo], before, after: before, from, to: this.id, at: now, sealed: false });
    }
    return { edits: written, undo };
  }

  // Records the selections a change left, for redo to restore.
  settle(after) {
    const last = this.done.at(-1);
    if (last) {
      last.after = after;
    }
  }

  // Ends the step being typed. The next edit starts a step of its own.
  seal() {
    const last = this.done.at(-1);
    if (last) {
      last.sealed = true;
    }
  }

  // Moves every place the history holds down by `lines`, for lines read in above the text.
  shift(lines) {
    const down = (p) => ({ line: p.line + lines, col: p.col });
    const sel = (one) => ({ anchor: down(one.anchor), head: down(one.head), goal: one.goal });
    for (const entry of [...this.done, ...this.undone]) {
      entry.steps = entry.steps.map((step) => step.map((edit) => ({ from: down(edit.from), to: down(edit.to), text: edit.text })));
      entry.before = entry.before?.map(sel);
      entry.after = entry.after?.map(sel);
    }
  }

  undo() {
    return this.travel(this.done, this.undone, "before", "from");
  }

  redo() {
    return this.travel(this.undone, this.done, "after", "to");
  }

  travel(source, target, restore, id) {
    const entry = source.pop();
    if (!entry) {
      return null;
    }
    const back = [];
    let changed = null;
    for (const step of [...entry.steps].reverse()) {
      const { edits, undo } = this.write(step);
      back.push(undo);
      changed = changed ?? edits;
    }
    target.push({ ...entry, steps: back, sealed: true });
    this.id = entry[id];
    return { selections: entry[restore], edits: changed ?? [] };
  }
}
