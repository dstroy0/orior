// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { endOf, pos } from "../document.js";
import { hiddenSpans } from "../folding.js";
import { caret, empty, endOfSel, parseSnippet, startOf } from "../view.js";

// Folding.

export function fold(line, close) {
  const s = this.s;
  // The innermost region holding the line is the nearest one above it whose end reaches it.
  let best = -1;
  for (let start = line; start >= Math.max(0, line - 5000); start -= 1) {
    const fits = close ? !s.folded.has(start) : s.folded.has(start);
    if (fits && s.endOf(start) >= line) {
      best = start;
      break;
    }
  }
  if (best < 0) {
    return;
  }
  if (close) {
    s.fold(best);
  } else {
    s.folded.delete(best);
  }
  this.folds();
}

export function toggleFold(line) {
  const s = this.s;
  if (s.folded.has(line)) {
    s.folded.delete(line);
  } else {
    s.fold(line);
  }
  this.folds();
}

export function foldAll(close) {
  const s = this.s;
  s.folded = close ? s.regions() : new Map();
  this.folds();
}

// The spans of lines the folds hide.
export function foldSpans() {
  return hiddenSpans(this.s.folded);
}

// After folds change: a cursor a fold hides goes to the end of the line that opens the fold.
export function folds() {
  const s = this.s;
  s.foldings += 1;
  const spans = this.foldSpans();
  const out = (p) => {
    const span = spans.find(([first, last]) => first <= p.line && p.line <= last);
    return span ? pos(span[0] - 1, this.doc.line(span[0] - 1).length) : p;
  };
  const next = s.selections.map((sel) => ({ anchor: out(sel.anchor), head: out(sel.head), goal: null }));
  this.setSelections(next, next[s.primary]);
  this.size();
  this.moved();
}

// Snippets.

export function insertSnippet(from, to, body) {
  const { text, stops } = parseSnippet(body);
  let placed = [];
  this.change([{ from, to, text }], null, (map) => {
    const start = map(from);
    placed = stops.map((stop) => ({ from: endOf(start, text.slice(0, stop.start)), to: endOf(start, text.slice(0, stop.end)) }));
    const first = placed[0] ?? { from: endOf(start, text), to: endOf(start, text) };
    return [{ anchor: first.from, head: first.to, goal: null }];
  });
  this.s.snippet = placed.length > 1 || (placed.length === 1 && stops[0].index !== 0) ? { stops: placed, at: 0 } : null;
  this.schedule();
}

export function snippetStep(direction) {
  const snippet = this.s.snippet;
  const at = snippet.at + direction;
  if (at < 0) {
    return true;
  }
  if (at >= snippet.stops.length) {
    const last = snippet.stops.at(-1);
    this.s.snippet = null;
    this.select([caret(last.to)]);
    return true;
  }
  snippet.at = at;
  const stop = snippet.stops[at];
  this.setSelections([{ anchor: stop.from, head: stop.to, goal: null }]);
  this.doc.seal();
  this.schedule();
  this.onCursor();
  this.reveal();
  return true;
}

// Leaving: closes what is open, then drops all but the primary selection, then collapses it.
export function escape() {
  if (this.suggest.open) {
    this.suggest.close();
  } else if (this.hover.shown) {
    this.hover.hide();
  } else if (this.find.shown) {
    this.find.close();
  } else if (this.s.snippet) {
    this.s.snippet = null;
    this.schedule();
  } else if (this.s.selections.length > 1) {
    const kept = this.primary();
    this.select([kept], kept);
  } else if (!empty(this.primary())) {
    this.select([caret(this.primary().head)]);
  }
}

// Clipboard.

export function copy(event, cut) {
  event.preventDefault();
  // a paged file's window of lines cut at the view's reach gives no copy until the view rests and its lines come whole
  if (this.s.window?.cut) {
    return;
  }
  const text = this.takeOut(cut);
  event.clipboardData.setData("text/plain", text.replace(/\n/g, this.doc.eol));
  this.note({ cut });
}

// What a copy takes, the selections' text or for none the whole lines the cursors are on, kept for
// a paste; a cut takes it out of the text as well.
export function takeOut(cut) {
  const doc = this.doc;
  const sels = this.s.selections;
  const whole = sels.every(empty);
  let parts;
  if (whole) {
    parts = [...new Set(sels.map((sel) => sel.head.line))].map((line) => `${doc.line(line)}\n`);
  } else {
    parts = sels.filter((sel) => !empty(sel)).map((sel) => doc.slice(startOf(sel), endOfSel(sel)));
  }
  const text = whole ? parts.join("") : parts.join("\n");
  this.clip = { text, whole, parts };
  if (cut) {
    if (whole) {
      this.deleteLines();
    } else {
      this.edit((sel) => (empty(sel) ? null : { from: startOf(sel), to: endOfSel(sel), text: "" }), null);
    }
  }
  return text;
}

// A paste puts the text on the clipboard, or the text pasteAs gives for it, at once or when its
// promise keeps it.
export function paste(event) {
  event.preventDefault();
  const text = event.clipboardData.getData("text/plain").replace(/\r\n?/g, "\n");
  if (text) {
    this.note({ paste: text });
    const given = this.pasteAs(this.s, text);
    if (typeof given?.then === "function") {
      given.then((made) => this.pasteText(made ?? text), () => this.pasteText(text));
    } else {
      this.pasteText(given ?? text);
    }
  }
}

// Puts text at each selection as a paste does: whole lines copied above the cursors' lines, a copy
// of as many selections as there are one part to each, and the text at each otherwise.
export function pasteText(text) {
  this.fillPastEnds();
  const sels = this.s.selections;
  const clip = this.clip?.text === text ? this.clip : null;
  if (clip?.whole && sels.every(empty)) {
    this.edit((sel) => ({ from: pos(sel.head.line, 0), to: pos(sel.head.line, 0), text, select: [text.length + sel.head.col, text.length + sel.head.col] }), null);
  } else if (sels.length > 1 && clip && !clip.whole && clip.parts.length === sels.length) {
    this.edit((sel, index) => ({ from: startOf(sel), to: endOfSel(sel), text: clip.parts[index] }), null);
  } else if (sels.length > 1 && text.split("\n").length === sels.length) {
    const parts = text.split("\n");
    this.edit((sel, index) => ({ from: startOf(sel), to: endOfSel(sel), text: parts[index] }), null);
  } else {
    this.edit((sel) => ({ from: startOf(sel), to: endOfSel(sel), text }), null);
  }
  this.doc.seal();
}
