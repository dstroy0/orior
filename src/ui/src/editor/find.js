// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find and replace, and going to a line: the two bars that open over the editor's top right.

import { cmp, pos } from "./document.js";

const MOST = 20000;

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
      this.refresh();
      this.jump(true);
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

  // The query as a pattern, or null where it is empty or does not compile.
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

  refresh() {
    const ed = this.ed;
    this.matches = [];
    if (!this.shown || !ed.s) {
      return;
    }
    const pattern = this.pattern();
    this.bar.dataset.bad = String(Boolean(this.query.value) && !pattern);
    if (pattern) {
      const doc = ed.doc;
      for (let line = 0; line < doc.count && this.matches.length < MOST; line += 1) {
        const text = doc.line(line);
        pattern.lastIndex = 0;
        for (let found = pattern.exec(text); found; found = pattern.exec(text)) {
          if (!found[0].length) {
            pattern.lastIndex += 1;
            continue;
          }
          this.matches.push({ from: pos(line, found.index), to: pos(line, found.index + found[0].length) });
        }
      }
    }
    this.index = this.nearest(true);
    this.drawCount();
    ed.schedule();
  }

  // The match at or after the primary selection's start, or with `fromStart` off after its end.
  nearest(fromStart, direction = 1) {
    if (!this.matches.length) {
      return -1;
    }
    const primary = this.ed.primary();
    const start = cmp(primary.anchor, primary.head) < 0 ? primary.anchor : primary.head;
    const end = cmp(primary.anchor, primary.head) < 0 ? primary.head : primary.anchor;
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

  current() {
    return this.matches[this.index] ?? null;
  }

  // The matches that fall on lines from first to last.
  between(first, last) {
    let low = 0;
    let high = this.matches.length;
    while (low < high) {
      const mid = (low + high) >> 1;
      if (this.matches[mid].to.line < first) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    const out = [];
    for (let index = low; index < this.matches.length && this.matches[index].from.line <= last; index += 1) {
      out.push(this.matches[index]);
    }
    return out;
  }

  jump(fromStart) {
    const match = this.current();
    if (!match) {
      return;
    }
    const ed = this.ed;
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

  replaceAll() {
    if (!this.matches.length) {
      return;
    }
    const edits = this.matches.map((match) => ({ from: match.from, to: match.to, text: this.replacementFor(match) }));
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
