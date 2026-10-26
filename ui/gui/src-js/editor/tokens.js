// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tokens of a text's lines as orior's side colors them, kept as they come: each line's runs one flat list, a column
// then a name for each run, read into classes as the line is drawn. A text held there comes with the runs of the lines
// in view; the page holds the runs of the lines near its view alone, those come as orior's side walks the text out from
// the view, and more are asked for as the view moves; the lines a change wrote come once it colors them again. The page
// colors nothing: a line a change wrote shows the runs of the line the change began on until its own come. A text read
// a window at a time counts its lines from the file's first, as orior's side does.

import { invoke, listen } from "../bridge.js";
import { classOf } from "./grammar.js";

export { classOf, compile, tokenKinds } from "./grammar.js";

const PLAIN = [[0, ""]];

// The most lines one splice puts in as its arguments.
const SPREAD = 1024;

// How many views' height of lines on each side of the view the page holds the runs of.
const HELD = 2;

// Each text held on orior's side by its key, and the colors it hands on heard once.
const held = new Map();
let hearing = null;

function heard() {
  hearing ??= listen("branch-colors", ({ payload }) => held.get(payload?.key)?.put(payload.seq, payload.colors, payload.names));
  return hearing;
}

// The lines `text` writes.
const linesOf = (text) => text.split("\n").length;

// A line's runs with a parse's spans laid over them, each span's class from its first col to its
// last. A parse that colors every col stands in for the runs whole.
function overlay(runs, spans, names, whole) {
  if (whole) {
    return spans.length ? spans.map(([from, , cls]) => [from, names[cls] ?? ""]) : [[0, ""]];
  }
  if (!spans.length) {
    return runs;
  }
  const classAt = (col) => {
    let name = "";
    for (const [start, cls] of runs) {
      if (start > col) {
        break;
      }
      name = cls;
    }
    return name;
  };
  const out = [];
  let col = 0;
  for (const [from, to, cls] of spans) {
    if (from > col) {
      out.push([col, classAt(col)]);
      for (const [start, name] of runs) {
        if (start > col && start < from) {
          out.push([start, name]);
        }
      }
    }
    out.push([from, names[cls] ?? ""]);
    col = to;
  }
  out.push([col, classAt(col)]);
  for (const [start, name] of runs) {
    if (start > col) {
      out.push([start, name]);
    }
  }
  return out;
}

// A line's runs as they came, a column then a name for each, read in the names' classes.
function classed(flat, classes) {
  const runs = [];
  for (let at = 0; at + 1 < flat.length; at += 2) {
    runs.push([flat[at], classes[flat[at + 1]] ?? ""]);
  }
  return runs;
}

export class Highlight {
  // `windowAt` gives, for a text read a window at a time, the file's number of the window's first line, and null for a
  // text read whole.
  constructor(doc, grammar, colored = null, windowAt = () => null) {
    this.doc = doc;
    this.grammar = grammar;
    // The file's parse, where one reads it: each line's spans by the line, the names of its classes,
    // whether it colors every col, the lines asked for, its folds, and whether the text changed since.
    this.parse = null;
    this.runs = [];
    this.classes = [""];
    this.colored = colored;
    this.windowAt = windowAt;
    // The key the text is held under on orior's side, null where it is held nowhere; the changes the page made to
    // it, counted, and those orior's side had not colored the text after when its last colors came, their lines
    // counted from the file's first; and the view told it last.
    this.key = null;
    this.seq = 0;
    this.changes = [];
    this.viewTold = null;
  }

  // The file's number of the first line held.
  base() {
    return this.windowAt() ?? 0;
  }

  // The text held on orior's side under `key`: the names its grammar's tokens take, and the colors that came with it.
  held(key, names, colors) {
    this.key = key;
    held.set(key, this);
    heard();
    this.named(names);
    this.put(0, colors ?? []);
  }

  // The text held nowhere from here on: no colors come for it.
  letGo() {
    held.delete(this.key);
    this.key = null;
  }

  // The names the grammar's tokens take; the runs kept show as they were until those of the grammar come.
  named(names) {
    this.classes = (names ?? [""]).map(classOf);
  }

  // Colors orior's side worked out as the text stood after its change `seq`, `[{ from, runs }]`, each line's runs kept
  // as they came where its line stands now, moved through the changes made since, a line one of them wrote or one
  // outside the lines held left out; the first colors after the text's grammar changed come with its names.
  put(seq, colors, names = null) {
    if (names) {
      this.named(names);
    }
    this.changes = this.changes.filter((change) => change.seq > seq);
    const base = this.base();
    for (const { from, runs } of colors) {
      runs.forEach((line, at) => {
        const placed = this.placed(from + at);
        const held = placed === null ? -1 : placed - base;
        if (held >= 0 && held < this.doc.count) {
          this.runs[held] = line;
        }
      });
    }
    this.colored?.();
  }

  // Where the line `line` of the text, as it stood before the changes kept, stands now: null where one of them wrote
  // it. The edits of one change are each placed against the text as it stood before that change.
  placed(line) {
    for (const { edits } of this.changes) {
      let moved = line;
      for (const { from, to, text } of edits) {
        if (line >= from.line && line <= to.line) {
          return null;
        }
        if (line > to.line) {
          moved += linesOf(text) - 1 - (to.line - from.line);
        }
      }
      line = moved;
    }
    return line;
  }

  // A change the page made, its edits as the document writes them: the lines each wrote take the place of those it
  // took out, the first showing the runs of the line it began on until its own come, and the lines after move with
  // them.
  changed(edits) {
    for (const { from, to, text } of [...edits].reverse()) {
      const count = linesOf(text);
      const removed = to.line - from.line + 1;
      if (count <= SPREAD) {
        const written = new Array(count);
        written[0] = this.runs[from.line];
        this.runs.splice(from.line, removed, ...written);
      } else {
        // a change of many lines takes the runs apart and puts them back, as one splice takes no more than SPREAD
        const written = new Array(count);
        written[0] = this.runs[from.line];
        this.runs = this.runs.slice(0, from.line).concat(written, this.runs.slice(from.line + removed));
      }
    }
    if (this.key) {
      const base = this.base();
      const moved = (place) => ({ line: place.line + base, col: place.col });
      this.seq += 1;
      this.changes.push({ seq: this.seq, edits: base ? edits.map((edit) => ({ ...edit, from: moved(edit.from), to: moved(edit.to) })) : edits });
    }
  }

  // The lines from `from` to `to` shown: where a line in them or within half the held reach of them has no runs yet,
  // orior's side is told the view, which it colors first, and the lines held, the view and HELD views' height on each
  // side, whose runs it hands on; the runs of the lines outside them are let go, and asked for again as the view comes
  // near them.
  shown(from, to) {
    if (!this.key) {
      return;
    }
    const count = this.doc.count;
    const reach = (to - from + 1) * HELD;
    const near = Math.floor(reach / 2);
    for (let line = Math.max(0, from - near); line <= Math.min(to + near, count - 1); line += 1) {
      if (!this.runs[line]) {
        const base = this.base();
        const heldFrom = Math.max(0, from - reach);
        const heldTo = Math.min(count, to + 1 + reach);
        const view = `${from + base}|${to + base}|${heldFrom + base}|${heldTo + base}`;
        if (this.viewTold !== view) {
          this.viewTold = view;
          this.kept(heldFrom, heldTo);
          invoke("branch_view", { key: this.key, first: from + base, last: to + base, from: heldFrom + base, to: heldTo + base }).catch(() => {});
        }
        return;
      }
    }
  }

  // The colors orior's side gave for lines asked of it, `[{ from, runs }]`, each line's runs in their classes by the
  // line as the page counts it, as a print reads them.
  classedFrom(colors) {
    const base = this.base();
    const lines = new Map();
    for (const { from, runs } of colors ?? []) {
      runs.forEach((line, at) => lines.set(from + at - base, classed(line, this.classes)));
    }
    return lines;
  }

  // The runs of the lines from `from` to before `to` kept and every other line's let go.
  kept(from, to) {
    if (this.runs.length > to) {
      this.runs.length = to;
    }
    this.runs.fill(undefined, 0, Math.min(from, this.runs.length));
  }

  // Every line's runs let go, as a text read a window at a time puts its window elsewhere; those of the lines it holds
  // come again as they are shown.
  cleared() {
    this.runs = [];
    this.viewTold = null;
  }

  // Lines put in above the first, and lines let go above the first, of a text read a window at a time.
  shift(added) {
    this.runs = new Array(added).concat(this.runs);
  }

  unshift(removed) {
    this.runs = this.runs.slice(removed);
  }

  // Every line from this one on forgotten, as a text read a window at a time reads more.
  forget(line) {
    if (this.runs.length > line) {
      this.runs.length = line;
    }
  }

  // A line's runs with its parse laid over them, where the parse holds the line.
  laid(line, runs) {
    const spans = this.parse?.byLine.get(line);
    return spans ? overlay(runs ?? PLAIN, spans, this.parse.names, this.parse.whole) : runs;
  }

  // A line's tokens where they have come, or null.
  cached(line) {
    if (this.parse?.whole && this.parse.byLine.has(line)) {
      return this.laid(line, PLAIN);
    }
    const flat = this.runs[line];
    return flat ? this.laid(line, classed(flat, this.classes)) : (this.laid(line, undefined) ?? null);
  }

  // A line's tokens: those that came, or none.
  runsOf(line) {
    const flat = this.runs[line];
    return this.laid(line, flat ? classed(flat, this.classes) : PLAIN);
  }

  // The class of the token at a place.
  classAt(line, col) {
    let name = "";
    for (const [start, cls] of this.runsOf(line)) {
      if (start > col) {
        break;
      }
      name = cls;
    }
    return name;
  }
}
