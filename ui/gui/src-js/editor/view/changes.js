// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { pos } from "../document.js";
import { caret, empty, endOfSel, startOf } from "../view.js";

// Deletes from each cursor to the start of its line.
export function deleteToStart() {
  this.edit((sel) => (empty(sel) ? { from: pos(sel.head.line, 0), to: sel.head, text: "" } : { from: startOf(sel), to: endOfSel(sel), text: "" }), null);
}

export function tab(back) {
  if (this.suggest.open) {
    this.suggest.accept();
    return;
  }
  if (this.s.snippet && this.snippetStep(back ? -1 : 1)) {
    return;
  }
  const spans = this.s.selections.some((sel) => startOf(sel).line !== endOfSel(sel).line);
  if (back || spans) {
    this.indentLines(back ? -1 : 1);
    return;
  }
  const size = this.s.indent.size;
  this.edit((sel) => {
    const from = startOf(sel);
    const text = this.s.indent.tabs ? "\t" : " ".repeat(size - (this.vcol(from) % size));
    return { from, to: endOfSel(sel), text };
  }, "type");
}

export function indentLines(direction) {
  const doc = this.doc;
  const lines = this.linesOf();
  const size = this.s.indent.size;
  const unit = this.unit();
  const outdent = new RegExp(`^(?:\\t| {1,${size}})`);
  const edits = [];
  for (const line of lines) {
    const text = doc.line(line);
    if (direction > 0) {
      if (text.trim() || lines.length === 1) {
        edits.push({ from: pos(line, 0), to: pos(line, 0), text: unit });
      }
    } else {
      const found = text.match(outdent);
      if (found) {
        edits.push({ from: pos(line, 0), to: pos(line, found[0].length), text: "" });
      }
    }
  }
  this.change(edits, null, (map) =>
    this.s.selections.map((sel) => ({ anchor: map(sel.anchor, sel.anchor.col > 0 || empty(sel)), head: map(sel.head, sel.head.col > 0 || empty(sel)), goal: null }))
  );
}

export function toggleComment() {
  const comments = this.s.language?.comments ?? {};
  const doc = this.doc;
  if (comments.line) {
    const token = comments.line;
    const filled = this.linesOf().filter((line) => doc.line(line).trim());
    if (!filled.length) {
      return;
    }
    const done = filled.every((line) => doc.line(line).trimStart().startsWith(token));
    const edits = [];
    if (done) {
      for (const line of filled) {
        const text = doc.line(line);
        const at = text.length - text.trimStart().length;
        const cut = text.slice(at + token.length).startsWith(" ") ? token.length + 1 : token.length;
        edits.push({ from: pos(line, at), to: pos(line, at + cut), text: "" });
      }
    } else {
      const lead = filled.reduce((found, line) => Math.min(found, doc.line(line).match(/^[ \t]*/)[0].length), Infinity);
      for (const line of filled) {
        edits.push({ from: pos(line, lead), to: pos(line, lead), text: `${token} ` });
      }
    }
    this.change(edits, null);
    return;
  }
  if (comments.block) {
    const [open, close] = comments.block;
    this.edit((sel) => {
      const from = startOf(sel);
      const to = endOfSel(sel);
      const text = doc.slice(from, to);
      if (text.startsWith(open) && text.endsWith(close) && text.length >= open.length + close.length) {
        const inner = text.slice(open.length, text.length - close.length).replace(/^ /, "").replace(/ $/, "");
        return { from, to, text: inner, select: [0, inner.length] };
      }
      return { from, to, text: `${open} ${text} ${close}`, select: [open.length + 1, open.length + 1 + text.length] };
    }, null);
  }
}

export function moveLines(direction) {
  const doc = this.doc;
  const blocks = this.blocks();
  if ((direction < 0 && blocks[0][0] === 0) || (direction > 0 && blocks.at(-1)[1] >= doc.count - 1)) {
    return;
  }
  const edits = blocks.map(([first, last]) => {
    const block = doc.lines.slice(first, last + 1).join("\n");
    if (direction < 0) {
      return { from: pos(first - 1, 0), to: pos(last, doc.line(last).length), text: `${block}\n${doc.line(first - 1)}` };
    }
    return { from: pos(first, 0), to: pos(last + 1, doc.line(last + 1).length), text: `${doc.line(last + 1)}\n${block}` };
  });
  const shift = (p) => pos(p.line + direction, p.col);
  this.change(edits, null, () => this.s.selections.map((sel) => ({ anchor: shift(sel.anchor), head: shift(sel.head), goal: sel.goal })));
}

export function copyLines(direction) {
  const doc = this.doc;
  const blocks = this.blocks();
  const edits = blocks.map(([first, last]) => {
    const block = doc.lines.slice(first, last + 1).join("\n");
    return direction > 0
      ? { from: pos(last, doc.line(last).length), to: pos(last, doc.line(last).length), text: `\n${block}` }
      : { from: pos(first, 0), to: pos(first, 0), text: `${block}\n` };
  });
  const shiftOf = (line) => {
    let shift = 0;
    for (const [first, last] of blocks) {
      const inside = first <= line && line <= last;
      if (last < line || (direction > 0 && inside)) {
        shift += last - first + 1;
      }
    }
    return shift;
  };
  const move = (p) => pos(p.line + shiftOf(p.line), p.col);
  this.change(edits, null, () => this.s.selections.map((sel) => ({ anchor: move(sel.anchor), head: move(sel.head), goal: sel.goal })));
}

export function deleteLines() {
  const doc = this.doc;
  const edits = this.blocks().map(([first, last]) => {
    if (last < doc.count - 1) {
      return { from: pos(first, 0), to: pos(last + 1, 0), text: "" };
    }
    if (first > 0) {
      return { from: pos(first - 1, doc.line(first - 1).length), to: pos(last, doc.line(last).length), text: "" };
    }
    return { from: pos(0, 0), to: pos(last, doc.line(last).length), text: "" };
  });
  const cols = this.s.selections.map((sel) => sel.head.col);
  this.change(edits, null, (map) => this.s.selections.map((sel, index) => caret(pos(map(pos(sel.head.line, 0)).line, cols[index]))));
}

export function insertLine(below) {
  const doc = this.doc;
  this.edit((sel) => {
    const line = sel.head.line;
    const lead = doc.line(line).match(/^[ \t]*/)[0];
    if (below) {
      const end = pos(line, doc.line(line).length);
      return { from: end, to: end, text: `\n${lead}` };
    }
    return { from: pos(line, 0), to: pos(line, 0), text: `${lead}\n`, select: [lead.length, lead.length] };
  }, null);
}

// Undo, or redo where `back` is false. A step tied to steps in other files, as a refactoring across
// files makes, is taken back or brought again in all of them.
export function undo(back = true) {
  const doc = this.doc;
  const step = back ? doc.steps.get(doc.id) : doc.steps.get(doc.next.get(doc.id));
  if (!this.s?.readOnly && step?.type?.startsWith?.("group:") && step.done === back) {
    this.onGroup(step.type, back, doc);
  }
  this.travel(() => (back ? this.doc.undo() : this.doc.redo()));
}

// Goes to any state of the text's history, as Undo History lists them.
export function goToState(id) {
  this.travel(() => this.doc.goTo(id));
}

export function travel(step) {
  if (this.s.readOnly) {
    return;
  }
  this.doc.writer = this.s;
  const found = step();
  this.doc.writer = null;
  if (!found) {
    return;
  }
  this.setSelections(found.selections.map((sel) => ({ ...sel })), found.selections[Math.min(this.s.primary, found.selections.length - 1)]);
  this.edited();
  this.moved();
  this.reveal();
}
