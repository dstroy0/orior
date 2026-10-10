// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// One open file as the editor holds it: its text, its language and colors, its selections, what is
// folded, where it is scrolled to, and a snippet being filled in. A tab keeps its session, and
// coming back to a tab finds everything as it was left.
//
// A large file is held as a window of its lines that grows outward, above and below, as the rest
// is read. `base` is the number in the file of the window's first line, and `window` the bytes
// read so far, { start, end, size }, or null once the file is whole.

import { Doc, mapThrough, pos } from "./document.js";
import { indentOf, opens, regionEnd, regions } from "./folding.js";
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
    this.found = new Map();
    this.foundAt = -1;
    // The indentation read from the lines, under any a mode line of the file sets, which is kept
    // apart for what an .editorconfig sets not to go over it.
    this.indentSet = modeIndent(this.doc);
    this.indent = { ...detectIndent(this.doc), ...this.indentSet };
    this.snippet = null;
    this.view = null;
    this.unwatch = this.doc.watch(({ edits, first }) => {
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
      this.highlight.forget(first);
      if (this.depths && this.depths.length > first + 1) {
        this.depths.length = first + 1;
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
  }

  // Colors the text in `language` from here on, as a plugin read again asks.
  setLanguage(language) {
    this.language = language ?? null;
    this.highlight = new Highlight(this.doc, this.language?.grammar ?? null, () => this.view?.schedule());
  }

  // Every region that folds. It reads the whole text, and only folding everything asks for it.
  regions() {
    return regions(this.doc, this.indent.size);
  }

  // Whether a line opens a region, read from the lines just after it.
  opens(line) {
    return opens(this.doc, line, this.indent.size);
  }

  // The last line of the region a line opens, or -1: the kept end where the region is folded, and
  // otherwise read from the lines after it and kept until the text changes.
  endOf(line) {
    const folded = this.folded.get(line);
    if (folded !== undefined) {
      return folded;
    }
    if (this.foundAt !== this.doc.id) {
      this.found = new Map();
      this.foundAt = this.doc.id;
    }
    let end = this.found.get(line);
    if (end === undefined) {
      end = regionEnd(this.doc, line, this.indent.size);
      this.found.set(line, end);
    }
    return end;
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
    this.foundAt = -1;
    this.top += added * (this.view?.lineHeight ?? 0);
    this.view?.grown(this, added, parts.slice(0, added));
  }
}
