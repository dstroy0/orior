// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find and replace, and going to a line: the two bars that open over the editor's top right.

import { cmp, pos } from "./document.js";

const MOST = 20000;
// Every search runs on the window's side, over the branch a text is held as or the pages a paged file is: by its sift,
// Horspool's shifts within one of the sift's windows and the sift's plan past it, where the query is sought as it is,
// and by the regular expressions the colorer reads where it is a pattern. The matches are drawn once the answer comes,
// each in the file's lines, a paged file's window placed among them.
let searching = null;

// Gives find the way to search on the window's side: `given.search(s, sought, most)` answers a promise of the matches
// in the file's lines as [line, from col, to col], or of null where the query does not read as a pattern, or null
// where the text of the session `s` is held nowhere; `given.reach(s, line)` answers a promise of the file's line `line`
// brought into the window of a paged session `s`.
export function findWith(given) {
  searching = given;
}

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children);
  return made;
}

const escapeRegex = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

export class Find {
  constructor(editor) {
    this.ed = editor;
    this.shown = false;
    this.matches = [];
    this.index = -1;
    this.options = { case: false, word: false, regex: false };
    this.query = element("input", { className: "ed-find-query", type: "text", placeholder: "Find", spellcheck: false });
    this.replacement = element("input", { className: "ed-find-query", type: "text", placeholder: "Replace", spellcheck: false });
    this.count = element("span", { className: "ed-find-count" });
    this.more = element("button", { type: "button", className: "ed-find-more", title: "Replace", textContent: "▸" });
    const option = (name, label, title) => {
      const button = element("button", { type: "button", className: "ed-find-option", textContent: label, title });
      button.dataset.option = name;
      button.addEventListener("click", () => this.toggle(name));
      return button;
    };
    this.optionButtons = [option("case", "Aa", "Match case"), option("word", "ab", "Whole word"), option("regex", ".*", "Regular expression")];
    const action = (label, title, run) => {
      const button = element("button", { type: "button", className: "ed-find-action", textContent: label, title });
      button.addEventListener("click", run);
      return button;
    };
    const findRow = element(
      "div",
      { className: "ed-find-row" },
      this.more,
      this.query,
      ...this.optionButtons,
      this.count,
      action("↑", "Previous", () => this.step(-1)),
      action("↓", "Next", () => this.step(1)),
      action("×", "Close", () => this.close())
    );
    this.replaceRow = element(
      "div",
      { className: "ed-find-row replace", hidden: true },
      this.replacement,
      action("Replace", "Replace", () => this.replaceOne()),
      action("All", "Replace all", () => this.replaceAll())
    );
    this.bar = element("div", { className: "ed-find", hidden: true }, findRow, this.replaceRow);
    this.bar.addEventListener("mousedown", (event) => event.stopPropagation());
    this.more.addEventListener("click", () => this.showReplace(this.replaceRow.hidden));
    editor.host.append(this.bar);
    this.query.addEventListener("input", () => {
      this.refresh()?.then((found) => found && this.jump(true));
    });
    this.query.addEventListener("keydown", (event) => this.key(event, false));
    this.replacement.addEventListener("keydown", (event) => this.key(event, true));
  }

  key(event, replacing) {
    const mod = event.ctrlKey || event.metaKey;
    if (event.key === "Escape") {
      event.preventDefault();
      this.close();
    } else if (event.key === "Enter" && replacing) {
      event.preventDefault();
      if (mod) {
        this.replaceAll();
      } else {
        this.replaceOne();
      }
    } else if (event.key === "Enter") {
      event.preventDefault();
      this.step(event.shiftKey ? -1 : 1);
    } else if (mod && event.code === "KeyH") {
      event.preventDefault();
      this.showReplace(this.replaceRow.hidden);
    } else if (mod && event.code === "KeyF") {
      event.preventDefault();
      this.query.select();
    } else if (event.altKey && ["KeyC", "KeyW", "KeyR"].includes(event.code)) {
      event.preventDefault();
      this.toggle({ KeyC: "case", KeyW: "word", KeyR: "regex" }[event.code]);
    } else if (event.key === "F3") {
      event.preventDefault();
      this.step(event.shiftKey ? -1 : 1);
    }
  }

  toggle(name) {
    this.options[name] = !this.options[name];
    this.optionButtons.forEach((button) => button.setAttribute("aria-pressed", String(this.options[button.dataset.option])));
    this.refresh();
  }

  showReplace(shown) {
    this.replaceRow.hidden = !shown;
    this.more.textContent = shown ? "▾" : "▸";
    if (shown) {
      this.replacement.focus();
    }
  }

  open(replace) {
    const ed = this.ed;
    if (!ed.s) {
      return;
    }
    const primary = ed.primary();
    const from = primary.anchor;
    const to = primary.head;
    if (from.line === to.line && from.col !== to.col) {
      this.query.value = ed.doc.slice(cmp(from, to) < 0 ? from : to, cmp(from, to) < 0 ? to : from);
    }
    this.shown = true;
    this.bar.hidden = false;
    this.showReplace(replace || !this.replaceRow.hidden);
    if (!replace || !this.query.value) {
      this.query.focus();
      this.query.select();
    }
    this.refresh();
  }

  close() {
    this.shown = false;
    this.bar.hidden = true;
    this.matches = [];
    this.ed.schedule();
    this.ed.focus();
  }

  // The query as a pattern, for a replacement's groups, or null where it is empty or does not compile.
  pattern(global = true) {
    const text = this.query.value;
    if (!text) {
      return null;
    }
    let source = this.options.regex ? text : escapeRegex(text);
    if (this.options.word) {
      source = `(?<![\\p{L}\\p{N}_$])(?:${source})(?![\\p{L}\\p{N}_$])`;
    }
    const flags = `${global ? "g" : ""}${this.options.case ? "" : "i"}`;
    // A pattern the unicode flag refuses, such as one with \- outside a class, compiles without it
    // where whole words, which need it, are not asked for.
    for (const unicode of this.options.word ? ["u"] : ["u", ""]) {
      try {
        return new RegExp(source, flags + unicode);
      } catch {
        continue;
      }
    }
    return null;
  }

  // The search asked of the window's side for the bar as it is set, the matches drawn kept until its answer comes: a
  // promise of whether the answer was the last asked, or null where nothing is asked.
  refresh() {
    const ed = this.ed;
    this.asking = null;
    if (!this.shown || !ed.s) {
      this.matches = [];
      return null;
    }
    const sought = { query: this.query.value, regex: this.options.regex, case: this.options.case, word: this.options.word };
    const asked = sought.query ? searching?.search(ed.s, sought, MOST) : null;
    if (!asked) {
      this.bar.dataset.bad = "false";
      this.found([]);
      return null;
    }
    this.asking = asked;
    return asked.then((found) => {
      if (this.asking !== asked) {
        return false;
      }
      this.asking = null;
      this.bar.dataset.bad = String(!found);
      this.found((found ?? []).map(([line, from, to]) => ({ from: pos(line, from), to: pos(line, to) })));
      return true;
    });
  }

  // The file's line of the session's first: a paged session's window's, 0 for a text held whole.
  base() {
    return this.ed.s?.base ?? 0;
  }

  // A match placed in the session's lines, or null where it stands outside a paged session's window.
  placed(match) {
    const base = this.base();
    const line = match.from.line - base;
    if (line < 0 || match.to.line - base >= this.ed.s.doc.count) {
      return null;
    }
    return { from: pos(line, match.from.col), to: pos(match.to.line - base, match.to.col) };
  }

  found(matches) {
    this.matches = matches;
    this.index = this.nearest(true);
    this.drawCount();
    this.ed.schedule();
  }

  // The match at or after the primary selection's start, or with `fromStart` off after its end, each in the file's
  // lines.
  nearest(fromStart, direction = 1) {
    if (!this.matches.length) {
      return -1;
    }
    const primary = this.ed.primary();
    const base = this.base();
    const anchor = pos(primary.anchor.line + base, primary.anchor.col);
    const head = pos(primary.head.line + base, primary.head.col);
    const start = cmp(anchor, head) < 0 ? anchor : head;
    const end = cmp(anchor, head) < 0 ? head : anchor;
    if (direction > 0) {
      const at = fromStart ? start : end;
      const found = this.matches.findIndex((match) => cmp(match.from, at) >= 0);
      return found < 0 ? 0 : found;
    }
    for (let index = this.matches.length - 1; index >= 0; index -= 1) {
      if (cmp(this.matches[index].from, start) < 0) {
        return index;
      }
    }
    return this.matches.length - 1;
  }

  drawCount() {
    const query = this.query.value;
    if (!query) {
      this.count.textContent = "";
    } else if (!this.matches.length) {
      this.count.textContent = "No results";
    } else {
      const more = this.matches.length >= MOST ? "+" : "";
      this.count.textContent = `${this.index + 1} of ${this.matches.length}${more}`;
    }
  }

  // The current match in the session's lines, null where there is none or it stands outside a paged session's window.
  current() {
    const match = this.matches[this.index];
    return match ? this.placed(match) : null;
  }

  // The matches that fall on the session's lines from first to last, in those lines.
  between(first, last) {
    const base = this.base();
    let low = 0;
    let high = this.matches.length;
    while (low < high) {
      const mid = (low + high) >> 1;
      if (this.matches[mid].to.line < first + base) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    const out = [];
    for (let index = low; index < this.matches.length && this.matches[index].from.line <= last + base; index += 1) {
      const placed = this.placed(this.matches[index]);
      if (placed) {
        out.push(placed);
      }
    }
    return out;
  }

  // The current match selected and shown, a paged session's window brought to it first where it stands outside it.
  async jump(fromStart) {
    const at = this.matches[this.index];
    if (!at) {
      return;
    }
    const ed = this.ed;
    const s = ed.s;
    if (!this.placed(at)) {
      await searching?.reach(s, at.from.line);
    }
    const match = ed.s === s && this.matches[this.index] === at ? this.placed(at) : null;
    if (!match) {
      return;
    }
    ed.setSelections([{ anchor: match.from, head: match.to, goal: null }]);
    ed.moved();
    ed.reveal(true);
    if (!fromStart) {
      ed.doc.seal();
    }
  }

  step(direction) {
    if (!this.shown) {
      this.open(false);
      return;
    }
    if (!this.matches.length) {
      return;
    }
    this.index = this.nearest(false, direction);
    this.drawCount();
    this.jump(false);
  }

  replacementFor(match) {
    const text = this.ed.doc.slice(match.from, match.to);
    if (!this.options.regex) {
      return this.replacement.value;
    }
    const pattern = this.pattern(false);
    return pattern ? text.replace(pattern, this.replacement.value) : this.replacement.value;
  }

  replaceOne() {
    const match = this.current();
    if (!match) {
      return;
    }
    const primary = this.ed.primary();
    const chosen = (cmp(primary.anchor, match.from) === 0 && cmp(primary.head, match.to) === 0) || (cmp(primary.head, match.from) === 0 && cmp(primary.anchor, match.to) === 0);
    if (chosen) {
      this.ed.change([{ from: match.from, to: match.to, text: this.replacementFor(match) }], null);
    }
    this.step(1);
  }

  // Every match the session's lines hold replaced: a paged session's window's alone.
  replaceAll() {
    const placed = this.between(0, this.ed.s.doc.count - 1);
    if (!placed.length) {
      return;
    }
    const edits = placed.map((match) => ({ from: match.from, to: match.to, text: this.replacementFor(match) }));
    this.ed.change(edits, null);
  }
}

export class GoTo {
  constructor(editor) {
    this.ed = editor;
    this.input = element("input", { className: "ed-find-query", type: "text", spellcheck: false });
    this.bar = element("div", { className: "ed-goto", hidden: true }, this.input);
    this.bar.addEventListener("mousedown", (event) => event.stopPropagation());
    editor.host.append(this.bar);
    this.input.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.preventDefault();
        this.close();
      } else if (event.key === "Enter") {
        event.preventDefault();
        this.go();
      }
    });
    this.input.addEventListener("blur", () => (this.bar.hidden = true));
  }

  open() {
    if (!this.ed.s) {
      return;
    }
    this.bar.hidden = false;
    this.input.value = "";
    const s = this.ed.s;
    this.input.placeholder = `Line ${s.base + 1} to ${s.base + s.doc.count}, or line:col`;
    this.input.focus();
  }

  close() {
    this.bar.hidden = true;
    this.ed.focus();
  }

  go() {
    const found = this.input.value.trim().match(/^(\d+)(?:[:,]\s*(\d+))?$/);
    if (found) {
      const ed = this.ed;
      const line = Math.max(0, Math.min(ed.doc.count - 1, Number(found[1]) - 1 - ed.s.base));
      const text = ed.doc.line(line);
      const col = found[2] ? ed.colAtV(text, Number(found[2]) - 1) : 0;
      ed.setSelections([{ anchor: pos(line, col), head: pos(line, col), goal: null }]);
      ed.doc.seal();
      ed.moved();
      ed.reveal(true);
    }
    this.close();
  }
}
