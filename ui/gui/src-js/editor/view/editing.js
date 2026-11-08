// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cmp, endOf, isWordChar, mapThrough, pos } from "../document.js";
import { caret, empty, endOfSel, OPERATOR_END, startOf } from "../view.js";

// Editing.

// Writes edits and sets the selections. `place` is given a function that carries a place in the
// old text to the new and answers the selections, or, left out, every selection is carried.
export function change(edits, type, place) {
  const s = this.s;
  // a paged file's window of lines cut at the view's reach takes no edit until the view rests and its lines come whole
  if (s.readOnly || s.window?.cut) {
    return false;
  }
  const before = this.copySelections();
  s.doc.writer = s;
  const { edits: written } = s.doc.change(edits, type, before);
  s.doc.writer = null;
  const map = (p, after = false) => mapThrough(p, written, after);
  const next = place
    ? place(map)
    : s.selections.map((sel) => ({ anchor: map(sel.anchor, !empty(sel) && cmp(sel.anchor, sel.head) > 0), head: map(sel.head, true), goal: null }));
  this.setSelections(next, next[Math.min(s.primary, next.length - 1)]);
  if (written.length) {
    s.doc.settle(this.copySelections());
    this.edited();
  }
  this.moved();
  this.reveal();
  return written.length > 0;
}

// Each selection gives one edit, { from, to, text }, with `select`, the offsets into its text the
// selection goes to after, or null to leave the selection to be carried.
export function edit(plan, type) {
  const plans = this.s.selections.map((sel, index) => plan(sel, index));
  const edits = plans.filter(Boolean).map(({ from, to, text }) => ({ from, to, text }));
  return this.change(edits, type, (map) =>
    plans.map((one, index) => {
      const sel = this.s.selections[index];
      if (!one) {
        return { anchor: map(sel.anchor), head: map(sel.head, true), goal: null };
      }
      const from = map(one.from);
      const end = endOf(from, one.text);
      const at = (offset) => (offset <= one.text.length ? endOf(from, one.text.slice(0, offset)) : pos(end.line, end.col + offset - one.text.length));
      const [a, b] = one.select ?? [one.text.length, one.text.length];
      return { anchor: at(a), head: at(b), goal: null };
    })
  );
}

// Each change to the text is told to onEdit as soon as the event that made it is done, after a key
// has drawn it, and to onChange once the keys rest.
export function edited() {
  const s = this.s;
  queueMicrotask(() => this.onEdit(s));
  this.later("find", () => this.find.refresh());
  this.later("change", () => this.onChange(s));
}

// The lines the selections cover. A selection that ends at the start of a line leaves that line out.
export function linesOf() {
  const lines = new Set();
  for (const sel of this.s.selections) {
    const start = startOf(sel);
    const end = endOfSel(sel);
    const last = end.line > start.line && end.col === 0 ? end.line - 1 : end.line;
    for (let line = start.line; line <= last; line += 1) {
      lines.add(line);
    }
  }
  return [...lines].sort((a, b) => a - b);
}

// The covered lines as runs of neighbors, [first, last].
export function blocks() {
  const out = [];
  for (const line of this.linesOf()) {
    if (out.length && line <= out.at(-1)[1] + 1) {
      out.at(-1)[1] = line;
    } else {
      out.push([line, line]);
    }
  }
  return out;
}

export function type(text) {
  const s = this.s;
  if (!s) {
    return;
  }
  this.fillPastEnds();
  const lang = s.language ?? {};
  const pairs = lang.pairs ?? [];
  const opens = new Map(pairs.map((pair) => [pair[0], pair[1]]));
  (lang.quotes ?? []).forEach((quote) => opens.set(quote, quote));
  const closers = new Set([...opens.values()]);
  const brackets = new Set(pairs.filter((pair) => pair[0] !== pair[1]).map((pair) => pair[1]));
  const one = [...text].length === 1;
  const doc = s.doc;
  this.edit((sel) => {
    const from = startOf(sel);
    const to = endOfSel(sel);
    const line = doc.line(from.line);
    if (one) {
      const next = line[to.col];
      const prev = line[from.col - 1];
      if (empty(sel) && closers.has(text) && next === text) {
        return { from, to, text: "", select: [1, 1] };
      }
      if (opens.has(text)) {
        const close = opens.get(text);
        if (!empty(sel)) {
          const inner = doc.slice(from, to);
          return { from, to, text: text + inner + close, select: [1, 1 + inner.length] };
        }
        const quote = close === text;
        const nextOk = next === undefined || /[\s;,.)\]}>]/.test(next);
        const prevOk = !quote || (!isWordChar(prev) && prev !== "\\" && prev !== text);
        if (nextOk && prevOk) {
          return { from, to, text: text + close, select: [1, 1] };
        }
      }
      if (brackets.has(text) && empty(sel) && from.col > 0 && /^\s*$/.test(line.slice(0, from.col))) {
        const lead = line.slice(0, from.col);
        const unit = this.unit();
        const less = lead.endsWith(unit) ? lead.slice(0, -unit.length) : lead.replace(/(?:\t| +)$/, "");
        return { from: pos(from.line, 0), to, text: less + text };
      }
    }
    return { from, to, text };
  }, one && !/\s/.test(text) ? "type" : "space");
  if (lang.complete && one && isWordChar(text)) {
    this.suggest.later(true);
  } else if (this.suggest.open || this.suggest.pending) {
    this.suggest.later(false);
  }
}

// The closing bracket of the innermost bracket that line `line` opens before `col` and leaves open
// there, outside its strings and comments, or null where it leaves none open.
export function openBefore(line, col, lang) {
  return this.openAt(line, col, lang)?.close ?? null;
}

// The innermost bracket line `line` opens before `col` and leaves open there: its closing bracket,
// and where it stands.
export function openAt(line, col, lang) {
  const closing = new Map((lang.pairs ?? []).filter((pair) => pair[0] !== pair[1]).map((pair) => [pair[0], pair[1]]));
  const closers = new Set(closing.values());
  const text = this.doc.line(line);
  const open = [];
  for (let at = 0; at < col; at += 1) {
    const char = text[at];
    if (!closing.has(char) && !closers.has(char)) {
      continue;
    }
    if (/t-string|t-comment/.test(this.s.highlight.classAt(line, at))) {
      continue;
    }
    if (closing.has(char)) {
      open.push({ close: closing.get(char), at });
    } else if (open.at(-1)?.close === char) {
      open.pop();
    }
  }
  return open.at(-1) ?? null;
}

// A new line at each selection, as deep as the line it leaves, and a step deeper after what the
// language opens a block with. A bracket the line leaves open before the cursor puts the new line
// deeper too, by the continuation indent of the place the bracket stands, one step where none is
// set, as Black, rustfmt and Prettier lay out a call that runs past one line; and where what
// follows the cursor closes that bracket, the close goes down to a line of its own at the first
// line's depth. Inside a bracket, where the reader has an operator's sign go to the next line, a
// line that ends in a binary operator takes it to the head of the new line.
export function newline() {
  const lang = this.s.language ?? {};
  const doc = this.doc;
  this.edit((sel) => {
    const from = startOf(sel);
    const to = endOfSel(sel);
    const line = doc.line(from.line);
    const before = line.slice(0, from.col);
    const rest = doc.line(to.line).slice(to.col);
    const lead = (line.match(/^[ \t]*/)[0]).slice(0, from.col);
    const opened = this.openAt(from.line, from.col, lang);
    const open = opened?.close ?? null;
    const deeper = open ? this.continuationAt(from.line, opened.at) : lang.indentAfter?.test(before) ? this.unit() : "";
    const prev = before.trimEnd().at(-1);
    const next = rest.trimStart()[0];
    const paired = (lang.pairs ?? []).some((pair) => pair[0] !== pair[1] && pair[0] === prev && pair[1] === next) || (open !== null && next === open);
    if (paired && deeper) {
      const gap = rest.length - rest.trimStart().length;
      const text = `\n${lead}${deeper}\n${lead}`;
      const at = 1 + lead.length + deeper.length;
      return { from, to: pos(to.line, to.col + gap), text, select: [at, at] };
    }
    const operator = this.operatorNext && open && empty(sel) ? before.match(OPERATOR_END) : null;
    if (operator) {
      return { from: pos(from.line, operator[1].length), to, text: `\n${lead}${deeper}${operator[2]} ` };
    }
    return { from, to, text: `\n${lead}${deeper}` };
  }, null);
  this.doc.seal();
}

export function backspace(word = false) {
  // Cursors past their lines' ends step back a column; past the end beside others, they are filled
  // out to where they stand first.
  const past = this.s.selections.map((sel) => this.pastEnd(sel));
  if (past.every((columns) => columns > 0)) {
    this.select(this.s.selections.map((sel, index) => ({ ...caret(sel.head, this.vcol(sel.head) + past[index] - 1), column: sel.column })));
    return;
  }
  this.fillPastEnds();
  const lang = this.s.language ?? {};
  const doc = this.doc;
  const pairs = [...(lang.pairs ?? []), ...(lang.quotes ?? []).map((quote) => quote + quote)];
  this.edit((sel) => {
    if (!empty(sel)) {
      return { from: startOf(sel), to: endOfSel(sel), text: "" };
    }
    const p = sel.head;
    const line = doc.line(p.line);
    if (p.col === 0) {
      return p.line === 0 ? null : { from: pos(p.line - 1, doc.line(p.line - 1).length), to: p, text: "" };
    }
    if (word) {
      return { from: this.wordLeft(p), to: p, text: "" };
    }
    const prev = line[p.col - 1];
    const next = line[p.col];
    if (pairs.some((pair) => pair[0] === prev && pair[1] === next)) {
      return { from: pos(p.line, p.col - 1), to: pos(p.line, p.col + 1), text: "" };
    }
    const lead = line.slice(0, p.col);
    if (!this.s.indent.tabs && /^ +$/.test(lead)) {
      const size = this.s.indent.size;
      return { from: pos(p.line, p.col - (lead.length % size || size)), to: p, text: "" };
    }
    const step = p.col >= 2 && /[\uDC00-\uDFFF]/.test(prev) ? 2 : 1;
    return { from: pos(p.line, p.col - step), to: p, text: "" };
  }, "delete");
  if (this.suggest.open || this.suggest.pending) {
    this.suggest.later(false);
  }
}

export function deleteForward(word = false) {
  const doc = this.doc;
  this.edit((sel) => {
    if (!empty(sel)) {
      return { from: startOf(sel), to: endOfSel(sel), text: "" };
    }
    const p = sel.head;
    const line = doc.line(p.line);
    if (p.col >= line.length) {
      return p.line >= doc.count - 1 ? null : { from: p, to: pos(p.line + 1, 0), text: "" };
    }
    if (word) {
      return { from: p, to: this.wordRight(p), text: "" };
    }
    const step = /[\uD800-\uDBFF]/.test(line[p.col]) ? 2 : 1;
    return { from: p, to: pos(p.line, p.col + step), text: "" };
  }, "delete-forward");
}
