// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor: a session drawn a screen of rows at a time, and every key, pointer and clipboard
// action that edits it or moves through it.
//
// Only the rows on screen are in the page. The text sits in a scrolling space as tall as every
// row. The scroll bars are the page's own, and each frame draws the rows the space shows. Keys
// and typing arrive at a hidden text area that sits at the primary cursor, which is also where an
// input method opens its window.

import { cmp, endOf, isWordChar, least, mapThrough, most, pos, same, wordAt, wordBefore } from "./document.js";
import { hiddenSpans, indentOf, Rows } from "./folding.js";
import { Find, GoTo } from "./find.js";
import { Layer } from "./layer.js";
import { Minimap } from "./minimap.js";
import { selectionPath } from "./shape.js";
import { Hover, Suggest } from "./widgets.js";
import { pressed, status, write } from "../status.js";
import { icon } from "../icons.js";

const PAD = 10;
// How wide the gutter's strip for breakpoints is, in pixels.
const BREAK_STRIP = 16;
// The height of a row, read from the code's line height each time the editor measures.
let LINE = 20;

// How round a selection's corners are, in pixels.
const SELECTION_ROUND = 4;

// Column Selection Mode's key: while it is on, a drag chooses a column, as Shift and Alt do.
const COLUMN_KEY = "orior.column";

// Sticky scroll: the most lines it holds along the top, the longest text it reads the regions of,
// and how long an edit rests before the regions are read again, in milliseconds.
const STICKY_MOST = 5;
const STICKY_LINES = 300000;
const STICKY_REST = 250;
const STICKY_KEY = "orior.sticky";

// Bracket pairs colored by depth: the setting's key, the most lines a text has for its brackets to be
// colored, the most lines read in one frame to learn a line's depth, and the tokens whose brackets
// do not count.
const BRACKETS_KEY = "orior.brackets";
const BRACKETS_LINES = 200000;
const BRACKETS_READ = 20000;
const UNBRACKETED = /\bt-(?:comment|string|regexp)/;
const SHOWN = 10000;
const MAC = /Mac|iPhone|iPad/.test(navigator.platform);
// The scroll speeds, in pixels a millisecond, past which a frame drops more of its work, how long
// a scroll rests before it counts as stopped, and how many screens ahead the idle coloring reaches.
const DROPS = [0.8, 2.5, 7];
const REST = 140;
const AHEAD = 3;
// The milliseconds a frame may spend drawing. A frame over it drops a level whatever the speed, and
// the level comes back a step at a time only after frames run well inside it.
const BUDGET = 8;
// How long input rests before the work it held runs.
const INPUT_REST = 300;

export const startOf = (sel) => least(sel.anchor, sel.head);
export const endOfSel = (sel) => most(sel.anchor, sel.head);
export const empty = (sel) => same(sel.anchor, sel.head);
const caret = (p, goal = null) => ({ anchor: p, head: p, goal });

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
export const escapeHtml = (text) => text.replace(/[&<>"]/g, (char) => ESCAPES[char]);

function div(className) {
  const made = document.createElement("div");
  made.className = className;
  return made;
}

const CODES = {
  BracketLeft: "[", BracketRight: "]", Slash: "/", Backslash: "\\", Period: ".", Comma: ",", Space: "Space",
  NumpadEnter: "Enter", ArrowLeft: "Left", ArrowRight: "Right", ArrowUp: "Up", ArrowDown: "Down",
};

// A key as the key map names it: Mod is Ctrl, or Cmd on a Mac, then Alt and Shift, then the key.
function keyName(event) {
  const parts = [];
  if (MAC ? event.metaKey : event.ctrlKey) {
    parts.push("Mod");
  }
  if (MAC && event.ctrlKey) {
    parts.push("Ctrl");
  }
  if (event.altKey) {
    parts.push("Alt");
  }
  if (event.shiftKey) {
    parts.push("Shift");
  }
  const code = event.code ?? "";
  let key = CODES[code];
  if (!key) {
    const letter = code.match(/^Key([A-Z])$/) ?? code.match(/^Digit(\d)$/);
    key = letter ? letter[1] : event.key;
  }
  parts.push(key);
  return parts.join("+");
}

// A snippet's text with its stops: ${1:name} stops on name, $1 stops where it stands, and $0 is
// where the cursor ends.
export function parseSnippet(body) {
  let text = "";
  const stops = [];
  const pattern = /\\\$|\$\{(\d+):([^}]*)\}|\$(\d+)/g;
  let at = 0;
  for (const found of body.matchAll(pattern)) {
    text += body.slice(at, found.index);
    at = found.index + found[0].length;
    if (found[0] === "\\$") {
      text += "$";
      continue;
    }
    const index = Number(found[1] ?? found[3]);
    const name = found[2] ?? "";
    stops.push({ index, start: text.length, end: text.length + name.length });
    text += name;
  }
  text += body.slice(at);
  stops.sort((a, b) => (a.index || Infinity) - (b.index || Infinity));
  return { text, stops };
}

export class Editor {
  // The status line goes in `statusHost` where one is given, and under the editor where not.
  constructor(host, { onCursor, onChange, onChangeMark, statusHost = null } = {}) {
    this.host = host;
    this.onChangeMark = onChangeMark ?? (() => {});
    this.onCursor = onCursor ?? (() => {});
    this.onChange = onChange ?? (() => {});
    host.classList.add("ed");
    this.gutter = div("ed-gutter");
    this.gutterRows = div("ed-gutter-rows");
    this.gutter.append(this.gutterRows);
    this.scroller = div("ed-scroll");
    this.space = div("ed-space");
    this.under = div("ed-under");
    this.text = div("ed-text");
    this.over = div("ed-over");
    this.input = document.createElement("textarea");
    this.input.className = "ed-input";
    this.input.spellcheck = false;
    this.input.setAttribute("autocorrect", "off");
    this.input.setAttribute("autocapitalize", "off");
    this.input.setAttribute("aria-label", "Text");
    // The selections, each one shape over the characters it covers, under the text.
    this.picked = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    this.picked.setAttribute("class", "ed-picked");
    this.picked.setAttribute("aria-hidden", "true");
    this.pickedHtml = "";
    this.space.append(this.under, this.picked, this.text, this.over, this.input);
    this.textLayer = new Layer(this.text);
    this.underLayer = new Layer(this.under);
    this.gutterLayer = new Layer(this.gutterRows);
    this.overHtml = "";
    this.scroller.append(this.space);
    const canvas = document.createElement("canvas");
    canvas.className = "ed-mini";
    this.status = div("ed-status");
    this.sticky = div("ed-sticky");
    this.sticky.hidden = true;
    this.stickyOn = localStorage.getItem(STICKY_KEY) !== "false";
    this.bracketsOn = localStorage.getItem(BRACKETS_KEY) !== "false";
    this.columnMode = localStorage.getItem(COLUMN_KEY) === "true";
    this.stickyList = [];
    this.stickyFor = null;
    this.stickyDoc = -1;
    this.stickyWait = 0;
    this.stickyKey = "";
    host.append(this.gutter, this.scroller, canvas, this.sticky);
    (statusHost ?? host).append(this.status);
    this.minimap = new Minimap(this, canvas);
    this.sticky.addEventListener("mousedown", (event) => {
      const row = event.target.closest(".ed-sticky-row");
      if (!row || !this.s) {
        return;
      }
      event.preventDefault();
      const line = Number(row.dataset.line);
      this.goTo(line);
      this.scroller.scrollTop = this.rows().rowOf(line) * LINE;
      this.focus();
    });
    this.sticky.addEventListener(
      "wheel",
      (event) => {
        event.preventDefault();
        this.scroller.scrollTop += event.deltaY;
        this.scroller.scrollLeft += event.deltaX;
      },
      { passive: false }
    );
    this.find = new Find(this);
    this.goto = new GoTo(this);
    this.hover = new Hover(this);
    this.suggest = new Suggest(this);
    this.s = null;
    this.cw = 7.8;
    this.rowsKey = "";
    this.rowsCache = null;
    this.widestAt = -1;
    this.widestCache = 0;
    this.chord = null;
    this.held = new Map();
    this.clip = null;
    this.composing = false;
    this.measure();
    document.fonts?.ready.then(() => {
      this.measure();
      this.schedule();
    });
    // A hidden editor measures nothing, and measures again once it shows.
    new ResizeObserver(() => {
      if (!this.measured) {
        this.measure();
      }
      this.schedule();
    }).observe(host);
    this.bind();
    this.show(null);
  }

  // Layout.

  measure() {
    const probe = document.createElement("span");
    probe.textContent = "M".repeat(100);
    this.text.append(probe);
    const width = probe.getBoundingClientRect().width;
    this.measured = width > 0;
    this.cw = width / 100 || this.cw || 7.8;
    probe.remove();
    LINE = Math.round(Number.parseFloat(getComputedStyle(this.host).lineHeight)) || LINE;
  }

  // Measures again and draws every row anew, as a change of font asks.
  restyle() {
    this.measure();
    this.rowsKey = "";
    this.rowsCache = null;
    this.widestAt = -1;
    this.schedule();
  }

  get doc() {
    return this.s.doc;
  }

  get lineHeight() {
    return LINE;
  }

  unit() {
    return this.s.indent.tabs ? "\t" : " ".repeat(this.s.indent.size);
  }

  vcolOf(text, col) {
    const size = this.s.indent.size;
    let v = 0;
    for (let index = 0; index < col && index < text.length; index += 1) {
      v += text[index] === "\t" ? size - (v % size) : 1;
    }
    return v;
  }

  vcol(p) {
    return this.vcolOf(this.doc.line(p.line), p.col);
  }

  // The col whose left edge is nearest a visual column, or with `round` off the col it falls in.
  colAtV(text, v, round = true) {
    const size = this.s.indent.size;
    let x = 0;
    for (let index = 0; index < text.length; index += 1) {
      const width = text[index] === "\t" ? size - (x % size) : 1;
      if (round ? x + width / 2 > v : x + width > v) {
        return index;
      }
      x += width;
    }
    return text.length;
  }

  rows() {
    const s = this.s;
    const key = `${s.doc.id}:${s.foldings}:${s.doc.count}`;
    if (key !== this.rowsKey) {
      this.rowsCache = new Rows(s.doc.count, s.folded.size ? hiddenSpans(s.folded) : []);
      this.rowsKey = key;
    }
    return this.rowsCache;
  }

  widthOf(lines) {
    let widest = 0;
    for (const text of lines) {
      if (text.length > widest || (text.includes("\t") && text.length * this.s.indent.size > widest)) {
        widest = Math.max(widest, this.vcolOf(text, Math.min(text.length, SHOWN)));
      }
    }
    return widest;
  }

  // The widest line, which sizes the space across. Reading every line costs a pass over the text.
  // An edit widens it by the lines the cursors are on, and the full pass waits for a pause.
  widest() {
    const doc = this.doc;
    if (this.widestOf !== doc) {
      this.widestOf = doc;
      this.widestCache = this.widthOf(doc.lines);
      this.widestAt = doc.id;
    } else if (this.widestAt !== doc.id) {
      this.widestAt = doc.id;
      this.widestCache = Math.max(this.widestCache, this.widthOf(this.s.selections.map((sel) => doc.line(sel.head.line))));
      window.clearTimeout(this.widestWait);
      this.widestWait = window.setTimeout(() => {
        this.later("widest", () => {
          if (this.s?.doc === doc) {
            this.widestCache = this.widthOf(doc.lines);
            this.schedule();
          }
        });
      }, 700);
    }
    return this.widestCache;
  }

  // Lines read into a session from its file. Lines above the window push everything down, and the
  // view scrolls by as many rows: what is on screen stays put.
  grown(session, added, lines) {
    if (session !== this.s) {
      return;
    }
    this.widestCache = Math.max(this.widestCache, this.widthOf(lines));
    this.rowsKey = "";
    this.size();
    if (added) {
      this.scroller.scrollTop += added * LINE;
    }
    if (this.find.shown) {
      window.clearTimeout(this.findWait);
      this.findWait = window.setTimeout(() => this.find.refresh(), 400);
    }
    this.schedule();
  }

  // Sizes the scrolling space to every row and the widest line.
  size() {
    const rows = this.rows();
    this.space.style.height = `${rows.size * LINE + LINE}px`;
    this.space.style.width = `${Math.max(this.scroller.clientWidth, PAD + (this.widest() + 4) * this.cw)}px`;
  }

  xOf(p) {
    return PAD + this.vcol(p) * this.cw;
  }

  // The place under a pointer.
  posAt(event) {
    const rect = this.space.getBoundingClientRect();
    const rows = this.rows();
    const row = Math.floor((event.clientY - rect.top) / LINE);
    if (row < 0) {
      return pos(rows.lineOf(0), 0);
    }
    if (row >= rows.size) {
      const last = rows.lineOf(rows.size - 1);
      return pos(last, this.doc.line(last).length);
    }
    const line = rows.lineOf(row);
    return pos(line, this.colAtV(this.doc.line(line), (event.clientX - rect.left - PAD) / this.cw));
  }

  // Showing a session.

  show(session) {
    if (this.s) {
      this.s.top = this.scroller.scrollTop / LINE;
      this.s.left = this.scroller.scrollLeft;
      this.s.view = null;
    }
    this.s = session;
    this.hover.hide();
    this.suggest.close();
    this.host.dataset.empty = String(!session);
    this.rowsKey = "";
    this.widestOf = null;
    this.statusSaid = "";
    if (session) {
      session.view = this;
      this.text.style.tabSize = String(session.indent.size);
      this.size();
      this.scroller.scrollTop = session.top * LINE;
      this.scroller.scrollLeft = session.left;
      this.find.refresh();
    }
    this.paint();
    this.onCursor();
  }

  focus() {
    this.input.focus({ preventScroll: true });
  }

  hasFocus() {
    return document.activeElement === this.input;
  }

  primary() {
    return this.s.selections[this.s.primary];
  }

  head() {
    return this.s ? this.primary().head : null;
  }

  // Selections.

  copySelections() {
    return this.s.selections.map((sel) => ({ anchor: { ...sel.anchor }, head: { ...sel.head }, goal: sel.goal }));
  }

  // Sets the selections, in order and with any that overlap merged. `chosen` is the primary.
  setSelections(list, chosen = list.at(-1)) {
    const doc = this.doc;
    const items = list.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: sel.goal ?? null, chosen: sel === chosen }));
    items.sort((a, b) => cmp(startOf(a), startOf(b)));
    const out = [];
    for (const sel of items) {
      const prev = out.at(-1);
      const touching = prev && same(startOf(sel), endOfSel(prev)) && (empty(sel) || empty(prev));
      if (prev && (cmp(startOf(sel), endOfSel(prev)) < 0 || touching)) {
        const start = least(startOf(prev), startOf(sel));
        const end = most(endOfSel(prev), endOfSel(sel));
        const forward = cmp(prev.head, prev.anchor) >= 0;
        prev.anchor = forward ? start : end;
        prev.head = forward ? end : start;
        prev.chosen = prev.chosen || sel.chosen;
        continue;
      }
      out.push(sel);
    }
    let index = out.findIndex((sel) => sel.chosen);
    if (index < 0) {
      index = out.length - 1;
    }
    out.forEach((sel) => delete sel.chosen);
    this.s.selections = out;
    this.s.primary = index;
  }

  select(list, chosen) {
    this.setSelections(list, chosen);
    this.doc.seal();
    this.moved();
    this.reveal();
  }

  // After the selections move: the frame, the panels that follow the cursor, and the snippet,
  // which ends once its cursor leaves it.
  moved() {
    const snippet = this.s?.snippet;
    if (snippet) {
      const head = this.primary().head;
      const inside = this.s.selections.length === 1 && snippet.stops.some((stop) => cmp(stop.from, head) <= 0 && cmp(head, stop.to) <= 0);
      if (!inside) {
        this.s.snippet = null;
      }
    }
    this.schedule();
    this.later("cursor", () => this.onCursor());
  }

  // Input first. A key or a press marks the status block's input active, and until it has rested
  // INPUT_REST every piece of work off the rows on screen is held here, one of each kind, and done
  // once input stops.
  //
  // The caret holds still while input is active. A caret that blinks starts its blink again each
  // time a key draws it, and that start costs the key a frame.
  inputting() {
    if (!status.input.active) {
      write("input", { active: true });
      this.host.dataset.typing = "true";
    }
    window.clearTimeout(this.inputWait);
    this.inputWait = window.setTimeout(() => {
      write("input", { active: false });
      delete this.host.dataset.typing;
      const held = this.held;
      this.held = new Map();
      held.forEach((run) => run());
      this.paint();
    }, INPUT_REST);
  }

  later(kind, run) {
    if (status.input.active) {
      this.held.set(kind, run);
    } else {
      run();
    }
  }

  // Folds open where they hide the primary cursor, and the view scrolls to it.
  reveal(center = false) {
    const s = this.s;
    if (!s) {
      return;
    }
    const head = this.primary().head;
    if (s.folded.size) {
      let opened = false;
      for (const [start, end] of [...s.folded]) {
        if (start < head.line && head.line <= end) {
          s.folded.delete(start);
          opened = true;
        }
      }
      if (opened) {
        s.foldings += 1;
      }
    }
    this.size();
    const y = this.rows().rowOf(head.line) * LINE;
    const top = this.scroller.scrollTop;
    const height = this.scroller.clientHeight;
    if (center && (y < top || y + LINE > top + height)) {
      this.scroller.scrollTop = y - height / 2;
    } else if (y < top) {
      this.scroller.scrollTop = y;
    } else if (y + LINE > top + height) {
      this.scroller.scrollTop = y + LINE - height;
    }
    const x = this.xOf(head);
    const left = this.scroller.scrollLeft;
    const width = this.scroller.clientWidth;
    if (x - 4 * this.cw < left) {
      this.scroller.scrollLeft = Math.max(0, x - 8 * this.cw);
    } else if (x + 4 * this.cw > left + width) {
      this.scroller.scrollLeft = x + 8 * this.cw - width;
    }
    this.schedule();
  }

  // Editing.

  // Writes edits and sets the selections. `place` is given a function that carries a place in the
  // old text to the new and answers the selections, or, left out, every selection is carried.
  change(edits, kind, place) {
    const s = this.s;
    if (s.readOnly) {
      return false;
    }
    const before = this.copySelections();
    const { edits: written } = s.doc.change(edits, kind, before);
    const map = (p, after = false) => mapThrough(p, written, after);
    const next = place
      ? place(map)
      : s.selections.map((sel) => ({ anchor: map(sel.anchor, !empty(sel) && cmp(sel.anchor, sel.head) > 0), head: map(sel.head, true), goal: null }));
    this.setSelections(next, next[Math.min(s.primary, next.length - 1)]);
    if (written.length) {
      s.doc.settle(this.copySelections());
      this.edited();
    }
    this.moved();
    this.reveal();
    return written.length > 0;
  }

  // Each selection gives one edit, { from, to, text }, with `select`, the offsets into its text the
  // selection goes to after, or null to leave the selection to be carried.
  edit(plan, kind) {
    const plans = this.s.selections.map((sel, index) => plan(sel, index));
    const edits = plans.filter(Boolean).map(({ from, to, text }) => ({ from, to, text }));
    return this.change(edits, kind, (map) =>
      plans.map((one, index) => {
        const sel = this.s.selections[index];
        if (!one) {
          return { anchor: map(sel.anchor), head: map(sel.head, true), goal: null };
        }
        const from = map(one.from);
        const end = endOf(from, one.text);
        const at = (offset) => (offset <= one.text.length ? endOf(from, one.text.slice(0, offset)) : pos(end.line, end.col + offset - one.text.length));
        const [a, b] = one.select ?? [one.text.length, one.text.length];
        return { anchor: at(a), head: at(b), goal: null };
      })
    );
  }

  edited() {
    const s = this.s;
    this.later("find", () => this.find.refresh());
    this.later("change", () => this.onChange(s));
  }

  // The lines the selections cover. A selection that ends at the start of a line leaves that line out.
  linesOf() {
    const lines = new Set();
    for (const sel of this.s.selections) {
      const start = startOf(sel);
      const end = endOfSel(sel);
      const last = end.line > start.line && end.col === 0 ? end.line - 1 : end.line;
      for (let line = start.line; line <= last; line += 1) {
        lines.add(line);
      }
    }
    return [...lines].sort((a, b) => a - b);
  }

  // The covered lines as runs of neighbors, [first, last].
  blocks() {
    const out = [];
    for (const line of this.linesOf()) {
      if (out.length && line <= out.at(-1)[1] + 1) {
        out.at(-1)[1] = line;
      } else {
        out.push([line, line]);
      }
    }
    return out;
  }

  type(text) {
    const s = this.s;
    if (!s) {
      return;
    }
    const lang = s.language ?? {};
    const pairs = lang.pairs ?? [];
    const opens = new Map(pairs.map((pair) => [pair[0], pair[1]]));
    (lang.quotes ?? []).forEach((quote) => opens.set(quote, quote));
    const closers = new Set([...opens.values()]);
    const brackets = new Set(pairs.filter((pair) => pair[0] !== pair[1]).map((pair) => pair[1]));
    const one = [...text].length === 1;
    const doc = s.doc;
    this.edit((sel) => {
      const from = startOf(sel);
      const to = endOfSel(sel);
      const line = doc.line(from.line);
      if (one) {
        const next = line[to.col];
        const prev = line[from.col - 1];
        if (empty(sel) && closers.has(text) && next === text) {
          return { from, to, text: "", select: [1, 1] };
        }
        if (opens.has(text)) {
          const close = opens.get(text);
          if (!empty(sel)) {
            const inner = doc.slice(from, to);
            return { from, to, text: text + inner + close, select: [1, 1 + inner.length] };
          }
          const quote = close === text;
          const nextOk = next === undefined || /[\s;,.)\]}>]/.test(next);
          const prevOk = !quote || (!isWordChar(prev) && prev !== "\\" && prev !== text);
          if (nextOk && prevOk) {
            return { from, to, text: text + close, select: [1, 1] };
          }
        }
        if (brackets.has(text) && empty(sel) && from.col > 0 && /^\s*$/.test(line.slice(0, from.col))) {
          const lead = line.slice(0, from.col);
          const unit = this.unit();
          const less = lead.endsWith(unit) ? lead.slice(0, -unit.length) : lead.replace(/(?:\t| +)$/, "");
          return { from: pos(from.line, 0), to, text: less + text };
        }
      }
      return { from, to, text };
    }, one && !/\s/.test(text) ? "type" : "space");
    if (lang.complete && one && isWordChar(text)) {
      this.suggest.update(true);
    } else if (this.suggest.open) {
      this.suggest.update(false);
    }
  }

  newline() {
    const lang = this.s.language ?? {};
    const doc = this.doc;
    this.edit((sel) => {
      const from = startOf(sel);
      const to = endOfSel(sel);
      const line = doc.line(from.line);
      const before = line.slice(0, from.col);
      const rest = doc.line(to.line).slice(to.col);
      const lead = (line.match(/^[ \t]*/)[0]).slice(0, from.col);
      const deeper = lang.indentAfter?.test(before) ? this.unit() : "";
      const prev = before.trimEnd().at(-1);
      const next = rest.trimStart()[0];
      const paired = (lang.pairs ?? []).some((pair) => pair[0] !== pair[1] && pair[0] === prev && pair[1] === next);
      if (paired && deeper) {
        const gap = rest.length - rest.trimStart().length;
        const text = `\n${lead}${deeper}\n${lead}`;
        const at = 1 + lead.length + deeper.length;
        return { from, to: pos(to.line, to.col + gap), text, select: [at, at] };
      }
      return { from, to, text: `\n${lead}${deeper}` };
    }, null);
    this.doc.seal();
  }

  backspace(word = false) {
    const lang = this.s.language ?? {};
    const doc = this.doc;
    const pairs = [...(lang.pairs ?? []), ...(lang.quotes ?? []).map((quote) => quote + quote)];
    this.edit((sel) => {
      if (!empty(sel)) {
        return { from: startOf(sel), to: endOfSel(sel), text: "" };
      }
      const p = sel.head;
      const line = doc.line(p.line);
      if (p.col === 0) {
        return p.line === 0 ? null : { from: pos(p.line - 1, doc.line(p.line - 1).length), to: p, text: "" };
      }
      if (word) {
        return { from: this.wordLeft(p), to: p, text: "" };
      }
      const prev = line[p.col - 1];
      const next = line[p.col];
      if (pairs.some((pair) => pair[0] === prev && pair[1] === next)) {
        return { from: pos(p.line, p.col - 1), to: pos(p.line, p.col + 1), text: "" };
      }
      const lead = line.slice(0, p.col);
      if (!this.s.indent.tabs && /^ +$/.test(lead)) {
        const size = this.s.indent.size;
        return { from: pos(p.line, p.col - (lead.length % size || size)), to: p, text: "" };
      }
      const step = p.col >= 2 && /[\uDC00-\uDFFF]/.test(prev) ? 2 : 1;
      return { from: pos(p.line, p.col - step), to: p, text: "" };
    }, "delete");
    if (this.suggest.open) {
      this.suggest.update(false);
    }
  }

  deleteForward(word = false) {
    const doc = this.doc;
    this.edit((sel) => {
      if (!empty(sel)) {
        return { from: startOf(sel), to: endOfSel(sel), text: "" };
      }
      const p = sel.head;
      const line = doc.line(p.line);
      if (p.col >= line.length) {
        return p.line >= doc.count - 1 ? null : { from: p, to: pos(p.line + 1, 0), text: "" };
      }
      if (word) {
        return { from: p, to: this.wordRight(p), text: "" };
      }
      const step = /[\uD800-\uDBFF]/.test(line[p.col]) ? 2 : 1;
      return { from: p, to: pos(p.line, p.col + step), text: "" };
    }, "delete-forward");
  }

  // Deletes from each cursor to the start of its line.
  deleteToStart() {
    this.edit((sel) => (empty(sel) ? { from: pos(sel.head.line, 0), to: sel.head, text: "" } : { from: startOf(sel), to: endOfSel(sel), text: "" }), null);
  }

  tab(back) {
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

  indentLines(direction) {
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

  toggleComment() {
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

  moveLines(direction) {
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

  copyLines(direction) {
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

  deleteLines() {
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

  insertLine(below) {
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

  undo(back = true) {
    if (this.s.readOnly) {
      return;
    }
    const found = back ? this.doc.undo() : this.doc.redo();
    if (!found) {
      return;
    }
    this.setSelections(found.selections.map((sel) => ({ ...sel })), found.selections[Math.min(this.s.primary, found.selections.length - 1)]);
    this.edited();
    this.moved();
    this.reveal();
  }

  // Selections from keys.

  charLeft(p) {
    if (p.col > 0) {
      const line = this.doc.line(p.line);
      return pos(p.line, p.col - (p.col >= 2 && /[\uDC00-\uDFFF]/.test(line[p.col - 1]) ? 2 : 1));
    }
    const rows = this.rows();
    const row = rows.rowOf(p.line);
    if (row === 0) {
      return p;
    }
    const line = rows.lineOf(row - 1);
    return pos(line, this.doc.line(line).length);
  }

  charRight(p) {
    const text = this.doc.line(p.line);
    if (p.col < text.length) {
      return pos(p.line, p.col + (/[\uD800-\uDBFF]/.test(text[p.col]) ? 2 : 1));
    }
    const rows = this.rows();
    const row = rows.rowOf(p.line);
    return row + 1 >= rows.size ? p : pos(rows.lineOf(row + 1), 0);
  }

  wordLeft(p) {
    if (p.col === 0) {
      return this.charLeft(p);
    }
    const text = this.doc.line(p.line);
    let at = p.col;
    while (at > 0 && /\s/.test(text[at - 1])) {
      at -= 1;
    }
    const kind = isWordChar(text[at - 1]);
    while (at > 0 && !/\s/.test(text[at - 1]) && isWordChar(text[at - 1]) === kind) {
      at -= 1;
    }
    return pos(p.line, at);
  }

  wordRight(p) {
    const text = this.doc.line(p.line);
    if (p.col >= text.length) {
      return this.charRight(p);
    }
    let at = p.col;
    while (at < text.length && /\s/.test(text[at])) {
      at += 1;
    }
    const kind = isWordChar(text[at]);
    while (at < text.length && !/\s/.test(text[at]) && isWordChar(text[at]) === kind) {
      at += 1;
    }
    return pos(p.line, at);
  }

  vertical(sel, rowsBy) {
    const goal = sel.goal ?? this.vcol(sel.head);
    const rows = this.rows();
    const row = rows.rowOf(sel.head.line) + rowsBy;
    if (row < 0) {
      return { p: pos(rows.lineOf(0), 0), goal };
    }
    if (row >= rows.size) {
      const last = rows.lineOf(rows.size - 1);
      return { p: pos(last, this.doc.line(last).length), goal };
    }
    const line = rows.lineOf(row);
    return { p: pos(line, this.colAtV(this.doc.line(line), goal)), goal };
  }

  // Moves every selection's head; with `extend` the anchors stay.
  moveBy(step, extend) {
    const next = this.s.selections.map((sel) => {
      const found = step(sel, extend);
      const p = found.p ?? found;
      return extend ? { anchor: sel.anchor, head: p, goal: found.goal ?? null } : caret(p, found.goal ?? null);
    });
    this.suggest.close();
    this.select(next, next[this.s.primary]);
  }

  page(direction, extend) {
    const by = Math.max(1, Math.floor(this.scroller.clientHeight / LINE) - 1);
    this.scroller.scrollTop += direction * by * LINE;
    this.moveBy((sel) => this.vertical(sel, direction * by), extend);
  }

  home(sel) {
    const text = this.doc.line(sel.head.line);
    const lead = text.match(/^[ \t]*/)[0].length;
    return pos(sel.head.line, sel.head.col === lead ? 0 : lead);
  }

  // Puts the cursor on a line of the text, from 0, and the line in the middle of the screen.
  goTo(line, col = 0) {
    const p = this.doc.clamp(pos(line, col));
    this.select([caret(p)]);
    this.reveal(true);
  }

  setColumnMode(on) {
    this.columnMode = on;
    localStorage.setItem(COLUMN_KEY, String(on));
  }

  // Lines.

  // The spans of whole lines the selections cover, [first, last], in order and merged where they
  // touch. A selection that only reaches the start of its last line leaves that line out, and one
  // empty selection alone covers the whole text.
  lineSpans() {
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
  rewriteLines(make, kind) {
    const doc = this.doc;
    const edits = this.lineSpans().map(([first, last]) => ({ from: pos(first, 0), to: pos(last, doc.line(last).length), text: make(doc.lines.slice(first, last + 1)).join("\n") }));
    return this.change(edits, kind);
  }

  sortLines(descending) {
    const order = new Intl.Collator(undefined, { numeric: true }).compare;
    this.rewriteLines((lines) => [...lines].sort((a, b) => (descending ? order(b, a) : order(a, b))), "sort");
  }

  uniqueLines() {
    this.rewriteLines((lines) => [...new Set(lines)], "unique");
  }

  // Joins each selection's lines into one, and a cursor's line with the line after it: the space
  // where two lines meet becomes one space, or none where either side is blank.
  joinLines() {
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
  transformCase(kind) {
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

  selectAll() {
    this.select([{ anchor: pos(0, 0), head: this.doc.end(), goal: null }]);
  }

  selectLine() {
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
  matchesOf(needle, whole) {
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
  addMatch(all) {
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

  addCursor(direction) {
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
  bracketPair() {
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

  jumpBracket() {
    const pair = this.bracketPair();
    if (pair) {
      const to = pair[1];
      this.select([caret(pos(to.line, to.col + 1))]);
    }
  }

  // Folding.

  fold(line, close) {
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

  toggleFold(line) {
    const s = this.s;
    if (s.folded.has(line)) {
      s.folded.delete(line);
    } else {
      s.fold(line);
    }
    this.folds();
  }

  foldAll(close) {
    const s = this.s;
    s.folded = close ? s.regions() : new Map();
    this.folds();
  }

  // After folds change: a cursor a fold hides goes to the end of the line that opens the fold.
  folds() {
    const s = this.s;
    s.foldings += 1;
    const spans = hiddenSpans(s.folded);
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

  insertSnippet(from, to, body) {
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

  snippetStep(direction) {
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
  escape() {
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

  copy(event, cut) {
    event.preventDefault();
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
    event.clipboardData.setData("text/plain", text.replace(/\n/g, doc.eol));
    this.clip = { text, whole, parts };
    if (cut) {
      if (whole) {
        this.deleteLines();
      } else {
        this.edit((sel) => (empty(sel) ? null : { from: startOf(sel), to: endOfSel(sel), text: "" }), null);
      }
    }
  }

  paste(event) {
    event.preventDefault();
    const text = event.clipboardData.getData("text/plain").replace(/\r\n?/g, "\n");
    if (!text) {
      return;
    }
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

  // Keys.

  keyMap() {
    const move = (step) => (extend) => () => this.moveBy(step, extend);
    const left = move((sel, extend) => (!extend && !empty(sel) ? startOf(sel) : this.charLeft(sel.head)));
    const right = move((sel, extend) => (!extend && !empty(sel) ? endOfSel(sel) : this.charRight(sel.head)));
    const up = move((sel) => this.vertical(sel, -1));
    const down = move((sel) => this.vertical(sel, 1));
    const wordLeft = move((sel) => this.wordLeft(sel.head));
    const wordRight = move((sel) => this.wordRight(sel.head));
    const lineStart = move((sel) => this.home(sel));
    const lineEnd = move((sel) => pos(sel.head.line, this.doc.line(sel.head.line).length));
    const docStart = move(() => pos(0, 0));
    const docEnd = move(() => this.doc.end());
    const both = (name, make) => ({ [name]: make(false), [`Shift+${name}`]: make(true) });
    const keys = {
      ...both("Left", left),
      ...both("Right", right),
      ...both("Up", up),
      ...both("Down", down),
      ...both("Home", lineStart),
      ...both("End", lineEnd),
      PageUp: () => this.page(-1, false),
      PageDown: () => this.page(1, false),
      "Shift+PageUp": () => this.page(-1, true),
      "Shift+PageDown": () => this.page(1, true),
      Backspace: () => this.backspace(),
      "Shift+Backspace": () => this.backspace(),
      Delete: () => this.deleteForward(),
      Enter: () => (this.suggest.open ? this.suggest.accept() : this.newline()),
      "Shift+Enter": () => this.newline(),
      "Mod+Enter": () => this.insertLine(true),
      "Mod+Shift+Enter": () => this.insertLine(false),
      Tab: () => this.tab(false),
      "Shift+Tab": () => this.tab(true),
      Escape: () => this.escape(),
      "Mod+A": () => this.selectAll(),
      "Mod+Z": () => this.undo(true),
      "Mod+Shift+Z": () => this.undo(false),
      "Mod+Y": () => this.undo(false),
      "Mod+D": () => this.addMatch(false),
      "Mod+Shift+L": () => this.addMatch(true),
      "Mod+L": () => this.selectLine(),
      "Mod+J": () => this.joinLines(),
      "Mod+/": () => this.toggleComment(),
      "Alt+Up": () => this.moveLines(-1),
      "Alt+Down": () => this.moveLines(1),
      "Shift+Alt+Up": () => this.copyLines(-1),
      "Shift+Alt+Down": () => this.copyLines(1),
      "Alt+Shift+Up": () => this.copyLines(-1),
      "Alt+Shift+Down": () => this.copyLines(1),
      "Mod+Shift+K": () => this.deleteLines(),
      "Mod+Alt+Up": () => this.addCursor(-1),
      "Mod+Alt+Down": () => this.addCursor(1),
      "Mod+]": () => this.indentLines(1),
      "Mod+[": () => this.indentLines(-1),
      "Mod+Shift+[": () => this.fold(this.primary().head.line, true),
      "Mod+Shift+]": () => this.fold(this.primary().head.line, false),
      "Mod+Shift+\\": () => this.jumpBracket(),
      "Mod+F": () => this.find.open(false),
      "Mod+H": () => this.find.open(true),
      F3: () => this.find.step(1),
      "Shift+F3": () => this.find.step(-1),
      "Mod+G": () => this.goto.open(),
      "Mod+Space": () => this.suggest.update(true, true),
      "Mod+K": () => (this.chord = "Mod+K"),
      "Mod+K Mod+0": () => this.foldAll(true),
      "Mod+K Mod+J": () => this.foldAll(false),
    };
    if (MAC) {
      Object.assign(keys, {
        ...both("Mod+Left", lineStart),
        ...both("Mod+Right", lineEnd),
        ...both("Alt+Left", wordLeft),
        ...both("Alt+Right", wordRight),
        ...both("Mod+Up", docStart),
        ...both("Mod+Down", docEnd),
        "Alt+Backspace": () => this.backspace(true),
        "Alt+Delete": () => this.deleteForward(true),
        "Mod+Backspace": () => this.deleteToStart(),
      });
      for (const name of ["Mod+Left", "Mod+Right", "Mod+Up", "Mod+Down"]) {
        keys[name.replace("Mod+", "Mod+Shift+")] = keys[`Shift+${name}`];
      }
      for (const name of ["Alt+Left", "Alt+Right"]) {
        keys[name.replace("Alt+", "Alt+Shift+")] = keys[`Shift+${name}`];
      }
    } else {
      Object.assign(keys, {
        ...both("Mod+Left", wordLeft),
        ...both("Mod+Right", wordRight),
        ...both("Mod+Home", docStart),
        ...both("Mod+End", docEnd),
        "Mod+Backspace": () => this.backspace(true),
        "Mod+Delete": () => this.deleteForward(true),
        "Mod+Up": () => (this.scroller.scrollTop -= LINE),
        "Mod+Down": () => (this.scroller.scrollTop += LINE),
      });
      for (const name of ["Mod+Left", "Mod+Right", "Mod+Home", "Mod+End"]) {
        keys[name.replace("Mod+", "Mod+Shift+")] = keys[`Shift+${name}`];
      }
    }
    return keys;
  }

  // Binds keys written as a menu lists them, such as "Shift+Alt+F" or "Ctrl+K Ctrl+I", each to its
  // `run`. A key the editor already binds keeps its own.
  addKeys(list) {
    const named = (press) => {
      const parts = press.split("+");
      const key = parts.pop() || "+";
      const held = [parts.includes("Ctrl") && "Mod", parts.includes("Alt") && "Alt", parts.includes("Shift") && "Shift"].filter(Boolean);
      return [...held, key].join("+");
    };
    for (const { keys, run } of list) {
      const name = keys.split(" ").map(named).join(" ");
      this.keys[name] ??= run;
    }
  }

  onKey(event) {
    if (!this.s || this.composing || event.isComposing) {
      return;
    }
    this.hover.hide();
    const name = keyName(event);
    if (this.suggest.open && this.suggest.key(name)) {
      event.preventDefault();
      return;
    }
    let found;
    if (this.chord) {
      found = this.keys[`${this.chord} ${name}`];
      if (!/^(?:Mod|Alt|Shift|Ctrl)\+(?:Control|Meta|Alt|Shift)$|^(?:Control|Meta|Alt|Shift)$/.test(name)) {
        this.chord = null;
      }
      if (!found) {
        return;
      }
    } else {
      found = this.keys[name];
    }
    if (found) {
      event.preventDefault();
      found();
    }
  }

  // Pointer.

  unitRange(unit, p) {
    if (unit === "word") {
      const word = wordAt(this.doc, p);
      return word ? [word.from, word.to] : [p, p];
    }
    if (unit === "line") {
      const next = p.line + 1 < this.doc.count ? pos(p.line + 1, 0) : pos(p.line, this.doc.line(p.line).length);
      return [pos(p.line, 0), next];
    }
    return [p, p];
  }

  onDown(event, fromGutter = false) {
    if (!this.s || event.button !== 0) {
      return;
    }
    this.inputting();
    const box = this.scroller.getBoundingClientRect();
    if (!fromGutter && (event.clientX - box.left >= this.scroller.clientWidth || event.clientY - box.top >= this.scroller.clientHeight)) {
      return;
    }
    // A press in the gutter's strip by its left edge sets or clears a breakpoint on the line.
    if (fromGutter && this.onBreakpoint && event.clientX - this.gutter.getBoundingClientRect().left < BREAK_STRIP) {
      event.preventDefault();
      this.onBreakpoint(this.s.base + this.posAt(event).line);
      return;
    }
    const target = event.target;
    if (target.dataset?.fold !== undefined) {
      event.preventDefault();
      this.toggleFold(Number(target.dataset.fold));
      return;
    }
    if (target.dataset?.change !== undefined) {
      event.preventDefault();
      this.onChangeMark(Number(target.dataset.change));
      return;
    }
    event.preventDefault();
    this.focus();
    this.hover.hide();
    this.suggest.close();
    const p = this.posAt(event);
    const unit = fromGutter ? "line" : ["char", "word", "line"][Math.min(event.detail || 1, 3) - 1];
    // Ctrl and a click, Cmd on a Mac, goes to where the word under it is defined, where the editor's
    // owner says how.
    if ((MAC ? event.metaKey : event.ctrlKey) && !event.altKey && !event.shiftKey && !fromGutter && unit === "char" && this.onDefinition) {
      this.select([caret(p)]);
      this.onDefinition(p);
      return;
    }
    const sels = this.s.selections;
    let index;
    let anchorFrom;
    let anchorTo;
    if (((event.altKey && event.shiftKey) || (this.columnMode && !event.altKey && !event.shiftKey && unit === "char")) && !fromGutter) {
      const goal = (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw;
      this.dragging = { column: { row: this.rows().rowOf(p.line), v: Math.max(0, goal) } };
      this.dragTo(event);
    } else {
      if (event.altKey && unit === "char" && !fromGutter) {
        const hit = sels.findIndex((sel) => empty(sel) && same(sel.head, p));
        if (hit >= 0 && sels.length > 1) {
          this.select(sels.filter((_, at) => at !== hit));
          return;
        }
        [anchorFrom, anchorTo] = this.unitRange(unit, p);
        const added = { anchor: anchorFrom, head: anchorTo, goal: null };
        this.setSelections([...sels, added], added);
      } else if (event.shiftKey) {
        const kept = this.primary();
        anchorFrom = kept.anchor;
        anchorTo = kept.anchor;
        this.setSelections([{ anchor: kept.anchor, head: p, goal: null }]);
      } else {
        [anchorFrom, anchorTo] = this.unitRange(unit, p);
        this.setSelections([{ anchor: anchorFrom, head: anchorTo, goal: null }]);
      }
      index = this.s.primary;
      this.dragging = { unit, anchorFrom, anchorTo, index };
      this.doc.seal();
      this.moved();
    }
    const move = (moved) => {
      this.lastPointer = moved;
      this.dragTo(moved);
    };
    const tick = window.setInterval(() => {
      const at = this.lastPointer;
      if (!at) {
        return;
      }
      const rect = this.scroller.getBoundingClientRect();
      if (at.clientY < rect.top) {
        this.scroller.scrollTop -= Math.min(LINE * 3, rect.top - at.clientY);
      } else if (at.clientY > rect.bottom) {
        this.scroller.scrollTop += Math.min(LINE * 3, at.clientY - rect.bottom);
      } else {
        return;
      }
      this.dragTo(at);
    }, 40);
    const up = () => {
      window.removeEventListener("mousemove", move);
      window.removeEventListener("mouseup", up);
      window.clearInterval(tick);
      this.dragging = null;
      this.lastPointer = null;
    };
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", up);
  }

  dragTo(event) {
    const drag = this.dragging;
    if (!drag || !this.s) {
      return;
    }
    const p = this.posAt(event);
    if (drag.column) {
      const rows = this.rows();
      const v = Math.max(0, (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw);
      const here = rows.rowOf(p.line);
      const list = [];
      const step = here >= drag.column.row ? 1 : -1;
      for (let row = drag.column.row; ; row += step) {
        const line = rows.lineOf(row);
        const text = this.doc.line(line);
        list.push({ anchor: pos(line, this.colAtV(text, drag.column.v)), head: pos(line, this.colAtV(text, v)), goal: v });
        if (row === here) {
          break;
        }
      }
      this.setSelections(list, list.at(-1));
      this.moved();
      return;
    }
    const [from, to] = this.unitRange(drag.unit, p);
    let anchor = drag.anchorFrom;
    let head = to;
    if (cmp(from, drag.anchorFrom) < 0) {
      anchor = drag.anchorTo;
      head = from;
    }
    const sels = [...this.s.selections];
    const index = Math.min(this.s.primary, sels.length - 1);
    sels[index] = { anchor, head, goal: null };
    this.setSelections(sels, sels[index]);
    this.moved();
  }

  onPointerMove(event) {
    window.clearTimeout(this.hoverWait);
    if (this.dragging || event.buttons || !this.s?.language?.hover) {
      return;
    }
    const { clientX, clientY } = event;
    this.hoverWait = window.setTimeout(() => this.hoverAt(clientX, clientY), 380);
  }

  // A language's hover may answer at once or later, as a language server does; an answer that
  // comes after the pointer has gone elsewhere is dropped.
  async hoverAt(clientX, clientY) {
    if (!this.s?.language?.hover && !this.s?.diagnostics?.length) {
      return;
    }
    const asked = (this.hoverAsked = (this.hoverAsked ?? 0) + 1);
    const session = this.s;
    const p = this.posAt({ clientX, clientY });
    const rect = this.space.getBoundingClientRect();
    const x = clientX - rect.left;
    const text = this.doc.line(p.line);
    if (x > PAD + this.vcolOf(text, text.length) * this.cw + this.cw || x < PAD) {
      this.hover.hide();
      return;
    }
    const said = this.diagnosticsAt(p);
    const found = this.s.language?.hover ? await this.s.language.hover(this.doc, p) : null;
    if (asked !== this.hoverAsked || this.s !== session) {
      return;
    }
    const parts = [...said, ...(found?.parts ?? [])];
    if (!parts.length) {
      this.hover.hide();
      return;
    }
    let from = found?.from ?? p;
    if (!found) {
      let col = p.col;
      while (col > 0 && /\w/.test(text[col - 1])) {
        col -= 1;
      }
      from = { line: p.line, col };
    }
    this.hover.show({ from, to: found?.to ?? p, parts });
  }

  // What the diagnostics under a place say, each in its severity's color: a language server's, or a
  // tool plugin's.
  diagnosticsAt(p) {
    const line = p.line + this.s.base;
    const names = ["", "error", "warning", "note", "hint"];
    return (this.s.diagnostics ?? [])
      .filter(
        (diag) =>
          (line > diag.from.line || (line === diag.from.line && p.col >= diag.from.col)) &&
          (line < diag.to.line || (line === diag.to.line && p.col <= Math.max(diag.to.col, diag.from.col + 1))),
      )
      .map((diag) => ({ className: `diag s${diag.severity}`, text: `**${names[diag.severity] ?? "note"}**${diag.source ? ` ${diag.source}` : ""}: ${diag.message}` }));
  }

  bind() {
    this.keys = this.keyMap();
    const input = this.input;
    // A key draws what it changed in its own event, and never waits on the frame after.
    const drawn = () => this.pending && this.paint();
    input.addEventListener("keydown", (event) => {
      this.inputting();
      this.onKey(event);
      drawn();
    });
    input.addEventListener("compositionstart", () => {
      this.composing = true;
      this.host.dataset.composing = "true";
    });
    input.addEventListener("compositionend", (event) => {
      this.composing = false;
      delete this.host.dataset.composing;
      const text = event.data || input.value;
      input.value = "";
      if (text && this.s) {
        this.type(text);
      }
    });
    input.addEventListener("input", () => {
      this.inputting();
      if (this.composing) {
        return;
      }
      const text = input.value.replace(/\r\n?/g, "\n");
      input.value = "";
      if (text && this.s) {
        this.type(text);
      }
      drawn();
    });
    input.addEventListener("copy", (event) => this.s && this.copy(event, false));
    input.addEventListener("cut", (event) => this.s && this.copy(event, true));
    input.addEventListener("paste", (event) => this.s && this.paste(event));
    input.addEventListener("focus", () => {
      this.host.dataset.focus = "true";
      this.schedule();
    });
    input.addEventListener("blur", () => {
      this.host.dataset.focus = "false";
      this.schedule();
    });
    this.scroller.addEventListener("mousedown", (event) => this.onDown(event));
    this.gutter.addEventListener("mousedown", (event) => this.onDown(event, true));
    this.scroller.addEventListener("mousemove", (event) => this.onPointerMove(event));
    this.scroller.addEventListener("mouseleave", (event) => {
      window.clearTimeout(this.hoverWait);
      this.hoverAsked = (this.hoverAsked ?? 0) + 1;
      if (!this.hover.holds(event.relatedTarget)) {
        this.hover.hideSoon();
      }
    });
    this.scroller.addEventListener("scroll", () => this.onScroll());
    this.scroller.addEventListener("contextmenu", (event) => event.preventDefault());
    this.gutter.addEventListener("wheel", (event) => {
      this.scroller.scrollTop += event.deltaY;
      event.preventDefault();
    }, { passive: false });
  }

  // Drawing.
  //
  // Each frame does only the work its rows need, and the faster the view moves the more it
  // drops. `level` runs from 0, everything, to 3:
  //
  //   1  the guides, the marks on the word under the cursor and the bracket pair wait
  //   2  rows take only the colors already worked out, and the fold marks and find matches wait
  //   3  rows are plain, and the minimap draws its lines in one color
  //
  // As the speed falls the level falls with it, and once the view rests a frame draws everything
  // and the lines ahead of where it was heading are colored a slice at a time. Over the speed sits
  // a throttle: a frame that runs past BUDGET raises `strain`, and the level is whichever of the two
  // is higher. A slow frame costs detail and never smoothness.

  onScroll() {
    this.hover.hide();
    const now = performance.now();
    const top = this.scroller.scrollTop;
    const moved = top - (this.lastTop ?? top);
    const spent = Math.max(1, now - (this.lastScroll ?? now - 16));
    this.lastTop = top;
    this.lastScroll = now;
    // Smoothed: one long jump between two events does not throw the level up and back.
    const speed = 0.6 * status.scroll.speed + 0.4 * (Math.abs(moved) / spent);
    const level = Math.max(status.frame.strain, DROPS.filter((drop) => speed > drop).length);
    write("scroll", { speed, level, heading: moved ? Math.sign(moved) : status.scroll.heading });
    // The gutter sits outside the scrolling space. It follows now and not a frame late.
    this.gutterRows.style.transform = `translateY(${-top}px)`;
    window.clearTimeout(this.restWait);
    this.restWait = window.setTimeout(() => {
      write("scroll", { speed: 0, level: 0 });
      write("frame", { strain: 0 });
      this.paint();
      this.warm();
    }, REST);
    this.schedule();
  }

  // The throttle: how long the last frame took moves the strain up at once or down a step.
  strained(spent) {
    let strain = status.frame.strain;
    if (spent > BUDGET) {
      strain = Math.min(DROPS.length, strain + 1);
      this.easy = 0;
    } else if (spent < BUDGET / 3 && strain > 0) {
      this.easy = (this.easy ?? 0) + 1;
      if (this.easy >= 6) {
        strain -= 1;
        this.easy = 0;
      }
    }
    write("frame", { spent, strain });
    if (status.scroll.speed) {
      write("scroll", { level: Math.max(strain, DROPS.filter((drop) => status.scroll.speed > drop).length) });
    }
  }

  // Colors the lines ahead of the view in the way it was heading, while the page is idle.
  // Scrolling on finds them done.
  warm() {
    const s = this.s;
    if (!s?.highlight.grammar) {
      return;
    }
    const rows = this.rows();
    const shown = Math.ceil(this.scroller.clientHeight / LINE);
    const first = Math.floor(this.scroller.scrollTop / LINE);
    const heading = status.scroll.heading;
    const from = heading > 0 ? first + shown : Math.max(0, first - 1);
    let row = from;
    const end = heading > 0 ? Math.min(rows.size, from + shown * AHEAD) : Math.max(0, from - shown * AHEAD);
    const idle = (run) => (window.requestIdleCallback ? window.requestIdleCallback(run) : window.setTimeout(() => run({ timeRemaining: () => 8 }), 16));
    const slice = (deadline) => {
      if (this.s !== s || pressed()) {
        return;
      }
      while (deadline.timeRemaining() > 2 && (heading > 0 ? row < end : row >= end)) {
        s.highlight.runsOf(rows.lineOf(row));
        row += heading;
      }
      write("colors", { ahead: Math.abs(row - from) });
      if (heading > 0 ? row < end : row >= end) {
        idle(slice);
      }
    };
    idle(slice);
  }

  schedule() {
    if (!this.pending) {
      this.pending = requestAnimationFrame(() => {
        this.pending = 0;
        this.paint();
      });
    }
  }

  // Bracket pairs: each bracket outside a comment or a string colored by how deep it stands, the
  // depth at the start of each line kept on the session until an edit reaches it.
  setBrackets(on) {
    this.bracketsOn = on;
    localStorage.setItem(BRACKETS_KEY, String(on));
    this.textLayer.clear();
    this.schedule();
  }

  // The depth at the end of a line that starts at `depth`, a closing bracket never taking it below 0.
  bracketEnd(line, depth) {
    const text = this.s.doc.line(line);
    const runs = this.s.highlight.runsOf(line);
    for (let index = 0; index < runs.length; index += 1) {
      const [start, name] = runs[index];
      if (UNBRACKETED.test(name)) {
        continue;
      }
      const end = runs[index + 1]?.[0] ?? text.length;
      for (let at = start; at < end; at += 1) {
        const char = text[at];
        if (char === "(" || char === "[" || char === "{") {
          depth += 1;
        } else if ((char === ")" || char === "]" || char === "}") && depth > 0) {
          depth -= 1;
        }
      }
    }
    return depth;
  }

  // The depth at the start of a line, or null where it is too far below what is read to read now.
  depthAt(line) {
    const s = this.s;
    if (!s.depths) {
      s.depths = [0];
    }
    let at = s.depths.length - 1;
    if (line - at > BRACKETS_READ) {
      return null;
    }
    for (; at < line; at += 1) {
      s.depths[at + 1] = this.bracketEnd(at, s.depths[at]);
    }
    return s.depths[line];
  }

  rowHtml(line) {
    const s = this.s;
    const text = s.doc.line(line);
    const level = status.scroll.level;
    const runs = level >= 3 ? [[0, ""]] : level === 2 ? s.highlight.cached(line) ?? [[0, ""]] : s.highlight.runsOf(line);
    const shown = Math.min(text.length, SHOWN);
    let depth = this.bracketsOn && level < 2 && s.doc.count <= BRACKETS_LINES && s.language ? this.depthAt(line) : null;
    let html = "";
    for (let index = 0; index < runs.length; index += 1) {
      const [start, name] = runs[index];
      const end = Math.min(runs[index + 1]?.[0] ?? shown, shown);
      if (end <= start) {
        continue;
      }
      let part;
      if (depth === null || UNBRACKETED.test(name)) {
        part = escapeHtml(text.slice(start, end));
      } else {
        part = "";
        let from = start;
        for (let at = start; at < end; at += 1) {
          const char = text[at];
          const opens = char === "(" || char === "[" || char === "{";
          const closes = char === ")" || char === "]" || char === "}";
          if (!opens && !closes) {
            continue;
          }
          if (closes && depth > 0) {
            depth -= 1;
          }
          part += `${escapeHtml(text.slice(from, at))}<span class="ed-br-${depth % 3}">${char}</span>`;
          if (opens) {
            depth += 1;
          }
          from = at + 1;
        }
        part += escapeHtml(text.slice(from, end));
      }
      html += name ? `<span class="${name}">${part}</span>` : part;
    }
    if (s.folded.has(line) && s.endOf(line) >= 0) {
      html += `<span class="ed-folded" data-fold="${line}">⋯</span>`;
    }
    return html;
  }

  // The gutter's mark of how a line differs from the last commit: a bar for a line added or
  // changed, and a wedge at the line's top edge where lines above it were taken out, at the last
  // line's foot for lines taken out below it.
  changeMark(line) {
    const changes = this.s.changes;
    if (!changes) {
      return "";
    }
    let mark = "";
    if (changes.added.has(line)) {
      mark += `<span class="ed-change added" data-change="${line}"></span>`;
    } else if (changes.changed.has(line)) {
      mark += `<span class="ed-change changed" data-change="${line}"></span>`;
    }
    if (changes.removed.has(line)) {
      mark += `<span class="ed-change removed" data-change="${line}"></span>`;
    }
    if (line === this.s.doc.count - 1 && changes.removed.has(line + 1)) {
      mark += `<span class="ed-change removed below" data-change="${line + 1}"></span>`;
    }
    return mark;
  }

  // Sets the marks of how a session's lines differ from the last commit, or takes them away.
  setChanges(session, changes) {
    session.changes = changes;
    if (session === this.s) {
      this.schedule();
    }
  }

  box(name, x, y, width, height = LINE) {
    return `<div class="${name}" style="left:${x}px;top:${y}px;width:${Math.max(0, width)}px;height:${height}px"></div>`;
  }

  // The marks behind the text on one row for a span of a line, from col to col.
  span(name, line, row, from, to, past = false) {
    const text = this.doc.line(line);
    const x = PAD + this.vcolOf(text, from) * this.cw;
    const width = (this.vcolOf(text, to) - this.vcolOf(text, from)) * this.cw + (past ? this.cw * 0.6 : 0);
    return this.box(name, x, row * LINE, width);
  }

  guides(line) {
    const doc = this.doc;
    const size = this.s.indent.size;
    let width = indentOf(doc.line(line), size);
    if (width < 0) {
      let before = -1;
      let after = -1;
      for (let at = line - 1; at >= Math.max(0, line - 200) && before < 0; at -= 1) {
        before = indentOf(doc.line(at), size);
      }
      for (let at = line + 1; at < Math.min(doc.count, line + 200) && after < 0; at += 1) {
        after = indentOf(doc.line(at), size);
      }
      width = Math.max(0, Math.min(before, after));
    }
    return Math.floor(width / size);
  }

  paint() {
    const began = performance.now();
    if (this.pending) {
      cancelAnimationFrame(this.pending);
      this.pending = 0;
    }
    const s = this.s;
    if (!s) {
      this.textLayer.clear();
      this.underLayer.clear();
      this.gutterLayer.clear();
      this.over.innerHTML = "";
      this.overHtml = "";
      this.status.replaceChildren();
      this.minimap.clear();
      this.sticky.hidden = true;
      this.stickyKey = "";
      return;
    }
    const doc = s.doc;
    const rows = this.rows();
    const cw = this.cw;
    const level = status.scroll.level;
    const base = s.base;
    const total = s.window ? Math.max(base + doc.count, Math.round((base + doc.count) * (s.window.size / Math.max(1, s.window.end - s.window.start)))) : doc.count;
    // The gutter holds, left to right, the strip a breakpoint is set in, the line numbers, and the
    // fold arrows and change marks.
    this.gutter.style.width = `${Math.round(String(total).length * cw + 36 + BREAK_STRIP)}px`;
    this.size();
    const top = this.scroller.scrollTop;
    const height = this.scroller.clientHeight;
    // The rows past each edge of the screen, more of them the way the view heads the faster it
    // goes. The page's own scroll never shows a row before a frame draws it.
    const lead = 2 + Math.min(80, Math.ceil((status.scroll.speed * 48) / LINE));
    const first = Math.max(0, Math.floor(top / LINE) - (status.scroll.heading < 0 ? lead : 2));
    const last = Math.min(rows.size - 1, Math.ceil((top + height) / LINE) + (status.scroll.heading > 0 ? lead : 2));
    const spaceWidth = this.space.offsetWidth;
    const focused = this.hasFocus();
    const sels = s.selections;
    const primary = this.primary();
    const caretLines = new Set(sels.filter(empty).map((sel) => sel.head.line));
    const headLines = new Set(sels.map((sel) => sel.head.line));
    // The session's breakpoints, each file line mapped to whether it is bound to code, and the file
    // line a debugged program is stopped on, as the editor's owner gives them.
    const breaks = this.breakpointsOf?.(s) ?? null;
    const paused = this.pausedOf?.(s) ?? null;

    const text = new Map();
    const under = new Map();
    const over = [];
    const gutter = new Map();
    const band = (row, html) => under.set(row, (under.get(row) ?? "") + html);
    const visible = (line) => {
      const row = rows.rowOf(line);
      return row >= first && row <= last && rows.lineOf(row) === line ? row : -1;
    };

    let occurrence = null;
    if (level >= 1) {
      occurrence = null;
    } else if (empty(primary)) {
      occurrence = wordAt(doc, primary.head)?.text ?? null;
    } else if (startOf(primary).line === endOfSel(primary).line) {
      const picked = doc.slice(startOf(primary), endOfSel(primary));
      occurrence = /^[\p{L}\p{N}_$]+$/u.test(picked) ? picked : null;
    }
    if (occurrence && occurrence.length > 120) {
      occurrence = null;
    }
    const matches = this.find.shown && level <= 1 ? this.find.between(rows.lineOf(first), rows.lineOf(last)) : [];
    const current = this.find.shown ? this.find.current() : null;

    for (let row = first; row <= last; row += 1) {
      const line = rows.lineOf(row);
      text.set(row, ["ed-row", this.rowHtml(line)]);
      if (caretLines.has(line)) {
        band(row, this.box("ed-current", 0, row * LINE, spaceWidth));
      }
      if (paused === base + line) {
        band(row, this.box("ed-paused", 0, row * LINE, spaceWidth));
      }
      const levels = level >= 1 ? 0 : this.guides(line);
      for (let step = 0; step < levels; step += 1) {
        band(row, this.box("ed-guide", PAD + step * s.indent.size * cw, row * LINE, 1));
      }
      if (occurrence) {
        const lineText = doc.line(line);
        for (let at = lineText.indexOf(occurrence); at >= 0; at = lineText.indexOf(occurrence, at + 1)) {
          if (!isWordChar(lineText[at - 1]) && !isWordChar(lineText[at + occurrence.length])) {
            band(row, this.span("ed-same", line, row, at, at + occurrence.length));
          }
        }
      }
      const number = `<span class="ed-num-text">${base + line + 1}</span>`;
      const foldable = s.folded.has(line) || (level <= 1 && s.opens(line));
      const folded = foldable && s.folded.has(line);
      const mark = foldable ? `<span class="ed-fold${folded ? " shut" : ""}" data-fold="${line}">${folded ? "▸" : "▾"}</span>` : "";
      const stop = breaks?.get(base + line);
      const dot = stop === undefined ? "" : `<span class="ed-break${stop ? "" : " unbound"}"></span>`;
      const here = paused === base + line ? '<span class="ed-pc"></span>' : "";
      gutter.set(row, [headLines.has(line) ? "ed-num on" : "ed-num", dot + here + number + mark + this.changeMark(line)]);
    }
    for (const match of matches) {
      for (let line = match.from.line; line <= match.to.line; line += 1) {
        const row = visible(line);
        if (row < 0) {
          continue;
        }
        const from = line === match.from.line ? match.from.col : 0;
        const to = line === match.to.line ? match.to.col : doc.line(line).length;
        const on = current && same(current.from, match.from);
        band(row, this.span(on ? "ed-match on" : "ed-match", line, row, from, to, line !== match.to.line));
      }
    }
    // A language server's diagnostics, each underlined in the color of its severity: 1 an error, 2 a
    // warning, 3 a note and 4 a hint. One that spans nothing marks the character it stands at.
    for (const diag of s.diagnostics ?? []) {
      for (let line = diag.from.line - s.base; line <= diag.to.line - s.base; line += 1) {
        const row = line >= 0 && line < doc.count ? visible(line) : -1;
        if (row < 0) {
          continue;
        }
        const from = line === diag.from.line - s.base ? diag.from.col : 0;
        let to = line === diag.to.line - s.base ? diag.to.col : doc.line(line).length;
        if (to <= from) {
          to = from + 1;
        }
        band(row, this.span(`ed-diag s${Math.min(4, Math.max(1, diag.severity))}`, line, row, from, to));
      }
    }
    if (s.snippet) {
      for (const stop of s.snippet.stops) {
        const row = visible(stop.from.line);
        if (row >= 0 && stop.from.line === stop.to.line) {
          band(row, this.span("ed-stop", stop.from.line, row, stop.from.col, stop.to.col, same(stop.from, stop.to)));
        }
      }
    }
    // Each selection covers its characters on each row it reaches, and a line's end inside it as
    // one blank character, the rows' spans drawn as one shape.
    const shapes = [];
    for (const sel of sels) {
      if (empty(sel)) {
        continue;
      }
      const start = startOf(sel);
      const end = endOfSel(sel);
      const spans = [];
      for (let row = Math.max(first, rows.rowOf(start.line)); row <= Math.min(last, rows.rowOf(end.line)); row += 1) {
        const line = rows.lineOf(row);
        if (line < start.line || line > end.line) {
          continue;
        }
        const text = doc.line(line);
        const from = line === start.line ? start.col : 0;
        const ends = line < end.line;
        const to = line === end.line ? end.col : text.length;
        spans.push([row, PAD + this.vcolOf(text, from) * cw, PAD + this.vcolOf(text, to) * cw + (ends ? cw : 0)]);
      }
      shapes.push(selectionPath(spans, LINE, SELECTION_ROUND));
    }
    const picked = shapes.join("") ? `<path class="${focused ? "ed-sel" : "ed-sel idle"}" d="${shapes.join("")}"/>` : "";
    if (picked !== this.pickedHtml) {
      this.picked.innerHTML = picked;
      this.pickedHtml = picked;
    }
    for (const sel of sels) {
      const row = visible(sel.head.line);
      if (row >= 0) {
        over.push(this.box(sel === primary ? "ed-caret main" : "ed-caret", this.xOf(sel.head) - 1, row * LINE, 2));
      }
    }
    const pair = level === 0 ? this.bracketPair() : null;
    if (pair) {
      for (const at of pair) {
        const row = visible(at.line);
        if (row >= 0) {
          over.push(this.span("ed-pair", at.line, row, at.col, at.col + 1));
        }
      }
    }

    const rowTop = (row) => row * LINE;
    this.textLayer.draw(text, rowTop);
    this.underLayer.draw(new Map([...under].map(([row, html]) => [row, ["ed-band", html]])));
    this.gutterLayer.draw(gutter, rowTop);
    const marks = over.join("");
    if (this.overHtml !== marks) {
      this.over.innerHTML = marks;
      this.overHtml = marks;
    }
    this.gutterRows.style.transform = `translateY(${-top}px)`;
    const headRow = rows.rowOf(primary.head.line);
    this.input.style.left = `${this.xOf(primary.head)}px`;
    this.input.style.top = `${headRow * LINE}px`;
    // Off the rows on screen: the map and the status line wait while input is active.
    if (!status.input.active) {
      this.minimap.paint(level);
      this.drawStatus();
    }
    this.drawSticky(rows, top);
    this.suggest.place();
    this.strained(performance.now() - began);
  }

  // Moves the cursor to the next of the language server's diagnostics after it, or the one before
  // it where `dir` is -1, round from the end to the start, and says what it is.
  stepProblem(dir) {
    const s = this.s;
    const all = [...(s?.diagnostics ?? [])].sort((a, b) => a.from.line - b.from.line || a.from.col - b.from.col);
    if (!all.length) {
      return null;
    }
    const head = this.primary().head;
    const at = { line: head.line + s.base, col: head.col };
    const after = (diag) => diag.from.line > at.line || (diag.from.line === at.line && diag.from.col > at.col);
    const before = (diag) => diag.from.line < at.line || (diag.from.line === at.line && diag.from.col < at.col);
    const found = dir > 0 ? all.find(after) ?? all[0] : [...all].reverse().find(before) ?? all.at(-1);
    this.goTo(found.from.line - s.base, found.from.col);
    return found;
  }

  // The status line, built again only when what it says changes.
  drawStatus() {
    const s = this.s;
    const primary = this.primary();
    const head = primary.head;
    let picked = "";
    const spans = s.selections.filter((sel) => !empty(sel));
    if (spans.length) {
      const lines = spans.reduce((sum, sel) => sum + endOfSel(sel).line - startOf(sel).line, 0);
      const chars = spans.reduce((sum, sel) => sum + (startOf(sel).line === endOfSel(sel).line ? endOfSel(sel).col - startOf(sel).col : 0), 0);
      picked = lines ? `${lines + spans.length} lines selected` : `${chars} selected`;
    }
    const read = s.window ? `${Math.floor((100 * (s.window.end - s.window.start)) / Math.max(1, s.window.size))}% read` : "";
    const errors = (s.diagnostics ?? []).filter((diag) => diag.severity === 1).length;
    const warnings = (s.diagnostics ?? []).filter((diag) => diag.severity === 2).length;
    const said = [s.base + head.line, this.vcol(head), picked, s.selections.length, read, s.indent.tabs, s.indent.size, s.doc.eol, s.language?.id, errors, warnings, s.readOnly].join("|");
    if (said === this.statusSaid) {
      return;
    }
    this.statusSaid = said;
    const parts = [];
    const where = document.createElement("button");
    where.type = "button";
    where.textContent = `${s.base + head.line + 1}:${this.vcol(head) + 1}`;
    where.title = `Line ${s.base + head.line + 1}, column ${this.vcol(head) + 1}: Go to Line (Ctrl+G)`;
    where.addEventListener("click", () => this.goto.open());
    parts.push(where);
    if (picked) {
      parts.push(Object.assign(document.createElement("span"), { textContent: picked }));
    }
    if (read) {
      parts.push(Object.assign(document.createElement("span"), { className: "reading", textContent: read }));
    }
    if (s.selections.length > 1) {
      parts.push(Object.assign(document.createElement("span"), { textContent: `${s.selections.length} cursors` }));
    }
    // Then the line ends, the encoding every file is read and written in, the indent, which a press
    // turns between tabs and spaces, and the lock, which a press turns where the editor's owner says
    // the text can be written.
    const eol = Object.assign(document.createElement("span"), { textContent: s.doc.eol === "\r\n" ? "CRLF" : "LF", title: "Line ends" });
    const encoding = Object.assign(document.createElement("span"), { textContent: "UTF-8", title: "Encoding" });
    const indent = document.createElement("button");
    indent.type = "button";
    indent.textContent = s.indent.tabs ? `Tab ${s.indent.size}` : `${s.indent.size} spaces`;
    indent.title = s.indent.tabs ? "Indent with tabs: a press indents with spaces" : "Indent with spaces: a press indents with tabs";
    indent.addEventListener("click", () => {
      s.indent.tabs = !s.indent.tabs;
      this.paint();
    });
    const lock = document.createElement("button");
    lock.type = "button";
    lock.className = "status-lock";
    lock.title = s.readOnly ? "Read-only: a press makes it writable" : "Writable: a press makes it read-only";
    lock.setAttribute("aria-label", s.readOnly ? "Read-only" : "Writable");
    lock.append(icon(s.readOnly ? "lock" : "unlock"));
    lock.addEventListener("click", () => this.onLock?.(s));
    parts.push(eol, encoding, indent, lock);
    this.status.replaceChildren(...parts);
  }

  // Sticky scroll: the line that opens each region the editor's top row is inside, held along the
  // top, the outermost first. A click on one goes to it, and the wheel over them scrolls the text.
  setSticky(on) {
    this.stickyOn = on;
    localStorage.setItem(STICKY_KEY, String(on));
    this.stickyKey = "";
    this.schedule();
  }

  // The regions of the text, each [first line, last line] in order of the first, read again a moment
  // after an edit; until then the ones before it.
  stickyRegions() {
    const s = this.s;
    if (this.stickyFor !== s || this.stickyDoc !== s.doc.id) {
      if (!this.stickyWait) {
        this.stickyWait = window.setTimeout(
          () => {
            this.stickyWait = 0;
            const now = this.s;
            if (!now) {
              return;
            }
            this.stickyList = now.doc.count > STICKY_LINES ? [] : [...now.regions()].sort((a, b) => a[0] - b[0]);
            this.stickyFor = now;
            this.stickyDoc = now.doc.id;
            this.schedule();
          },
          this.stickyFor === s ? STICKY_REST : 0
        );
      }
      if (this.stickyFor !== s) {
        return [];
      }
    }
    return this.stickyList.filter(([start]) => start < s.doc.count);
  }

  drawSticky(rows, top) {
    const s = this.s;
    if (!this.stickyOn || status.scroll.level >= 2) {
      this.sticky.hidden = true;
      this.stickyKey = "";
      return;
    }
    const list = this.stickyRegions();
    const firstRow = Math.floor(top / LINE);
    let held = [];
    // The held lines cover rows of their own, and they hold the regions of the row under them.
    for (let pass = 0; pass < 3; pass += 1) {
      const line = rows.lineOf(Math.min(rows.size - 1, firstRow + held.length));
      const next = list.filter(([start, end]) => start < line && end >= line).map(([start]) => start).slice(-STICKY_MOST);
      if (next.join() === held.join()) {
        break;
      }
      held = next;
    }
    if (top <= 0) {
      held = [];
    }
    const left = this.scroller.scrollLeft;
    const gutter = this.gutter.offsetWidth;
    const key = `${held.join()}|${s.doc.id}|${left}|${gutter}|${this.scroller.clientWidth}|${s.base}`;
    if (key === this.stickyKey) {
      return;
    }
    this.stickyKey = key;
    this.sticky.hidden = !held.length;
    if (!held.length) {
      return;
    }
    this.sticky.style.width = `${gutter + this.scroller.clientWidth}px`;
    this.sticky.innerHTML = held
      .map(
        (line) =>
          `<div class="ed-sticky-row" data-line="${line}" style="height:${LINE}px"><span class="ed-sticky-num" style="width:${gutter}px">${s.base + line + 1}</span><span class="ed-sticky-text"><span style="display:inline-block;padding-left:${PAD}px;transform:translateX(${-left}px)">${this.rowHtml(line)}</span></span></div>`
      )
      .join("");
  }

  // Colors the minimap reads from the stylesheet, read again once the scheme changes.
  refreshColors() {
    this.minimap.colors = null;
    this.schedule();
  }

  // The rectangle, in the page, of a place's row.
  rectOf(p) {
    const rect = this.space.getBoundingClientRect();
    const row = this.rows().rowOf(p.line);
    return { left: rect.left + this.xOf(p), top: rect.top + row * LINE, bottom: rect.top + (row + 1) * LINE };
  }

  // The word before the primary cursor, which completion replaces.
  prefix() {
    return wordBefore(this.doc, this.primary().head);
  }
}

export { LINE, PAD };
