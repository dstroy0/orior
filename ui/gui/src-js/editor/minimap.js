// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The text drawn small down the editor's right edge, each word a bar in its token's color, with
// the part on screen marked by a slider that drags. Where every row fits, the map shows them all;
// where they do not, it scrolls with the text so the slider stays over the part on screen. Along its
// right edge the overview strip marks the whole text.

const ROW = 2;
const CHAR = 1;
const MARGIN = 4;
const WIDEST = 160;
const PLAIN = [[0, ""]];

// The overview strip down the map's right edge: the whole text at once, a lane for how lines differ
// from the last commit and a lane for find's matches and the cursors, each mark at least MARK tall.
// A press on it goes to that part of the text.
const STRIP = 8;
const MARK = 2;

export class Minimap {
  constructor(editor, canvas) {
    this.ed = editor;
    this.canvas = canvas;
    this.colors = null;
    canvas.addEventListener("mousedown", (event) => this.down(event));
    // The wheel over the map scrolls the text, as it does over the text.
    canvas.addEventListener("wheel", (event) => editor.onWheel(event), { passive: false });
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
      this.colorsMade = (this.colorsMade ?? 0) + 1;
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

  // The canvas the map's lines are kept in, the size of the map in the screen's pixels.
  linesCanvas(width, height) {
    if (!this.lines) {
      this.lines = document.createElement("canvas");
    }
    if (this.lines.width !== width || this.lines.height !== height) {
      this.lines.width = width;
      this.lines.height = height;
      this.linesFor = null;
    }
    return this.lines;
  }

  // The map's size, as the editor's resize observer last gave it.
  size() {
    this.kept ??= { width: this.canvas.clientWidth, height: this.canvas.clientHeight };
    return this.kept;
  }

  // Where the map stands beside the view, the view scrolled to `scrollTop`, the scroller's own.
  geometry(scrollTop = this.ed.scroller.scrollTop) {
    const ed = this.ed;
    const rows = ed.rows();
    const height = this.size().height;
    const fit = Math.max(1, Math.floor(height / ROW));
    const view = ed.viewSize();
    const range = Math.max(1, (ed.spaceHeight ?? ed.scroller.scrollHeight) - view.height);
    const onScreen = view.height / ed.lineHeight;
    const s = ed.s;
    const viewRow = (scrollTop - ed.pad) / ed.lineHeight;
    // The view's line in the whole file. Lines read in above the view leave it where it is.
    const fileTop = (s?.base ?? 0) + viewRow;
    let start = 0;
    if (s?.window) {
      // A file stored as pages: the map stands where the view stands in the whole file, its line count as the index
      // counts it, and not in the window kept now, so the slider moves with the view as the window moves under it.
      // The map's rows above or below the window stand empty until the window comes to them.
      const total = ed.linesInFile();
      const ratio = Math.min(1, Math.max(0, fileTop / Math.max(1, total - onScreen)));
      start = Math.round(ratio * Math.max(0, total - fit)) - s.base;
    } else {
      if (this.anchor && this.anchor.session === s && this.anchor.fileTop === fileTop) {
        // The view has not moved since the map last stood: the map stands where it did beside it.
        start = viewRow - this.anchor.offset;
      } else if (rows.size > fit) {
        start = Math.round(Math.min(1, scrollTop / range) * (rows.size - fit));
      }
      start = Math.round(Math.max(0, Math.min(Math.max(0, rows.size - fit), start)));
    }
    this.anchor = { session: s, fileTop, offset: viewRow - start };
    const sliderTop = (viewRow - start) * ROW;
    return { rows, fit, start, range, onScreen, sliderTop, sliderHeight: Math.max(8, onScreen * ROW) };
  }

  // The lines of the file the map stands beside, from and to before, the view where its last scroll left it.
  fileSpan() {
    const ed = this.ed;
    const { start, fit } = this.geometry(ed.lastTop ?? ed.scroller.scrollTop);
    return [ed.s.base + start, ed.s.base + start + fit];
  }

  paint(scrollTop = this.ed.scroller.scrollTop) {
    const ed = this.ed;
    const s = ed.s;
    const canvas = this.canvas;
    const ratio = window.devicePixelRatio || 1;
    const { width, height } = this.size();
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
    const { rows, fit, start, sliderTop, sliderHeight } = this.geometry(scrollTop);
    // the rows of the map the window holds: a paged file's map reaches past the window on either side
    const first = Math.max(0, start);
    const end = Math.max(first, Math.min(rows.size, start + fit));
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
      const top = Math.max(first, rows.rowOf(from.line));
      const bottom = Math.min(end - 1, rows.rowOf(to.line));
      if (bottom >= top) {
        context.fillRect(0, (top - start) * ROW, width, (bottom - top + 1) * ROW);
      }
    }
    if (ed.find.shown && end > first) {
      context.fillStyle = this.colorOf("ed-mini-match");
      for (const match of ed.find.between(rows.lineOf(first), rows.lineOf(end - 1))) {
        context.fillRect(0, (rows.rowOf(match.from.line) - start) * ROW, width, ROW);
      }
    }
    // The lines, from the map's own canvas of them, which keeps the rows in view drawn. A scroll moves
    // what it holds and draws only the rows that come into view; a change to the text, its folding,
    // the colors, the runs that come for its lines, the detail or the map's size draws every row in
    // view again.
    const lines = this.linesCanvas(Math.round(width * ratio), Math.round(height * ratio));
    const pen = lines.getContext("2d");
    const kept = this.linesFor;
    const key = [s.doc.id, s.foldings, s.base, rows.size, this.colorsMade, s.highlight.came, lines.width, lines.height].join("|");
    const moved = kept && kept.session === s && kept.key === key ? start - kept.start : null;
    let from = first;
    let to = end;
    if (moved !== null && Math.abs(moved) < fit) {
      if (moved !== 0) {
        // The rows still in view move up or down by the rows the map moved, and only those it
        // brought into view are drawn.
        pen.setTransform(1, 0, 0, 1, 0, 0);
        pen.globalCompositeOperation = "copy";
        pen.drawImage(lines, 0, -moved * ROW * ratio);
        pen.globalCompositeOperation = "source-over";
        if (moved > 0) {
          from = Math.max(first, end - moved);
          pen.clearRect(0, (from - start) * ROW * ratio, lines.width, lines.height);
        } else {
          to = Math.min(end, start - moved);
          pen.clearRect(0, 0, lines.width, (to - start) * ROW * ratio);
        }
      } else {
        to = from;
      }
    } else {
      pen.setTransform(1, 0, 0, 1, 0, 0);
      pen.clearRect(0, 0, lines.width, lines.height);
    }
    this.linesFor = { session: s, key, start };
    pen.setTransform(ratio, 0, 0, ratio, 0, 0);
    pen.globalAlpha = 0.8;
    // Each word a bar on the path of its color, every path filled once: the canvas takes a fill a color and not a
    // fill a word.
    const paths = new Map();
    const bar = (color, x, y, wide) => {
      let path = paths.get(color);
      if (!path) {
        path = new Path2D();
        paths.set(color, path);
      }
      path.rect(MARGIN + x * CHAR, y, wide * CHAR, ROW - 0.5);
    };
    for (let row = from; row < to; row += 1) {
      const line = rows.lineOf(row);
      const text = doc.line(line);
      const runs = s.highlight.runsOf(line);
      const y = (row - start) * ROW;
      let v = 0;
      let index = 0;
      for (let run = 0; run < runs.length && v < WIDEST; run += 1) {
        const stop = runs[run + 1]?.[0] ?? text.length;
        const color = this.colorOf(runs[run][1] || "t-plain");
        let begun = -1;
        for (; index < stop && v < WIDEST; index += 1) {
          const char = text[index];
          const blank = char === " " || char === "\t";
          if (!blank && begun < 0) {
            begun = v;
          }
          if (blank && begun >= 0) {
            bar(color, begun, y, v - begun);
            begun = -1;
          }
          v += char === "\t" ? size - (v % size) : 1;
        }
        if (begun >= 0) {
          bar(color, begun, y, v - begun);
        }
      }
    }
    for (const [color, path] of paths) {
      pen.fillStyle = color;
      pen.fill(path);
    }
    pen.globalAlpha = 1;
    context.setTransform(1, 0, 0, 1, 0, 0);
    // A minimap with no room, as one hidden, has lines of no size, which a canvas cannot draw from.
    if (lines.width && lines.height) {
      context.drawImage(lines, 0, 0);
    }
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.fillStyle = this.colorOf(this.dragging ? "ed-mini-slider-on" : "ed-mini-slider");
    context.fillRect(0, sliderTop, width, sliderHeight);
    this.paintStrip(context, width, height, rows);
  }

  paintStrip(context, width, height, rows) {
    const ed = this.ed;
    const s = ed.s;
    const left = width - STRIP;
    // The strip stands for the whole file, a file still being read among them.
    const size = Math.max(1, ed.allRows());
    const above = ed.pad / ed.lineHeight;
    const tall = Math.max(MARK, height / size);
    const yOf = (line) => ((above + rows.rowOf(Math.min(line, s.doc.count - 1))) / size) * height;
    context.fillStyle = this.colorOf("ed-bg");
    context.fillRect(left, 0, STRIP, height);
    context.fillStyle = this.colorOf("ed-widget-line");
    context.fillRect(left, 0, 1, height);
    const changes = s.changes;
    if (changes) {
      for (const [set, color] of [
        [changes.added, "ed-added"],
        [changes.changed, "ed-changed"],
      ]) {
        context.fillStyle = this.colorOf(color);
        for (const line of set) {
          context.fillRect(left + 1, yOf(line), 3, tall);
        }
      }
      context.fillStyle = this.colorOf("ed-removed");
      for (const line of changes.removed) {
        context.fillRect(left + 1, Math.max(0, yOf(line) - 1), 3, MARK);
      }
    }
    if (ed.find.shown) {
      context.fillStyle = this.colorOf("ed-mini-match");
      for (const match of ed.find.between(0, s.doc.count - 1)) {
        context.fillRect(left + 4, yOf(match.from.line), STRIP - 4, tall);
      }
    }
    context.fillStyle = this.colorOf("ed-caret");
    for (const sel of s.selections) {
      context.fillRect(left + 4, yOf(sel.head.line), STRIP - 4, MARK);
    }
  }

  // The row of the text at `y`, the map's own pixels from its top, the map standing as `found` says.
  rowAt(y, found = this.geometry()) {
    return found.start + y / ROW;
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
    if (event.clientX - box.left >= box.width - STRIP) {
      ed.scroller.scrollTop = (y / box.height) * ed.allRows() * ed.lineHeight - ed.scroller.clientHeight / 2;
      ed.focus();
      return;
    }
    if (y < found.sliderTop || y > found.sliderTop + found.sliderHeight) {
      const row = this.rowAt(y, found);
      ed.place(row * ed.lineHeight - ed.scroller.clientHeight / 2);
      found = this.geometry();
    }
    const startY = event.clientY;
    const startTop = ed.scroller.scrollTop;
    // the slider's run down the map, the rows the map stands for counted in the whole file where it is stored as pages
    const all = ed.s.window ? ed.linesInFile() : found.rows.size;
    const span = Math.max(1, (Math.min(all, found.fit) - found.onScreen) * ROW);
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
