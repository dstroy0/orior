// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor: a session drawn a screen of rows at a time, and every key, pointer and clipboard
// action that edits it or moves through it.
//
// Only the rows on screen are in the page. The text sits in a scrolling space as tall as every
// row. The scroll bars are the page's own, and each frame draws the rows the space shows. Keys
// and typing arrive at a hidden text area that sits at the primary cursor, which is also where an
// input method opens its window.

import { least, most, same } from "./document.js";
import { motionReduced } from "../motion.js";
import { Find, GoTo } from "./find.js";
import { Layer } from "./layer.js";
import { Minimap } from "./minimap.js";
import { letterOf } from "../keys.js";
import { Hover, Suggest } from "./widgets.js";
import { authorsShown } from "../authors.js";
import * as changes from "./view/changes.js";
import * as editing from "./view/editing.js";
import * as folding from "./view/folding.js";
import * as geometry from "./view/geometry.js";
import * as keys from "./view/keys.js";
import * as layout from "./view/layout.js";
import * as lines from "./view/lines.js";
import * as frame from "./view/frame.js";
import * as pointer from "./view/pointer.js";
import * as drawn from "./view/drawn.js";
import * as scroll from "./view/scroll.js";
import * as selections from "./view/selections.js";
import * as statusline from "./view/statusline.js";

const PAD = 10;
// How wide the gutter's strip for breakpoints is, in pixels.
export const BREAK_STRIP = 16;
// How wide Line History's column is, in characters: a date and an author's name where authors show,
// and a date alone where they do not.
const HISTORY_CHARS = 22;
const DATE_CHARS = 11;
export const historyChars = () => (authorsShown() ? HISTORY_CHARS : DATE_CHARS);
// The height of a row, read from the code's line height each time the editor measures.
let LINE = 20;

// How many rows of padding a paged file's window has above it and below it at most: the scrolling
// space stays within what the page lays out, and the window moves as the view nears its edge.
export const PAGED_PAD = 250000;

// The fewest columns a wrapped row holds, and the most lines' breaks kept.
export const WRAP_LEAST = 20;
export const WRAPS_KEPT = 200000;

// How round a selection's corners are, in pixels.
export const SELECTION_ROUND = 4;

// Rows are placed from an origin, a row near the screen and a whole number of ORIGIN_ROWS from
// the first, and never from the first row itself. A row of a long file placed from the first stands
// millions of pixels down, where the page draws text a pixel off, and a row drawn again there lands
// over what it was and not on it.
export const ORIGIN_ROWS = 512;

// The letters of the scripts that run right to left: Hebrew, Arabic, Syriac, Thaana, N'Ko and the
// rest of the right-to-left blocks, and their presentation forms. A line that holds one is placed as
// the page draws it, each character where it stands, and every other line by its columns.
export const RTL = /[\u0590-\u08FF\uFB1D-\uFDFF\uFE70-\uFEFF\u{10800}-\u{10FFF}\u{1E800}-\u{1EFFF}]/u;

// Whether each character of `text` is drawn running right to left: a letter of such a script is, a
// letter of any other script and a digit are not, and a character of neither, as a space or a mark of
// punctuation, runs as the letters on both sides of it do where they agree, and left to right where
// they do not.
export function runsRightToLeft(text) {
  const strong = [...text].flatMap((char) => {
    const kind = RTL.test(char) ? "R" : /[\p{L}\p{N}]/u.test(char) ? "L" : null;
    return char.length === 2 ? [kind, kind] : [kind];
  });
  const out = new Array(strong.length).fill(false);
  let before = null;
  for (let at = 0; at < strong.length; at += 1) {
    if (strong[at]) {
      out[at] = strong[at] === "R";
      before = strong[at];
      continue;
    }
    let after = null;
    for (let next = at + 1; next < strong.length && !after; next += 1) {
      after = strong[next];
    }
    out[at] = before === "R" && after === "R";
  }
  return out;
}

// Column Selection Mode's key: while it is on, a drag chooses a column, as Shift and Alt do.
export const COLUMN_KEY = "orior.column";

// Sticky scroll: the most lines it holds along the top.
export const STICKY_MOST = 5;

// Smooth scrolling: a wheel's step and a page key glide to where they head on a critically damped
// spring of rate GLIDE_RATE a second, which covers all but a twentieth of its way in about 190 ms. A
// step during a glide moves where it heads and the glide keeps its speed: under a steady wheel each
// frame moves the text an even step, and no frame stands still. The spring moves in pieces of at most GLIDE_PIECE ms, each
// frame by the time since the one before. Flick scrolling: a touchpad's movement runs on once the
// fingers lift, where no event has come for FLICK_REST and the system gives no run of its own, from
// no slower than FLICK_LEAST pixels a millisecond, slowing by a factor of e every FLICK_SLOWS until
// under FLICK_STOP. Each is a setting, on where none is kept.
export const GLIDE_RATE = 25;
export const GLIDE_PIECE = 4;
export const FLICK_REST = 50;
export const FLICK_LEAST = 0.3;
export const FLICK_SLOWS = 325;
export const FLICK_STOP = 0.02;
export const SMOOTH_KEY = "orior.smooth-scroll";
// Smooth scrolling is on unless the reader turned it off, or turned on the system's reduced motion
// and never turned it on here.
export const smoothScrolling = () => (localStorage.getItem(SMOOTH_KEY) ?? String(!motionReduced())) !== "false";

// The kept settings of the reader's formatting: the continuation indent of each place a bracket
// stands, in columns; whether an operator's sign goes to the next line where a line breaks after it
// inside brackets; and the margin documentation is filled to.
export const CONTINUATION_KEY = "orior.continuation";
export const OPERATOR_KEY = "orior.operator-next-line";
export const DOC_MARGIN_KEY = "orior.doc-margin";

// What stands before a bracket that opens a declaration's parameters: `def name`, `function name`,
// `fn name`, or a C declaration's type and name.
export const DECLARES = /(?:\bdef\s+[\w$]+|\bfunction\b\s*\*?\s*[\w$]*|\bfn\s+[\w$]+(?:<[^>]*>)?|^\s*(?!(?:return|else|new|delete|throw|case|await|yield)\b)(?:(?:static|inline|extern|const|unsigned|signed|long|short|struct|enum|virtual)\s+)*[\w:<>]+[\s*&]+[\w$:~]+)$/;

// A line that ends in a binary operator after a value: what stands before the operator, and the
// operator.
export const OPERATOR_END = /^(.*?[\w$)\]}"'`])\s*(\*\*|\/\/|==|!=|<=|>=|&&|\|\||\?\?|\band|\bor|[-+*\/%<>|&^])\s*$/;

// The kept settings of the hints a server writes in the text: inferred types, and parameters' names.
export const HINT_KEYS = { type: "orior.type-hints", parameter: "orior.parameter-hints" };
export const FLICK_KEY = "orior.flick-scroll";
export const STICKY_KEY = "orior.sticky";
export const WRAP_KEY = "orior.word-wrap";

// Bracket pairs colored by depth: the setting's key.
export const BRACKETS_KEY = "orior.brackets";
export const SHOWN = 10000;
// A row is drawn as far as the view reaches across, to the end of the step it reaches into, so that
// a scroll across draws the rows again only as it passes into the next: a line of tens of thousands
// of characters lays out what can be seen, and not the rest. The step is in characters.
export const REACH_STEP = 64;
// The rows a view draws are those in sight first, then out from them a slice of DRAW_SLICE rows at a
// time, as a large file is read, to DRAWN_MOST rows at the most, and DRAWN_PAGED for a file kept as
// pages, whose window moves under the view and whose rows are drawn again as it does.
export const DRAW_SLICE = 32;
export const DRAWN_MOST = 1200;
export const DRAWN_PAGED = 120;
const sessionIds = new WeakMap();
let sessionsSeen = 0;

// The most rows whose markup is kept; past it the kept rows are let go and made again as they show.
export const ROWS_KEPT = 5000;

// What is not drawn: the keys of the settings that mark spaces, tabs and line ends, and that hide
// comments, and the most lines a text has for its lines of comments alone to go out of sight.
export const MARKS_KEY = "orior.whitespace";
export const COMMENTS_KEY = "orior.comments-hidden";

// The key of the setting that lets a cursor go past the end of its line.
export const PAST_KEY = "orior.past-ends";
export const COMMENT_LINES = 200000;

// A row's text, escaped, with a mark laid over each space and each tab, neither moving a letter.
export const marked = (html) => html.replace(/ /g, '<span class="ed-sp"> </span>').replace(/\t/g, '<span class="ed-tab">\t</span>');
export const MAC = /Mac|iPhone|iPad/.test(navigator.platform);
// How long a scroll rests before it counts as stopped, and how many screens ahead the idle coloring
// reaches.
export const REST = 140;
const AHEAD = 3;
// How many rows ahead of the view the idle coloring reaches, for a view `shown` rows high: AHEAD
// screens, or as far as `speed`, in pixels a millisecond, carries the view over two screens' coloring
// of `screenMs` each, where that is farther.
// The height of a row, in pixels, as the editor's zoom sets it.
export const lineHeight = () => LINE;

export const coloringReach =(shown, speed, screenMs) => Math.max(shown * AHEAD, Math.ceil((speed / LINE) * screenMs * 2));

// How long input rests before the work it kept runs.
export const INPUT_REST = 300;

export const startOf = (sel) => least(sel.anchor, sel.head);
export const endOfSel = (sel) => most(sel.anchor, sel.head);
export const empty = (sel) => same(sel.anchor, sel.head);
export const caret = (p, goal = null) => ({ anchor: p, head: p, goal });

// The letters a jump marks places with, those under the fingers at rest first.
export const JUMP_LABELS = "asdfjklghqweruiopzxcvbnmty";

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
export const escapeHtml = (text) => text.replace(/[&<>"]/g, (char) => ESCAPES[char]);

export function div(className) {
  const made = document.createElement("div");
  made.className = className;
  return made;
}

const CODES = {
  BracketLeft: "[", BracketRight: "]", Slash: "/", Backslash: "\\", Period: ".", Comma: ",", Space: "Space",
  NumpadEnter: "Enter", ArrowLeft: "Left", ArrowRight: "Right", ArrowUp: "Up", ArrowDown: "Down",
};

// Keys as the menus write them, Ctrl+Shift+K or Ctrl+K Ctrl+I, named as the key map names them.
export function keysName(keys) {
  const named = (press) => {
    const parts = press.split("+");
    const key = parts.pop() || "+";
    const modifiers = [parts.includes("Ctrl") && "Mod", parts.includes("Alt") && "Alt", parts.includes("Shift") && "Shift"].filter(Boolean);
    return [...modifiers, key].join("+");
  };
  return keys.split(" ").map(named).join(" ");
}

// A key as the key map names it: Mod is Ctrl, or Cmd on a Mac, then Alt and Shift, then the key.
export function keyName(event) {
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
    key = letterOf(event) ?? event.key;
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
  constructor(host, { onCursor, onChange, onEdit, pasteAs, onChangeMark, onHistory, onGroup, onShown, spansOf, pairOf, statusHost = null } = {}) {
    this.host = host;
    this.onGroup = onGroup ?? (() => {});
    this.onChangeMark = onChangeMark ?? (() => {});
    this.onHistory = onHistory ?? (() => {});
    this.onCursor = onCursor ?? (() => {});
    this.onChange = onChange ?? (() => {});
    this.onEdit = onEdit ?? (() => {});
    this.pasteAs = pasteAs ?? (() => null);
    this.onShown = onShown ?? (() => {});
    this.spansOf = spansOf ?? (async () => []);
    this.pairOf = pairOf ?? (async () => null);
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
    // The scrolling space holds nothing and only sizes the scroll. What is drawn stands in a sheet
    // over it that lets the pointer through, moved as the view scrolls, the gutter's numbers with it.
    this.sheet = div("ed-sheet");
    this.layers = div("ed-layers");
    this.layers.append(this.under, this.picked, this.text, this.over, this.input);
    // The formatter's width, a line down the sheet after its last column.
    this.margin = div("ed-margin");
    this.margin.hidden = true;
    this.marginAt = null;
    // What the line is, at its top: the formatter that keeps lines to it and the width.
    this.marginSays = div("ed-margin-says");
    this.margin.append(this.marginSays);
    this.marginShown = false;
    // The documentation's margin, where the reader sets one apart from the code's.
    this.docLine = div("ed-margin doc");
    this.docLine.hidden = true;
    this.docLineAt = null;
    this.sheet.append(this.margin, this.docLine, this.layers);
    this.originRow = 0;
    this.pad = 0;
    this.below = 0;
    this.textLayer = new Layer(this.text, { flow: true });
    this.underLayer = new Layer(this.under);
    this.gutterLayer = new Layer(this.gutterRows);
    this.overHtml = "";
    this.scroller.append(this.space);
    const canvas = document.createElement("canvas");
    canvas.className = "ed-mini";
    this.status = div("ed-status");
    this.status.dataset.item = "editor";
    this.sticky = div("ed-sticky");
    this.sticky.hidden = true;
    this.readSettings();
    // The glide under way, { from, to, at }, in the scrolling space's own units, and the run-on.
    this.glide = null;
    this.flick = null;
    this.stickyKey = "";
    // The lines kept along the top, and the top row and text they were read for.
    this.stickyRows = [];
    this.stickyRowsFor = "";
    host.append(this.gutter, this.scroller, this.sheet, canvas, this.sticky);
    (statusHost ?? host).append(this.status);
    this.minimap = new Minimap(this, canvas);
    // The scroller's inner size and the map's, kept as the page lays them out, read without
    // asking the page to lay itself out again.
    new ResizeObserver(([entry]) => {
      this.viewKept = { width: Math.round(entry.contentRect.width), height: Math.round(entry.contentRect.height) };
      this.schedule();
    }).observe(this.scroller);
    new ResizeObserver(([entry]) => {
      this.minimap.kept = { width: entry.contentRect.width, height: entry.contentRect.height };
      this.schedule();
    }).observe(canvas);
    this.sticky.addEventListener("mousedown", (event) => {
      const row = event.target.closest(".ed-sticky-row");
      if (!row || !this.s) {
        return;
      }
      event.preventDefault();
      const line = Number(row.dataset.line);
      this.goTo(line);
      this.place(this.rows().rowOf(line) * LINE);
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
    this.deferred = new Map();
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

  // A number for each session the view has shown, kept with the session.
  sessionId(s) {
    if (!s) {
      return 0;
    }
    if (!sessionIds.has(s)) {
      sessionsSeen += 1;
      sessionIds.set(s, sessionsSeen);
    }
    return sessionIds.get(s);
  }
}

// The editor's methods kept in editor/view/, put on the class as its own methods are: not counted among an object's keys.
for (const part of [layout, geometry, selections, editing, changes, lines, folding, keys, pointer, scroll, drawn, frame, statusline]) {
  for (const [name, method] of Object.entries(part)) {
    Object.defineProperty(Editor.prototype, name, { value: method, writable: true, configurable: true });
  }
}

export { LINE, PAD };
