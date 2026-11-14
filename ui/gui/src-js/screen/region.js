// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The escape sequences' work on a screen: scrolling its region, erasing, saving the cursor, the second screen, the
// CSI sequences and their modes, and the colors and weight of what is printed next. Each is a method of Screen
// (screen.js), `this` the screen it runs for.

import { PLAIN, blank, bufferOf, lineHtml, lineOf, restyled } from "./cells.js";

// Scrolls the region up n lines. On the main screen, with the region the whole screen, each line
// that leaves the top is retained, and its row goes with it.
export function scrollUp(count) {
  const { lines, rows } = this.buffer;
  for (let at = 0; at < Math.min(count, this.bottom - this.top + 1); at += 1) {
    if (this.buffer === this.main && this.top === 0) {
      const gone = lines.shift();
      const row = rows.shift();
      if (gone.dirty || gone === this.cursorDrawn) {
        row.innerHTML = lineHtml(gone, -1);
      }
      this.retainRow(row);
      lines.splice(this.bottom, 0, lineOf(this.cols, this.style));
      const fresh = document.createElement("div");
      rows.splice(this.bottom, 0, fresh);
      this.buffer.host.insertBefore(fresh, rows[this.bottom + 1] ?? null);
      if (this.cursorDrawn === gone) {
        this.cursorDrawn = null;
      }
    } else {
      lines.splice(this.top, 1);
      lines.splice(this.bottom, 0, lineOf(this.cols, this.style));
      for (let y = this.top; y <= this.bottom; y += 1) {
        lines[y].dirty = true;
      }
    }
  }
}

export function scrollDown(count) {
  const { lines } = this.buffer;
  for (let at = 0; at < Math.min(count, this.bottom - this.top + 1); at += 1) {
    lines.splice(this.bottom, 1);
    lines.splice(this.top, 0, lineOf(this.cols, this.style));
  }
  for (let y = this.top; y <= this.bottom; y += 1) {
    lines[y].dirty = true;
  }
}

// Blanks lines from `from` up to, not including, `to`.
export function eraseLines(from, to) {
  for (let y = Math.max(0, from); y < Math.min(this.rowCount, to); y += 1) {
    this.buffer.lines[y] = lineOf(this.cols, this.style);
  }
}

export function eraseCells(y, from, to) {
  const cells = this.buffer.lines[y].cells;
  const cell = blank(this.style);
  for (let x = Math.max(0, from); x < Math.min(this.cols, to); x += 1) {
    cells[x] = cell;
  }
  this.touch(y);
}

export function save() {
  this.saved = { x: this.x, y: this.y, style: this.style, wrapNext: this.wrapNext };
}

export function restore() {
  const saved = this.saved ?? { x: 0, y: 0, style: PLAIN, wrapNext: false };
  this.moveTo(saved.x, saved.y);
  this.style = saved.style;
  this.wrapNext = saved.wrapNext;
}

export function enterAlt() {
  if (this.buffer !== this.main) {
    return;
  }
  this.save();
  this.altHost.replaceChildren();
  this.alt = bufferOf(this.cols, this.rowCount, this.altHost);
  this.buffer = this.alt;
  for (const part of [this.above, this.head, this.between, this.tail]) {
    part.hidden = true;
  }
  this.mainHost.hidden = true;
  this.altHost.hidden = false;
  this.top = 0;
  this.bottom = this.rowCount - 1;
}

export function leaveAlt() {
  if (this.buffer === this.main) {
    return;
  }
  this.buffer = this.main;
  this.alt = null;
  this.altHost.hidden = true;
  this.altHost.replaceChildren();
  for (const part of [this.above, this.head, this.between, this.tail]) {
    part.hidden = false;
  }
  this.mainHost.hidden = false;
  this.top = 0;
  this.bottom = this.rowCount - 1;
  this.main.lines.forEach((line) => (line.dirty = true));
  this.restore();
}

export function csi(body, final) {
  const lead = /^[?>=!]/.test(body) ? body[0] : "";
  const rest = lead ? body.slice(1) : body;
  const between = rest.replace(/[\d;:]/g, "");
  const numbers = rest
    .replace(/[^\d;:]/g, "")
    .split(/[;:]/)
    .map((part) => (part === "" ? null : Number(part)));
  const n = (at, fallback) => numbers[at] ?? fallback;
  const count = Math.max(1, n(0, 1));
  if (lead === "?") {
    if (final === "h" || final === "l") {
      numbers.forEach((mode) => this.privateMode(mode, final === "h"));
    }
    return;
  }
  if (lead === ">" || lead === "=") {
    if (final === "c") {
      this.answer("\x1b[>0;0;0c");
    }
    return;
  }
  if (between) {
    return;
  }
  switch (final) {
    case "@": {
      const cells = this.buffer.lines[this.y].cells;
      cells.splice(this.x, 0, ...Array.from({ length: count }, () => blank(this.style)));
      cells.length = this.cols;
      this.touch();
      return;
    }
    case "A":
      this.moveTo(this.x, Math.max(this.y - count, this.y >= this.top ? this.top : 0));
      return;
    case "B":
      this.moveTo(this.x, Math.min(this.y + count, this.y <= this.bottom ? this.bottom : this.rowCount - 1));
      return;
    case "C":
      this.moveTo(this.x + count, this.y);
      return;
    case "D":
      this.moveTo(this.x - count, this.y);
      return;
    case "E":
      this.moveTo(0, this.y + count);
      return;
    case "F":
      this.moveTo(0, this.y - count);
      return;
    case "G":
    case "`":
      this.moveTo(count - 1, this.y);
      return;
    case "H":
    case "f":
      this.moveTo(Math.max(1, n(1, 1)) - 1, count - 1);
      return;
    case "d":
      this.moveTo(this.x, count - 1);
      return;
    case "J":
      this.eraseDisplay(n(0, 0));
      return;
    case "K": {
      const how = n(0, 0);
      this.eraseCells(this.y, how === 0 ? this.x : 0, how === 1 ? this.x + 1 : this.cols);
      return;
    }
    case "L":
    case "M": {
      if (this.y < this.top || this.y > this.bottom) {
        return;
      }
      const { lines } = this.buffer;
      for (let at = 0; at < Math.min(count, this.bottom - this.y + 1); at += 1) {
        if (final === "L") {
          lines.splice(this.bottom, 1);
          lines.splice(this.y, 0, lineOf(this.cols, this.style));
        } else {
          lines.splice(this.y, 1);
          lines.splice(this.bottom, 0, lineOf(this.cols, this.style));
        }
      }
      for (let y = this.y; y <= this.bottom; y += 1) {
        lines[y].dirty = true;
      }
      this.moveTo(0, this.y);
      return;
    }
    case "P": {
      const cells = this.buffer.lines[this.y].cells;
      cells.splice(this.x, Math.min(count, this.cols - this.x));
      while (cells.length < this.cols) {
        cells.push(blank(this.style));
      }
      this.touch();
      return;
    }
    case "X":
      this.eraseCells(this.y, this.x, this.x + count);
      return;
    case "S":
      this.scrollUp(count);
      return;
    case "T":
      this.scrollDown(count);
      return;
    case "m":
      this.sgr(numbers.length ? numbers : [0]);
      return;
    case "r":
      this.top = Math.max(0, n(0, 1) - 1);
      this.bottom = Math.min(this.rowCount - 1, n(1, this.rowCount) - 1);
      if (this.bottom <= this.top) {
        this.top = 0;
        this.bottom = this.rowCount - 1;
      }
      this.moveTo(0, 0);
      return;
    case "s":
      this.save();
      return;
    case "u":
      this.restore();
      return;
    case "h":
    case "l":
      if (n(0, 0) === 4) {
        this.modes.insert = final === "h";
      }
      return;
    case "n":
      if (n(0, 0) === 6) {
        this.answer(`\x1b[${this.y + 1};${this.x + 1}R`);
      } else if (n(0, 0) === 5) {
        this.answer("\x1b[0n");
      }
      return;
    case "c":
      this.answer("\x1b[?1;2c");
      return;
  }
}

export function eraseDisplay(how) {
  if (how === 0) {
    this.eraseCells(this.y, this.x, this.cols);
    this.eraseLines(this.y + 1, this.rowCount);
  } else if (how === 1) {
    this.eraseLines(0, this.y);
    this.eraseCells(this.y, 0, this.x + 1);
  } else if (how === 2) {
    this.eraseLines(0, this.rowCount);
  } else if (how === 3) {
    this.head.replaceChildren();
    this.tail.replaceChildren();
    this.headFrom = this.written;
    this.headTo = this.written;
    this.tailFrom = this.written;
    this.floor = this.written;
    this.sizeGaps();
  }
}

export function privateMode(mode, on) {
  switch (mode) {
    case 1:
      this.modes.appCursor = on;
      return;
    case 7:
      this.modes.wrap = on;
      return;
    case 25:
      this.modes.cursor = on;
      this.touch();
      return;
    case 47:
    case 1047:
    case 1049:
      if (on) {
        this.enterAlt();
        if (mode !== 1049) {
          this.eraseLines(0, this.rowCount);
        }
      } else {
        this.leaveAlt();
      }
      return;
    case 2004:
      this.modes.paste = on;
      return;
  }
}

// Select Graphic Rendition: the colors and weight of what is printed next.
export function sgr(numbers) {
  this.style = restyled(this.style, numbers);
}
