// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cmp, isWordChar, pos, same, wordAt } from "../document.js";
import { caret, empty, endOfSel, startOf } from "../view.js";

// Lines.

// The spans of whole lines the selections cover, [first, last], in order and merged where they
// touch. A selection that only reaches the start of its last line leaves that line out, and one
// empty selection alone covers the whole text.
export function lineSpans() {
  const doc = this.doc;
  const sels = this.s.selections;
  if (sels.length === 1 && empty(sels[0])) {
    return [[0, doc.count - 1]];
  }
  const spans = sels
    .map((sel) => {
      const start = startOf(sel);
      const end = endOfSel(sel);
      return [start.line, end.line > start.line && end.col === 0 ? end.line - 1 : end.line];
    })
    .sort((a, b) => a[0] - b[0]);
  const merged = [];
  for (const span of spans) {
    const last = merged.at(-1);
    if (last && span[0] <= last[1] + 1) {
      last[1] = Math.max(last[1], span[1]);
    } else {
      merged.push([...span]);
    }
  }
  return merged;
}

// Writes each span of lines again as `make` gives it from its lines.
export function rewriteLines(make, kind) {
  const doc = this.doc;
  const edits = this.lineSpans().map(([first, last]) => ({ from: pos(first, 0), to: pos(last, doc.line(last).length), text: make(doc.lines.slice(first, last + 1)).join("\n") }));
  return this.change(edits, kind);
}

export function sortLines(descending) {
  const order = new Intl.Collator(undefined, { numeric: true }).compare;
  this.rewriteLines((lines) => [...lines].sort((a, b) => (descending ? order(b, a) : order(a, b))), "sort");
}

export function uniqueLines() {
  this.rewriteLines((lines) => [...new Set(lines)], "unique");
}

// Joins each selection's lines into one, and a cursor's line with the line after it: the space
// where two lines meet becomes one space, or none where either side is blank.
export function joinLines() {
  const doc = this.doc;
  const edits = [];
  for (const sel of this.s.selections) {
    const first = startOf(sel).line;
    const last = Math.min(doc.count - 1, Math.max(endOfSel(sel).line, first + 1));
    if (last === first) {
      continue;
    }
    let joined = doc.line(first);
    for (let line = first + 1; line <= last; line += 1) {
      const next = doc.line(line).trimStart();
      joined = joined.trimEnd();
      joined = !joined.trim() ? joined + next : !next ? joined : `${joined} ${next}`;
    }
    edits.push({ from: pos(first, 0), to: pos(last, doc.line(last).length), text: joined });
  }
  this.change(edits, "join");
}

// Changes the case of each selection, or of the word at each cursor: upper, lower, or title, each
// word's first letter upper and the rest lower.
export function transformCase(kind) {
  const make = {
    upper: (text) => text.toUpperCase(),
    lower: (text) => text.toLowerCase(),
    title: (text) => text.replace(/\p{L}[\p{L}\p{N}'’]*/gu, (word) => word[0].toUpperCase() + word.slice(1).toLowerCase()),
  }[kind];
  const edits = [];
  for (const sel of this.s.selections) {
    const span = empty(sel) ? wordAt(this.doc, sel.head) : { from: startOf(sel), to: endOfSel(sel) };
    if (span) {
      edits.push({ from: span.from, to: span.to, text: make(this.doc.slice(span.from, span.to)) });
    }
  }
  this.change(edits, "case", (map) => this.s.selections.map((sel) => ({ anchor: map(sel.anchor, false), head: map(sel.head, true), goal: null })));
}

export function selectAll() {
  this.select([{ anchor: pos(0, 0), head: this.doc.end(), goal: null }]);
}

export function selectLine() {
  const doc = this.doc;
  const next = this.s.selections.map((sel) => {
    const start = startOf(sel);
    let end = endOfSel(sel);
    if (!empty(sel) && start.col === 0 && end.col === 0) {
      end = pos(end.line, 1);
    }
    const last = end.col === 0 && end.line > start.line ? end.line - 1 : end.line;
    const stop = last + 1 < doc.count ? pos(last + 1, 0) : pos(last, doc.line(last).length);
    return { anchor: pos(start.line, 0), head: stop, goal: null };
  });
  this.select(next, next[this.s.primary]);
}

// Every match of a text, as { from, to }, found in the whole text joined by \n.
export function matchesOf(needle, whole) {
  const doc = this.doc;
  const out = [];
  if (!needle) {
    return out;
  }
  if (!needle.includes("\n")) {
    for (let line = 0; line < doc.count; line += 1) {
      const text = doc.line(line);
      for (let at = text.indexOf(needle); at >= 0; at = text.indexOf(needle, at + 1)) {
        if (whole && (isWordChar(text[at - 1]) || isWordChar(text[at + needle.length]))) {
          continue;
        }
        out.push({ from: pos(line, at), to: pos(line, at + needle.length) });
        if (out.length > 20000) {
          return out;
        }
      }
    }
    return out;
  }
  const all = doc.lines.join("\n");
  const starts = [0];
  doc.lines.forEach((text, index) => starts.push(starts[index] + text.length + 1));
  const place = (offset) => {
    let low = 0;
    let high = starts.length - 1;
    while (low < high) {
      const mid = (low + high + 1) >> 1;
      if (starts[mid] <= offset) {
        low = mid;
      } else {
        high = mid - 1;
      }
    }
    return pos(low, offset - starts[low]);
  };
  for (let at = all.indexOf(needle); at >= 0 && out.length <= 20000; at = all.indexOf(needle, at + 1)) {
    out.push({ from: place(at), to: place(at + needle.length) });
  }
  return out;
}

// Selects the word at each cursor, then adds the next match of the primary selection, or with
// `all` every match.
export function addMatch(all) {
  const doc = this.doc;
  const primary = this.primary();
  if (empty(primary)) {
    const next = this.s.selections.map((sel) => {
      if (!empty(sel)) {
        return sel;
      }
      const word = wordAt(doc, sel.head);
      return word ? { anchor: word.from, head: word.to, goal: null } : sel;
    });
    this.setSelections(next, next[this.s.primary]);
    if (!all) {
      this.moved();
      return;
    }
  }
  const chosen = this.primary();
  const from = startOf(chosen);
  const to = endOfSel(chosen);
  const needle = doc.slice(from, to);
  const word = /^[\p{L}\p{N}_$]+$/u.test(needle) && !isWordChar(doc.line(from.line)[from.col - 1]) && !isWordChar(doc.line(to.line)[to.col]);
  const matches = this.matchesOf(needle, word);
  if (all) {
    const list = matches.map((match) => ({ anchor: match.from, head: match.to, goal: null }));
    const kept = list.find((sel) => same(sel.anchor, from)) ?? list.at(-1);
    if (list.length) {
      this.select(list, kept);
    }
    return;
  }
  const taken = (match) => this.s.selections.some((sel) => same(startOf(sel), match.from) && same(endOfSel(sel), match.to));
  const after = matches.filter((match) => cmp(match.from, to) >= 0 && !taken(match));
  const found = after[0] ?? matches.find((match) => !taken(match));
  if (found) {
    const added = { anchor: found.from, head: found.to, goal: null };
    this.select([...this.s.selections, added], added);
  }
}

export function addCursor(direction) {
  const sels = this.s.selections;
  const edge = direction < 0 ? sels[0] : sels.at(-1);
  const rows = this.rows();
  const row = rows.rowOf(edge.head.line) + direction;
  if (row < 0 || row >= rows.size) {
    return;
  }
  const goal = edge.goal ?? this.vcol(edge.head);
  const line = rows.lineOf(row);
  const added = caret(pos(line, this.colAtV(this.doc.line(line), goal)), goal);
  this.select([...sels, added], added);
}

// The bracket pair at the cursor as orior's side reads it off the branch, by the roles the grammar names, its lines the
// file's, asked once for each state of the text and each place of the cursor in the file: a frame asks for it every
// frame, and its answer stays until either moves, none until it comes. One is asked at a time: a cursor moved on while
// one is asked has the place it stands asked once that answer comes, the places it passed meanwhile never asked. A
// paged file's window asks for none while it moves, its lines cut short or its view not yet at rest.
export function bracketPair() {
  const s = this.s;
  const head = this.primary().head;
  const moving = Boolean(s.window && (s.window.cut || !s.window.paged?.resting));
  const key = [s.doc.id, s.base + head.line, head.col, moving].join("|");
  if (this.pairKept?.session !== s || this.pairKept.key !== key) {
    this.pairKept = { session: s, key, pair: null, asked: false };
  }
  const kept = this.pairKept;
  if (!moving && !kept.asked && !this.pairAsked) {
    kept.asked = true;
    this.pairAsked = this.pairOf(s, head)
      .then((pair) => {
        if (this.pairKept === kept && pair) {
          kept.pair = pair;
        }
      })
      .finally(() => {
        this.pairAsked = null;
        this.schedule();
      });
  }
  return kept.pair?.map((at) => pos(at.line - s.base, at.col)) ?? null;
}

// Extend Selection: the primary selection grows to the least span orior's side reads off the text that holds it and
// is more than it: the leaf it stands in, each bracket pair around it, inside its brackets and with them, the
// statement it stands in among a region's, each region with its head, and the whole text, each by the roles its
// grammar names; or each span of the file's parse, where one reads it whole. Shrink Selection takes each step back.
export async function expandSelection() {
  const s = this.s;
  const sel = this.primary();
  const [from, to] = cmp(sel.anchor, sel.head) <= 0 ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
  const found = await this.spansOf(s, from, to);
  if (this.s !== s || this.primary() !== sel) {
    return;
  }
  const holds = (span) => cmp(span[0], from) <= 0 && cmp(to, span[1]) <= 0 && (cmp(span[0], from) < 0 || cmp(to, span[1]) < 0);
  const spans = found.map(([start, end]) => [pos(start.line, start.col), pos(end.line, end.col)]);
  const next = spans.filter(holds).sort((a, b) => cmp(b[0], a[0]) || cmp(a[1], b[1]))[0];
  if (!next) {
    return;
  }
  this.grown = [...(this.stillGrown() ? this.grown : []), { anchor: sel.anchor, head: sel.head }];
  this.select([{ anchor: next[0], head: next[1], goal: null }]);
  this.grownTo = { anchor: next[0], head: next[1] };
}

// Whether the selection is still the one Extend Selection last made, and so can shrink back.
export function stillGrown() {
  const sel = this.primary();
  return Boolean(this.grownTo && this.s.selections.length === 1 && same(sel.anchor, this.grownTo.anchor) && same(sel.head, this.grownTo.head));
}

export function shrinkSelection() {
  if (!this.stillGrown() || !this.grown?.length) {
    return;
  }
  const back = this.grown.pop();
  this.select([{ anchor: back.anchor, head: back.head, goal: null }]);
  this.grownTo = { anchor: back.anchor, head: back.head };
}

// The cursor taken past the bracket the one at it pairs with, once orior's side answers where the pair is not come yet.
export async function jumpBracket() {
  const s = this.s;
  const head = this.primary().head;
  const asked = this.bracketPair() ? null : await this.pairOf(s, head);
  const pair = asked?.map((at) => pos(at.line - s.base, at.col)) ?? this.bracketPair();
  if (pair && this.s === s && same(this.primary().head, head)) {
    const to = pair[1];
    this.select([caret(pos(to.line, to.col + 1))]);
  }
}
