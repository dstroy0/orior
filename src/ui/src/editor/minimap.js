// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The text drawn small down the editor's right edge, each word a bar in its token's color, with
// the part on screen marked by a slider that drags. Where every row fits, the map shows them all;
// where they do not, it scrolls with the text so the slider stays over the part on screen.

const ROW = 2;
const CHAR = 1;
const MARGIN = 4;
const WIDEST = 160;
const PLAIN = [[0, ""]];

export class Minimap {
  constructor(editor, canvas) {
    this.ed = editor;
    this.canvas = canvas;
    this.colors = null;
    canvas.addEventListener("mousedown", (event) => this.down(event));
  }

  clear() {
    const context = this.canvas.getContext("2d");
    context.setTransform(1, 0, 0, 1, 0, 0);
    context.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }

  // A token's color from the stylesheet: the most particular class with a color given wins.
  colorOf(name) {
    if (!this.colors) {
      this.style = getComputedStyle(this.ed.host);
      this.colors = new Map();
    }
    let found = this.colors.get(name);
    if (found === undefined) {
      found = "";
      for (const part of name.split(" ").reverse()) {
        const value = this.style.getPropertyValue(`--${part}`).trim();
        if (value) {
          found = value;
          break;
        }
      }
      found = found || this.style.getPropertyValue("--ed-fg").trim() || "#888";
      this.colors.set(name, found);
    }
    return found;
  }

  geometry() {
    const ed = this.ed;
    const rows = ed.rows();
    const height = this.canvas.clientHeight;
    const fit = Math.max(1, Math.floor(height / ROW));
    const scroller = ed.scroller;
    const range = Math.max(1, scroller.scrollHeight - scroller.clientHeight);
    const ratio = Math.min(1, scroller.scrollTop / range);
    const start = rows.size <= fit ? 0 : Math.round(ratio * (rows.size - fit));
    const onScreen = scroller.clientHeight / ed.lineHeight;
    const sliderTop = (scroller.scrollTop / ed.lineHeight - start) * ROW;
    return { rows, fit, start, range, onScreen, sliderTop, sliderHeight: Math.max(8, onScreen * ROW) };
  }

  // `level` is the editor's: from 2 the map takes only the colors already worked out, and at 3 it
  // draws every line in one color.
  paint(level = 0) {
    const ed = this.ed;
    const s = ed.s;
    const canvas = this.canvas;
    const ratio = window.devicePixelRatio || 1;
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    if (canvas.width !== Math.round(width * ratio) || canvas.height !== Math.round(height * ratio)) {
      canvas.width = Math.round(width * ratio);
      canvas.height = Math.round(height * ratio);
    }
    const context = canvas.getContext("2d");
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);
    if (!s || !width) {
      return;
    }
    const { rows, fit, start, sliderTop, sliderHeight } = this.geometry();
    const end = Math.min(rows.size, start + fit);
    const size = s.indent.size;
    const doc = s.doc;
    const marks = this.colorOf("ed-mini-sel");
    for (const sel of s.selections) {
      const from = sel.anchor.line < sel.head.line ? sel.anchor : sel.head;
      const to = from === sel.anchor ? sel.head : sel.anchor;
      if (from.line === to.line && from.col === to.col) {
        continue;
      }
      context.fillStyle = marks;
      const top = Math.max(start, rows.rowOf(from.line));
      const bottom = Math.min(end - 1, rows.rowOf(to.line));
      if (bottom >= top) {
        context.fillRect(0, (top - start) * ROW, width, (bottom - top + 1) * ROW);
      }
    }
    if (ed.find.shown) {
      context.fillStyle = this.colorOf("ed-mini-match");
      for (const match of ed.find.between(rows.lineOf(start), rows.lineOf(end - 1))) {
        context.fillRect(0, (rows.rowOf(match.from.line) - start) * ROW, width, ROW);
      }
    }
    context.globalAlpha = 0.8;
    for (let row = start; row < end; row += 1) {
      const line = rows.lineOf(row);
      const text = doc.line(line);
      const runs = level >= 3 ? PLAIN : level === 2 ? s.highlight.cached(line) ?? PLAIN : s.highlight.runsOf(line);
      const y = (row - start) * ROW;
      let v = 0;
      let index = 0;
      for (let run = 0; run < runs.length && v < WIDEST; run += 1) {
        const stop = runs[run + 1]?.[0] ?? text.length;
        context.fillStyle = this.colorOf(runs[run][1] || "t-plain");
        let from = -1;
        for (; index < stop && v < WIDEST; index += 1) {
          const char = text[index];
          const blank = char === " " || char === "\t";
          if (!blank && from < 0) {
            from = v;
          }
          if (blank && from >= 0) {
            context.fillRect(MARGIN + from * CHAR, y, (v - from) * CHAR, ROW - 0.5);
            from = -1;
          }
          v += char === "\t" ? size - (v % size) : 1;
        }
        if (from >= 0) {
          context.fillRect(MARGIN + from * CHAR, y, (v - from) * CHAR, ROW - 0.5);
        }
      }
    }
    context.globalAlpha = 1;
    context.fillStyle = this.colorOf(this.dragging ? "ed-mini-slider-on" : "ed-mini-slider");
    context.fillRect(0, sliderTop, width, sliderHeight);
  }

  down(event) {
    const ed = this.ed;
    if (!ed.s || event.button !== 0) {
      return;
    }
    event.preventDefault();
    const box = this.canvas.getBoundingClientRect();
    const y = event.clientY - box.top;
    let found = this.geometry();
    if (y < found.sliderTop || y > found.sliderTop + found.sliderHeight) {
      const row = found.start + y / ROW;
      ed.scroller.scrollTop = row * ed.lineHeight - ed.scroller.clientHeight / 2;
      found = this.geometry();
    }
    const startY = event.clientY;
    const startTop = ed.scroller.scrollTop;
    const span = Math.max(1, (Math.min(found.rows.size, found.fit) - found.onScreen) * ROW);
    this.dragging = true;
    const move = (moved) => {
      ed.scroller.scrollTop = startTop + ((moved.clientY - startY) * found.range) / span;
    };
    const up = () => {
      this.dragging = false;
      window.removeEventListener("mousemove", move);
      window.removeEventListener("mouseup", up);
      ed.schedule();
    };
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", up);
    ed.focus();
  }
}
