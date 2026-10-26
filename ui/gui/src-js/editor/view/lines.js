// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cmp, isWordChar, pos, same, wordAt } from "../document.js";
import { caret, empty, endOfSel, SPANS_WAIT, startOf } from "../view.js";

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

// The bracket at or before the primary cursor and the one it pairs with, outside strings and
// comments, or null.
// The bracket pair at the cursor, read once for each state of the text and place of the cursor: a
// scroll asks for it every frame, and its answer stays until either moves.
export function bracketPair() {
  const s = this.s;
  const head = this.primary().head;
  const key = [s.doc.id, s.doc.count, s.base, head.line, head.col].join("|");
  if (this.pairKept?.session !== s || this.pairKept.key !== key) {
    this.pairKept = { session: s, key, pair: this.bracketPairNow() };
  }
  return this.pairKept.pair;
}

export function bracketPairNow() {
  const s = this.s;
  const head = this.primary().head;
  const pairs = { "(": ")", "[": "]", "{": "}" };
  const back = { ")": "(", "]": "[", "}": "{" };
  const plain = (line, col) => !/t-comment|t-string/.test(s.highlight.classAt(line, col));
  const text = s.doc.line(head.line);
  for (const col of [head.col - 1, head.col]) {
    const char = text[col];
    if (!(char in pairs || char in back) || !plain(head.line, col)) {
      continue;
    }
    const forward = char in pairs;
    const mate = forward ? pairs[char] : back[char];
    let depth = 0;
    let line = head.line;
    let at = col;
    const limit = 3000;
    for (let seen = 0; seen < limit && line >= 0 && line < s.doc.count; ) {
      const row = s.doc.line(line);
      for (; at >= 0 && at < row.length; at += forward ? 1 : -1) {
        const found = row[at];
        if ((found === char || found === mate) && plain(line, at)) {
          depth += found === char ? 1 : -1;
          if (depth === 0) {
            return [pos(head.line, col), pos(line, at)];
          }
        }
      }
      line += forward ? 1 : -1;
      seen += 1;
      at = forward ? 0 : s.doc.line(line).length - 1;
    }
  }
  return null;
}

// Extend Selection: the primary selection grows to the next span that holds it, the word, the inside
// of the string it is in, the string, the inside of the brackets around it, the brackets, its whole
// lines, then the whole text; and, where the file has a parse, each span of the parse between them,
// its statements and its blocks among them. Shrink Selection takes each step back.
export async function expandSelection() {
  const s = this.s;
  const sel = this.primary();
  const asked = [cmp(sel.anchor, sel.head) <= 0 ? sel.anchor : sel.head, cmp(sel.anchor, sel.head) <= 0 ? sel.head : sel.anchor];
  // The tree's spans are taken where they come within SPANS_WAIT; past it the editor's own spans below answer alone,
  // so the selection grows at once whatever the parse's state.
  const parsed = await Promise.race([this.spansOf(s, asked[0], asked[1]), new Promise((done) => window.setTimeout(() => done([]), SPANS_WAIT))]);
  if (this.s !== s || this.primary() !== sel) {
    return;
  }
  const [from, to] = cmp(sel.anchor, sel.head) <= 0 ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
  const holds = (span) => cmp(span[0], from) <= 0 && cmp(to, span[1]) <= 0 && (cmp(span[0], from) < 0 || cmp(to, span[1]) < 0);
  const spans = [];
  const word = wordAt(s.doc, from);
  if (word && from.line === to.line) {
    spans.push([word.from, word.to]);
  }
  // The string the selection is in, on its line: inside its quotes, then with them.
  if (from.line === to.line) {
    const text = s.doc.line(from.line);
    const quoted = /(["'`])(?:\\.|(?!\1).)*\1/g;
    for (let found = quoted.exec(text); found; found = quoted.exec(text)) {
      const start = found.index;
      const end = start + found[0].length;
      if (start <= from.col && to.col <= end) {
        spans.push([pos(from.line, start + 1), pos(from.line, end - 1)], [pos(from.line, start), pos(from.line, end)]);
      }
    }
  }
  // The brackets around it, found outward from both ends, past comments and strings.
  const plain = (line, col) => !/t-comment|t-string/.test(s.highlight.classAt(line, col));
  const opens = { "(": ")", "[": "]", "{": "}" };
  const closes = { ")": "(", "]": "[", "}": "{" };
  let depth = {};
  let open = null;
  for (let line = from.line, col = from.col - 1, seen = 0; line >= 0 && seen < 4000 && !open; seen += 1) {
    const text = s.doc.line(line);
    for (; col >= 0; col -= 1) {
      const char = text[col];
      if (char in closes && plain(line, col)) {
        depth[char] = (depth[char] ?? 0) + 1;
      } else if (char in opens && plain(line, col)) {
        if (depth[opens[char]]) {
          depth[opens[char]] -= 1;
        } else {
          open = { at: pos(line, col), char };
          break;
        }
      }
    }
    line -= 1;
    col = line >= 0 ? s.doc.line(line).length - 1 : -1;
  }
  if (open) {
    const mate = opens[open.char];
    let level = 0;
    let close = null;
    for (let line = to.line, col = to.col, seen = 0; line < s.doc.count && seen < 4000 && !close; seen += 1) {
      const text = s.doc.line(line);
      for (; col < text.length; col += 1) {
        const char = text[col];
        if (char === open.char && plain(line, col)) {
          level += 1;
        } else if (char === mate && plain(line, col)) {
          if (level) {
            level -= 1;
          } else {
            close = pos(line, col);
            break;
          }
        }
      }
      line += 1;
      col = 0;
    }
    if (close) {
      spans.push([pos(open.at.line, open.at.col + 1), close], [open.at, pos(close.line, close.col + 1)]);
    }
  }
  const lastLine = to.col === 0 && to.line > from.line ? to.line - 1 : to.line;
  spans.push([pos(from.line, 0), pos(lastLine, s.doc.line(lastLine).length)], [pos(0, 0), s.doc.end()]);
  spans.push(...parsed.map(([start, end]) => [pos(start.line, start.col), pos(end.line, end.col)]));
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

export function jumpBracket() {
  const pair = this.bracketPair();
  if (pair) {
    const to = pair[1];
    this.select([caret(pos(to.line, to.col + 1))]);
  }
}
