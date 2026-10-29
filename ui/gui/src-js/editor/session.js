// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// One open file as the editor holds it: its text, its language and colors, its selections, what is
// folded, where it is scrolled to, and a snippet being filled in. A tab keeps its session, and
// coming back to a tab finds everything as it was left.
//
// A large file is held as a window of its lines that grows outward, above and below, as the rest
// is read. `base` is the number in the file of the window's first line, and `window` the bytes
// read so far, { start, end, size }, or null once the file is whole.

import { invoke } from "../bridge.js";
import { Doc, mapThrough, pos } from "./document.js";
import { indentOf } from "./folding.js";
import { Highlight } from "./tokens.js";

// Tabs where more indented lines open with a tab than with spaces, and otherwise the width that
// most often steps one line's indent to the next.
function detectIndent(doc) {
  let tabs = 0;
  let spaces = 0;
  const steps = new Map();
  let last = 0;
  for (let line = 0; line < Math.min(doc.count, 4000); line += 1) {
    const text = doc.line(line);
    if (!text.trim()) {
      continue;
    }
    if (text[0] === "\t") {
      tabs += 1;
    } else if (text[0] === " ") {
      spaces += 1;
    }
    const width = indentOf(text, 4);
    const step = Math.abs(width - last);
    if (text[0] !== "\t" && step >= 2 && step <= 8) {
      steps.set(step, (steps.get(step) ?? 0) + 1);
    }
    last = width;
  }
  let size = 4;
  let most = 0;
  for (const [step, count] of steps) {
    if (count > most && [2, 4, 8].includes(step)) {
      size = step;
      most = count;
    }
  }
  return { tabs: tabs > spaces, size: tabs > spaces ? 4 : size };
}

// How many lines at each end of a file a mode line is looked for in, as Vim looks.
const MODE_LINES = 5;

// The indentation a line of the file sets for it, as Vim and Emacs read one among its first and last
// lines: `vim: set ts=4 sw=4 et:`, sw or ts the width and et or noet spaces or tabs, and
// `-*- indent-tabs-mode: nil; tab-width: 4 -*-`. Holds only what the line sets.
export function modeIndent(doc) {
  const set = {};
  const lines = new Set();
  for (let at = 0; at < Math.min(MODE_LINES, doc.count); at += 1) {
    lines.add(at);
    lines.add(doc.count - 1 - at);
  }
  for (const line of lines) {
    const text = doc.line(line);
    const vim = text.match(/(?:^|\s)(?:vi|vim|ex):\s*(?:set?\s+)?(.*)/);
    for (const option of vim ? vim[1].split(/[\s:]+/) : []) {
      const [name, value] = option.split("=");
      const width = Number(value);
      if (name === "et" || name === "expandtab") {
        set.tabs = false;
      } else if (name === "noet" || name === "noexpandtab") {
        set.tabs = true;
      } else if ((name === "sw" || name === "shiftwidth") && width > 0) {
        set.size = width;
      } else if ((name === "ts" || name === "tabstop") && width > 0) {
        set.size ??= width;
      }
    }
    const emacs = text.match(/-\*-(.*)-\*-/);
    for (const part of emacs ? emacs[1].split(";") : []) {
      const [name, value] = part.split(":").map((one) => one?.trim());
      if (name === "indent-tabs-mode") {
        set.tabs = value !== "nil";
      } else if (name === "tab-width" && Number(value) > 0) {
        set.size = Number(value);
      }
    }
  }
  return set;
}

const FAR = Number.MAX_SAFE_INTEGER;

export class Session {
  // A session given `doc` holds that text with another session, each with its own selections, folds
  // and place, as a split editor shows one file twice.
  constructor(text, language, { base = 0, window = null, readOnly = false, doc = null } = {}) {
    this.doc = doc ?? new Doc(text);
    // A session that is read only takes no edit, and nothing it holds is ever written.
    this.readOnly = readOnly;
    this.setLanguage(language);
    this.selections = [{ anchor: pos(0, 0), head: pos(0, 0), goal: null }];
    this.primary = 0;
    this.top = 0;
    this.left = 0;
    this.base = base;
    this.window = window;
    // A folded region keeps the end it had when folded, carried through every edit, and nothing
    // reads its lines again while it stays folded.
    this.folded = new Map();
    this.foldings = 0;
    // The indentation read from the lines, under any a mode line of the file sets, which is kept
    // apart for what an .editorconfig sets not to go over it.
    this.indentSet = modeIndent(this.doc);
    this.indent = { ...detectIndent(this.doc), ...this.indentSet };
    this.snippet = null;
    this.view = null;
    this.unwatch = this.doc.watch(({ edits }) => {
      // A text two sessions hold is written by one view at a time. Every session but the writer has
      // its selections carried through what was written and its view drawn again.
      const writer = this.doc.writer;
      if (this.shared && writer !== this) {
        if (writer || this.of) {
          // A cursor with nothing chosen stays one, carried to the end of what was written at it.
          this.selections = this.selections.map((sel) => {
            const empty = sel.anchor.line === sel.head.line && sel.anchor.col === sel.head.col;
            return { anchor: mapThrough(sel.anchor, edits, empty), head: mapThrough(sel.head, edits, true), goal: null };
          });
        }
        this.view?.schedule();
      }
      // A twin's colors and folds are the session's it was made from, and they take each change once; orior's side's
      // folds stay with their lines until it hands them on again.
      if (!this.of) {
        this.highlight.changed(edits);
        const carried = new Map();
        for (const [start, [end, shown]] of this.highlight.folds) {
          const from = mapThrough(pos(start, 0), edits, true).line;
          const to = mapThrough(pos(end, FAR), edits, true).line;
          if (to > from) {
            carried.set(from, [to, shown]);
          }
        }
        this.highlight.folds = carried;
      }
      if (this.folded.size) {
        const carried = new Map();
        for (const [start, end] of this.folded) {
          const from = mapThrough(pos(start, FAR), edits, true).line;
          const to = mapThrough(pos(end, FAR), edits, true).line;
          if (to > from) {
            carried.set(from, to);
          }
        }
        this.folded = carried;
        this.foldings += 1;
      }
      // A parse's spans and folds stay with their lines where no edit reached them, and wait to be
      // asked for again.
      const parse = this.highlight.parse;
      if (parse) {
        const reached = (line) => edits.some((edit) => edit.from.line <= line && line <= edit.to.line);
        const moved = (line) => mapThrough(pos(line, 0), edits, true).line;
        const byLine = new Map();
        for (const [line, spans] of parse.byLine) {
          if (!reached(line)) {
            byLine.set(moved(line), spans);
          }
        }
        const folds = new Map();
        for (const [start, end] of parse.folds) {
          const from = moved(start);
          const to = mapThrough(pos(end, FAR), edits, true).line;
          if (to > from) {
            folds.set(from, to);
          }
        }
        this.highlight.parse = { ...parse, byLine, folds, stale: true };
      }
      // A hint stays with its line where no edit reached the line, and waits to be asked for again.
      if (this.hints) {
        const carried = new Map();
        for (const [line, hints] of this.hints.byLine) {
          if (!edits.some((edit) => edit.from.line <= line && line <= edit.to.line)) {
            carried.set(mapThrough(pos(line, 0), edits, true).line, hints);
          }
        }
        this.hints = { ...this.hints, byLine: carried, stale: true };
      }
      if (this.snippet) {
        for (const stop of this.snippet.stops) {
          stop.from = mapThrough(stop.from, edits, false);
          stop.to = mapThrough(stop.to, edits, true);
        }
      }
    });
  }

  // A session of this one's text, its colors and indent the same, its selections and place a
  // copy of these. `of` is the session it was made from. A file still being read has none.
  twin() {
    const made = new Session("", this.language, { base: this.base, readOnly: this.readOnly, doc: this.doc });
    made.of = this;
    // What the app marks on a file, and how it reads and colors it, is the first session's.
    for (const name of ["language", "highlight", "indent", "readOnly", "diagnostics", "changes"]) {
      Object.defineProperty(made, name, { get: () => this[name], set: () => {}, configurable: true });
    }
    made.selections = this.selections.map((sel) => ({ ...sel }));
    made.primary = this.primary;
    made.top = this.view ? this.view.scrollY() / this.view.lineHeight : this.top;
    made.left = this.view ? this.view.scroller.scrollLeft : this.left;
    this.shared = true;
    made.shared = true;
    return made;
  }

  // Stops following the text, as a twin closed asks.
  drop() {
    this.unwatch();
    if (this.of) {
      this.of.shared = false;
    }
    // a branch a view mirrors for itself alone let go
    this.release?.();
    this.release = null;
  }

  // Colors the text in `language` from here on, as a plugin read again asks: a text the window's side holds is
  // colored there again, its colors coming as they are worked out.
  setLanguage(language) {
    this.language = language ?? null;
    const key = this.highlight?.key;
    if (key) {
      invoke("branch_language", { key, id: this.language?.id ?? "" }).catch(() => {});
      return;
    }
    this.highlight = this.highlighted();
  }

  // The colors of the text, read in the language set, a window's lines counted from the file's first.
  highlighted() {
    return new Highlight(this.doc, this.language?.grammar ?? null, () => this.view?.schedule(), () => (this.window ? this.base : null));
  }

  // Every region that folds, from the line it opens on to the last line its fold hides: those orior's side reads off
  // the regions the grammar's roles open and close, the headings' and blocks' the language marks, and the parse's,
  // each where none before it opens on the line.
  regions() {
    const found = new Map();
    for (const [line, end] of this.highlight.parse?.folds ?? []) {
      found.set(line, end);
    }
    for (const [line, end] of this.sectionFolds() ?? []) {
      found.set(line, end);
    }
    for (const [line, [end]] of this.highlight.folds) {
      found.set(line, end);
    }
    return found;
  }

  // Whether the fold of the region that opens on `line` shows, after what it hides, the line it closes on: orior's
  // side says so where the line holds closes alone and the fold hides it.
  shows(line) {
    const given = this.highlight.folds.get(line);
    return Boolean(given?.[1]) && this.folded.get(line) === given[0];
  }

  // The lines the regions `line` stands inside open on, the outermost first and the `most` innermost at most.
  openersOf(line, most) {
    const found = [];
    for (const [start, end] of this.regions()) {
      if (start < line && line <= end) {
        found.push(start);
      }
    }
    return found.sort((one, other) => one - other).slice(-most);
  }

  // The folds of the headings and blocks the language marks, where it marks any, kept until the text
  // changes.
  sectionFolds() {
    if (!this.language?.folds) {
      return null;
    }
    if (this.sectionsAt !== this.doc.id || this.sectionsOf !== this.language) {
      this.sections = this.language.folds(this.doc);
      this.sectionsAt = this.doc.id;
      this.sectionsOf = this.language;
    }
    return this.sections;
  }

  // Whether a line opens a region: by orior's side's folds, the language's headings and blocks, or the parse.
  opens(line) {
    return this.endOf(line) > line;
  }

  // The last line the fold of the region a line opens hides, or -1: the kept end where the region is folded, else
  // orior's side's, the language's or the parse's, the first that gives one.
  endOf(line) {
    const folded = this.folded.get(line);
    if (folded !== undefined) {
      return folded;
    }
    const given = this.highlight.folds.get(line)?.[0] ?? this.sectionFolds()?.get(line) ?? this.highlight.parse?.folds?.get(line);
    return given > line ? given : -1;
  }

  fold(start) {
    const end = this.endOf(start);
    if (end > start) {
      this.folded.set(start, end);
    }
  }

  // Lines read from the file above the window or below it. Neither is an edit: the history and the
  // dirty mark stay as they are, and every place held moves down by the lines written above it.
  grow(text, above) {
    const doc = this.doc;
    const parts = text.split(/\r?\n/);
    if (!above) {
      const last = doc.lines.length - 1;
      doc.lines[last] += parts[0];
      for (let index = 1; index < parts.length; index += 1) {
        doc.lines.push(parts[index]);
      }
      this.highlight.forget(last);
      this.view?.grown(this, 0, parts.slice(1));
      return;
    }
    this.grownAbove(parts);
  }

  // The lines `parts` put in above the window's first line, the last joined to the start of that line.
  grownAbove(parts, colors = null) {
    const doc = this.doc;
    const added = parts.length - 1;
    parts[added] += doc.lines[0];
    doc.lines = parts.concat(doc.lines.slice(1));
    doc.shift(added);
    this.base -= added;
    const down = (p) => pos(p.line + added, p.col);
    this.selections = this.selections.map((sel) => ({ anchor: down(sel.anchor), head: down(sel.head), goal: sel.goal }));
    this.folded = new Map([...this.folded].map(([start, end]) => [start + added, end + added]));
    this.foldings += 1;
    if (this.snippet) {
      this.snippet.stops = this.snippet.stops.map((stop) => ({ from: down(stop.from), to: down(stop.to) }));
    }
    this.highlight.shift(added);
    this.highlight.given(colors);
    this.top += added * (this.view?.lineHeight ?? 0);
    this.view?.grown(this, added, parts.slice(0, added));
  }

  // Whole lines of a paged file read in above the window or below it, as `grow` reads a slice in, with `colors`, the
  // colors read with them, put before they are drawn.
  growLines(lines, above, colors = null) {
    if (!lines.length) {
      return;
    }
    this.view?.holdLine(this);
    if (!above) {
      const last = this.doc.lines.length - 1;
      this.doc.lines.push(...lines);
      this.highlight.forget(last + 1);
      this.highlight.given(colors);
      this.view?.grown(this, 0, lines);
      return;
    }
    this.grownAbove(lines.concat(""), colors);
  }

  // Lets go of `count` lines of a paged file's window, above it or below it. Neither is an edit; the
  // history goes with them where any step of it reaches one, and every place held in the window
  // moves up by the lines let go above it.
  shrink(count, above) {
    const doc = this.doc;
    count = Math.min(count, doc.count - 1);
    if (count <= 0) {
      return;
    }
    this.view?.holdLine(this);
    const kept = above ? [count, doc.count - 1] : [0, doc.count - 1 - count];
    if (doc.reaches((line) => line < kept[0] || line > kept[1])) {
      doc.forgetSteps();
    }
    const clamp = (p) => doc.clamp(pos(Math.max(kept[0], Math.min(kept[1], p.line)) - kept[0], p.col));
    if (!above) {
      doc.lines.length -= count;
      this.selections = this.selections.map((sel) => ({ anchor: clamp(sel.anchor), head: clamp(sel.head), goal: null }));
      this.folded = new Map([...this.folded].filter(([, end]) => end < doc.count));
      this.foldings += 1;
      this.highlight.forget(doc.count);
      this.view?.shrunk(this, count, false);
      return;
    }
    const before = this.selections;
    doc.lines = doc.lines.slice(count);
    this.selections = before.map((sel) => ({ anchor: clamp(sel.anchor), head: clamp(sel.head), goal: null }));
    doc.shift(-count);
    this.base += count;
    this.folded = new Map([...this.folded].filter(([start]) => start >= count).map(([start, end]) => [start - count, end - count]));
    this.foldings += 1;
    this.snippet = null;
    this.highlight.unshift(count);
    this.top = Math.max(0, this.top - count * (this.view?.lineHeight ?? 0));
    this.view?.shrunk(this, count, true);
  }

  // Puts the window of a paged file at `base`, holding `lines`, its line `top` at the top of the
  // view, in place of the one it held: the history, the folds and the colors worked out go with the
  // lines let go, and `colors`, those read with the lines, are put before they are drawn. Where `top`
  // is null the view stays on the line it stands on, as a window put where the view went.
  replaceWindow(lines, base, top = 0, colors = null) {
    if (top === null) {
      this.view?.holdLine(this);
    }
    // each selection carried to the same line of the file where the new window holds it, else to the window's start
    const shift = this.base - base;
    const last = Math.max(0, lines.length - 1);
    const carried = (p) => (p.line + shift >= 0 && p.line + shift <= last ? pos(p.line + shift, p.col) : pos(0, 0));
    const selections = this.selections.map((sel) => ({ anchor: carried(sel.anchor), head: carried(sel.head), goal: sel.goal }));
    this.doc.lines = lines.length ? lines : [""];
    this.doc.forgetSteps();
    this.base = base;
    this.selections = selections.map((sel) => ({ anchor: this.doc.clamp(sel.anchor), head: this.doc.clamp(sel.head), goal: sel.goal }));
    this.folded = new Map();
    this.foldings += 1;
    this.snippet = null;
    this.highlight.cleared();
    this.highlight.given(colors);
    this.top = top ?? 0;
    this.view?.replaced(this);
  }
}
