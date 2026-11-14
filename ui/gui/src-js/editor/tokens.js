// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tokens of a text's lines as orior's side colors them, retained as they come: each line's runs one flat list, a column
// then a name for each run, read into classes as the line is drawn. A branch stored there comes with the runs of the
// lines in view; the page mirrors the runs of the lines near its view alone, those come as orior's side walks the text
// out from the view, and more are asked for as the view moves; the lines a change wrote come once it colors them again.
// The page colors nothing: a line a change wrote shows the runs of the line the change began on until its own come. A
// text read a window at a time counts its lines from the file's first, as orior's side does.

import { invoke, listen } from "../bridge.js";
import { classOf } from "./grammar.js";

export { classOf, compile, tokenKinds: tokenTypes } from "./grammar.js";

const PLAIN = [[0, ""]];

// The max lines one splice puts in as its arguments.
const SPREAD = 1024;

// How many views' height of lines on each side of the view the page mirrors the runs of.
const REACH = 2;

// Each branch on orior's side the page mirrors, by its key, and the colors it passes on heard once; and the folds it
// passed on for a branch before the page came to mirror it, the last by its key, taken as the mirror begins.
const mirrored = new Map();
const early = new Map();
let hearing = null;

// The branches' colors and folds heard from here on, before a branch is asked for, so none it passes on is missed.
export function hearBranches() {
  return heard();
}

function heard() {
  hearing ??= listen("branch-colors", ({ payload }) => {
    const mirror = mirrored.get(payload?.key);
    if (payload?.folds && mirror) {
      mirror.folded(payload.seq, payload.folds);
    } else if (payload?.folds) {
      early.set(payload.key, payload);
    } else {
      mirror?.put(payload.seq, payload.colors, payload.names);
    }
  });
  return hearing;
}

// The lines `text` writes.
const linesOf = (text) => text.split("\n").length;

// A line's runs with a parse's spans positioned over them, each span's class from its first col to its
// last. A parse that colors every col is in for the runs whole.
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

// Whether a line's runs as they came, `one`, are the runs `other`, column for column and name for name.
function sameRuns(one, other) {
  return Boolean(one) && one.length === other.length && one.every((value, at) => value === other[at]);
}

// A line's runs as they came, a column then a name for each, read in the names' classes.
export function classed(flat, classes) {
  const runs = [];
  for (let at = 0; at + 1 < flat.length; at += 2) {
    runs.push([flat[at], classes[flat[at + 1]] ?? ""]);
  }
  return runs;
}

// The text of a line cut at its runs, `[class, text]` for each run that takes a character of it.
export function partsOf(text, runs) {
  const parts = [];
  runs.forEach(([start, name], index) => {
    const end = runs[index + 1]?.[0] ?? text.length;
    if (end > start) {
      parts.push([name, text.slice(start, end)]);
    }
  });
  return parts;
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
    // Each line's runs read in the names' classes, retained by the runs as they came, so that runs read twice in the same
    // classes are the same runs and what is drawn from them is drawn once.
    this.classedOf = new WeakMap();
    this.colored = colored;
    this.windowAt = windowAt;
    // The key of the branch on orior's side the text mirrors, null where it mirrors none; the changes the page made to
    // it, counted, and those orior's side had not colored the text after when its last colors came, their lines
    // counted from the file's first; and the view told it last.
    this.key = null;
    this.seq = 0;
    this.changes = [];
    this.viewTold = null;
    // The lines mirrored as orior's side was last told them, the characters of each line it colors as it was last told
    // them, 0 for every one, and how many times runs have come, for what draws from them to know it has more to draw.
    this.mirrorTold = null;
    this.columnsTold = 0;
    this.came = 0;
    // The folds orior's side read off the regions its grammar's roles open and close, by the line each opens on: the
    // last line it hides and whether it shows the line it closes on; and how many times they have come.
    this.folds = new Map();
    this.foldsCame = 0;
  }

  // The file's number of the first line the page has.
  base() {
    return this.windowAt() ?? 0;
  }

  // The text the mirror of the branch on orior's side under `key`: the names its grammar's tokens take, the colors
  // that came with it, and the characters of each line those colors reach, 0 for every one.
  mirrors(key, names, colors, columns = 0) {
    this.key = key;
    this.columnsTold = columns;
    mirrored.set(key, this);
    heard();
    this.named(names);
    this.put(0, colors ?? []);
    const waiting = early.get(key);
    if (waiting) {
      early.delete(key);
      this.folded(waiting.seq, waiting.folds);
    }
  }

  // The text the mirror of no branch from here on: no colors come for it.
  letGo() {
    mirrored.delete(this.key);
    this.key = null;
  }

  // The names the grammar's tokens take; the runs retained show as they were until those of the grammar come.
  named(names) {
    this.classes = (names ?? [""]).map(classOf);
  }

  // Colors orior's side worked out as the text was after its change `seq`, `[{ from, runs }]`, each line's runs retained
  // as they came where its line is now, moved through the changes made since, a line one of them wrote or one
  // outside the lines mirrored left out, and runs the same as those a line retains leaving those it retains, so what is
  // drawn from them stays drawn; the first colors after the text's grammar changed come with its names.
  put(seq, colors, names = null) {
    if (names) {
      this.named(names);
    }
    this.changes = this.changes.filter((change) => change.seq > seq);
    const base = this.base();
    for (const { from, runs } of colors) {
      runs.forEach((line, at) => {
        const placed = this.placed(from + at);
        const here = placed === null ? -1 : placed - base;
        if (here >= 0 && here < this.doc.count && !sameRuns(this.runs[here], line)) {
          this.runs[here] = line;
        }
      });
    }
    this.came += 1;
    this.colored?.();
  }

  // Folds orior's side read off the text as it was after its change `seq`, `[[first, last, displayed]]`, each placed where
  // its lines are now: a fold one of the changes since wrote a line of left out.
  folded(seq, folds) {
    this.changes = this.changes.filter((change) => change.seq > seq);
    const base = this.base();
    const retained = new Map();
    for (const [first, last, displayed] of folds) {
      const start = this.placed(first);
      const end = this.placed(last);
      if (start !== null && end !== null && start - base >= 0 && end - base < this.doc.count) {
        retained.set(start - base, [end - base, displayed === 1]);
      }
    }
    this.folds = retained;
    this.foldsCame += 1;
    this.colored?.();
  }

  // The colors of lines read with their text, `[{ from, runs }]`, put where those lines are, runs the same as those a
  // line retains leaving those it retains: they are the colors of the text as it is, read after every change before them,
  // and come before the lines are drawn.
  given(colors) {
    if (!colors?.length) {
      return;
    }
    const base = this.base();
    for (const { from, runs } of colors) {
      runs.forEach((line, at) => {
        const here = from + at - base;
        if (here >= 0 && here < this.doc.count && !sameRuns(this.runs[here], line)) {
          this.runs[here] = line;
        }
      });
    }
    this.came += 1;
  }

  // Where the line `line` of the text, as it was before the changes retained, is now: null where one of them wrote
  // it. The edits of one change are each placed against the text as it was before that change.
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

  // The lines from `from` to `to` displayed: where a line in them or within half the mirror's reach of them has no runs
  // yet, orior's side is told the view, which it colors first, and the lines mirrored, the view and REACH views' height
  // on each side, whose runs it passes on; the runs of the lines outside them are let go, and asked for again as the view
  // comes near them. A text read a window at a time mirrors its whole window, the lines near the view alone, each read
  // with its colors, and orior's side is told the window again each time it moves. `drawn` is how far across the view
  // draws each line, Infinity for every character: a view drawing farther than the colors reach has orior's side color the
  // lines it mirrors as far again.
  displayed(from, to, drawn = Infinity) {
    if (!this.key) {
      return;
    }
    const count = this.doc.count;
    const reach = (to - from + 1) * REACH;
    const near = Math.floor(reach / 2);
    const windowed = this.windowAt() !== null;
    let lacking = false;
    for (let line = Math.max(0, from - near); line <= Math.min(to + near, count - 1) && !lacking; line += 1) {
      lacking = !this.runs[line];
    }
    const columns = Number.isFinite(drawn) ? drawn : 0;
    const farther = this.columnsTold !== 0 && (columns === 0 || columns > this.columnsTold);
    const base = this.base();
    const mirrorFrom = windowed ? 0 : Math.max(0, from - reach);
    const mirrorTo = windowed ? count : Math.min(count, to + 1 + reach);
    const mirror = `${mirrorFrom + base}|${mirrorTo + base}`;
    if (!lacking && !farther && !(windowed && mirror !== this.mirrorTold)) {
      return;
    }
    const view = `${from + base}|${to + base}|${mirror}|${farther ? columns : this.columnsTold}`;
    if (this.viewTold !== view) {
      this.viewTold = view;
      this.mirrorTold = mirror;
      this.retained(mirrorFrom, mirrorTo);
      const told = { key: this.key, first: from + base, last: to + base, from: mirrorFrom + base, to: mirrorTo + base };
      if (farther) {
        this.columnsTold = columns;
        told.columns = columns;
      }
      invoke("branch_view", told).catch(() => {});
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

  // The runs of the lines from `from` to before `to` retained and every other line's let go.
  retained(from, to) {
    if (this.runs.length > to) {
      this.runs.length = to;
    }
    this.runs.fill(undefined, 0, Math.min(from, this.runs.length));
  }

  // Every line's runs let go, as a text read a window at a time puts its window elsewhere; those of the lines it holds
  // come with them.
  cleared() {
    this.runs = [];
    this.folds = new Map();
    this.viewTold = null;
    this.mirrorTold = null;
    this.came += 1;
  }

  // Lines put in above the first, and lines let go above the first, of a text read a window at a time, the folds moving
  // with their lines and those of the lines let go let go with them.
  shift(added) {
    this.runs = new Array(added).concat(this.runs);
    this.folds = new Map([...this.folds].map(([start, [end, displayed]]) => [start + added, [end + added, displayed]]));
  }

  unshift(removed) {
    this.runs = this.runs.slice(removed);
    this.folds = new Map([...this.folds].filter(([start]) => start >= removed).map(([start, [end, displayed]]) => [start - removed, [end - removed, displayed]]));
  }

  // Every line from this one on forgotten, as a text read a window at a time reads more.
  forget(line) {
    if (this.runs.length > line) {
      this.runs.length = line;
    }
  }

  // A line's runs with its parse positioned over them, where the parse holds the line.
  positioned(line, runs) {
    const spans = this.parse?.byLine.get(line);
    return spans ? overlay(runs ?? PLAIN, spans, this.parse.names, this.parse.whole) : runs;
  }

  // The runs `flat`, as they came, read in the names' classes: the same runs each time they are read in the same classes.
  classedRuns(flat) {
    const retained = this.classedOf.get(flat);
    if (retained && retained.classes === this.classes) {
      return retained.runs;
    }
    const runs = classed(flat, this.classes);
    this.classedOf.set(flat, { classes: this.classes, runs });
    return runs;
  }

  // A line's tokens where they have come, or null.
  cached(line) {
    if (this.parse?.whole && this.parse.byLine.has(line)) {
      return this.positioned(line, PLAIN);
    }
    const flat = this.runs[line];
    return flat ? this.positioned(line, this.classedRuns(flat)) : (this.positioned(line, undefined) ?? null);
  }

  // A line's tokens: those that came, or none.
  runsOf(line) {
    const flat = this.runs[line];
    return this.positioned(line, flat ? this.classedRuns(flat) : PLAIN);
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
