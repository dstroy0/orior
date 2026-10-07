// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal's screen: the grid of characters a shell's output draws, read the way an xterm reads
// it, and drawn into the page a line at a time.
//
// The output is text with control characters and escape sequences in it. Printed characters land at
// the cursor in the colors and weight of the moment, and the sequences move the cursor, erase,
// insert and delete, scroll a region of the screen, set colors from the 16, the 256 and any red,
// green and blue, switch to the second screen full-screen programs draw on, and ask the terminal
// where its cursor is and what it is. What the screen cannot do it reads past.
//
// The main screen keeps what scrolls off its top, as many as KEPT lines, and the second screen keeps
// nothing. A line is one row of the page, drawn again only when something in it has changed, and a
// line that scrolls off the top of the main screen takes its row with it into what is kept.

const KEPT = 5000;
const TAB = 8;

// A style is shared by every cell drawn in it and never changed; a new one is made for each change.
const PLAIN = Object.freeze({ fg: null, bg: null, bold: false, dim: false, italic: false, underline: false, inverse: false, strike: false });

const blank = (style) => [" ", style.bg === null ? PLAIN : Object.freeze({ ...PLAIN, bg: style.bg })];

function lineOf(cols, style = PLAIN) {
  const cell = blank(style);
  return { cells: Array.from({ length: cols }, () => cell), dirty: true };
}

// A color as the page draws it: one of the 16 by its class, any other as a CSS color.
function cube(index) {
  if (index >= 232) {
    const gray = 8 + (index - 232) * 10;
    return `rgb(${gray}, ${gray}, ${gray})`;
  }
  const at = index - 16;
  const part = (value) => (value ? 55 + value * 40 : 0);
  return `rgb(${part(Math.floor(at / 36))}, ${part(Math.floor(at / 6) % 6)}, ${part(at % 6)})`;
}

const escapeHtml = (text) => text.replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" })[c]);

// The markup of a run of cells in one style.
function runHtml(text, style, cursor) {
  let fg = style.fg;
  let bg = style.bg;
  const classes = [];
  const inline = [];
  if (style.inverse) {
    [fg, bg] = [bg ?? "default-bg", fg ?? "default-fg"];
  }
  for (const [color, kind] of [
    [fg, "f"],
    [bg, "b"],
  ]) {
    if (color === null) {
      continue;
    }
    if (typeof color === "number" && color < 16) {
      classes.push(`t-${kind}${color}`);
    } else if (color === "default-fg" || color === "default-bg") {
      classes.push(`t-${kind}-${color}`);
    } else {
      inline.push(`${kind === "f" ? "color" : "background"}:${typeof color === "number" ? cube(color) : color}`);
    }
  }
  for (const [on, name] of [
    [style.bold, "t-bold"],
    [style.dim, "t-dim"],
    [style.italic, "t-italic"],
    [style.underline, "t-under"],
    [style.strike, "t-strike"],
    [cursor, "t-cursor"],
  ]) {
    if (on) {
      classes.push(name);
    }
  }
  if (!classes.length && !inline.length) {
    return escapeHtml(text);
  }
  const classAttr = classes.length ? ` class="${classes.join(" ")}"` : "";
  const styleAttr = inline.length ? ` style="${inline.join(";")}"` : "";
  return `<span${classAttr}${styleAttr}>${escapeHtml(text)}</span>`;
}

function lineHtml(line, cursorAt) {
  let html = "";
  let start = 0;
  const cells = line.cells;
  for (let at = 1; at <= cells.length; at += 1) {
    const ends = at === cells.length || cells[at][1] !== cells[start][1] || at === cursorAt || start === cursorAt;
    if (!ends) {
      continue;
    }
    let text = "";
    for (let one = start; one < at; one += 1) {
      text += cells[one][0];
    }
    html += runHtml(text, cells[start][1], start === cursorAt);
    start = at;
  }
  return html || " ";
}

// A screen: its lines, and the rows of the page they are drawn in.
function bufferOf(cols, rows, host) {
  const lines = Array.from({ length: rows }, () => lineOf(cols));
  const rowsOf = lines.map(() => host.appendChild(document.createElement("div")));
  return { lines, rows: rowsOf, host };
}

export class Screen {
  // `view` is the scrolling element the screen is drawn in, and `answer` what the screen says back
  // to the shell, as when it is asked where its cursor is.
  constructor(view, cols, rows, answer) {
    this.view = view;
    this.answer = answer;
    this.atFoot = true;
    // Where the screen last scrolled the view to itself, which says nothing of where the reader is.
    this.placed = 0;
    view.addEventListener("scroll", () => {
      if (view.scrollTop !== this.placed) {
        this.atFoot = view.scrollHeight - view.scrollTop - view.clientHeight < 4;
      }
    });
    this.cols = cols;
    this.rowCount = rows;
    this.kept = view.appendChild(Object.assign(document.createElement("div"), { className: "term-kept" }));
    this.mainHost = view.appendChild(Object.assign(document.createElement("div"), { className: "term-rows" }));
    this.altHost = view.appendChild(Object.assign(document.createElement("div"), { className: "term-rows", hidden: true }));
    this.main = bufferOf(cols, rows, this.mainHost);
    this.alt = null;
    this.buffer = this.main;
    this.reset();
    this.state = "ground";
    this.sequence = "";
    this.focused = false;
    this.cursorDrawn = null;
    this.pending = false;
  }

  reset() {
    this.x = 0;
    this.y = 0;
    this.style = PLAIN;
    this.top = 0;
    this.bottom = this.rowCount - 1;
    this.wrapNext = false;
    this.saved = null;
    this.modes = { appCursor: false, wrap: true, cursor: true, paste: false, insert: false };
  }

  // Reads output from the shell.
  write(text) {
    for (const char of text) {
      this.step(char);
    }
    this.schedule();
  }

  step(char) {
    const code = char.codePointAt(0);
    switch (this.state) {
      case "ground":
        if (code === 0x1b) {
          this.state = "escape";
        } else if (code < 0x20 || code === 0x7f) {
          this.control(code);
        } else {
          this.print(char);
        }
        return;
      case "escape":
        this.escape(char);
        return;
      case "csi":
        if (code >= 0x40 && code <= 0x7e) {
          this.csi(this.sequence, char);
          this.state = "ground";
        } else if (code === 0x1b) {
          this.state = "escape";
        } else if (code >= 0x20) {
          this.sequence += char;
        } else {
          this.control(code);
        }
        return;
      case "osc":
        if (code === 0x07) {
          this.state = "ground";
        } else if (code === 0x1b) {
          this.state = "string-end";
        }
        return;
      case "dcs":
        if (code === 0x1b) {
          this.state = "string-end";
        }
        return;
      case "string-end":
        this.state = char === "\\" ? "ground" : "escape";
        if (this.state === "escape") {
          this.escape(char);
        }
        return;
      case "charset":
        this.state = "ground";
        return;
    }
  }

  control(code) {
    switch (code) {
      case 0x07:
        return;
      case 0x08:
        this.wrapNext = false;
        this.moveTo(this.x - 1, this.y);
        return;
      case 0x09:
        this.moveTo(Math.min(this.cols - 1, (Math.floor(this.x / TAB) + 1) * TAB), this.y);
        return;
      case 0x0a:
      case 0x0b:
      case 0x0c:
        this.lineFeed();
        return;
      case 0x0d:
        this.wrapNext = false;
        this.moveTo(0, this.y);
        return;
    }
  }

  escape(char) {
    this.state = "ground";
    switch (char) {
      case "[":
        this.state = "csi";
        this.sequence = "";
        return;
      case "]":
        this.state = "osc";
        return;
      case "P":
        this.state = "dcs";
        return;
      case "(":
      case ")":
      case "*":
      case "+":
        this.state = "charset";
        return;
      case "7":
        this.save();
        return;
      case "8":
        this.restore();
        return;
      case "D":
        this.lineFeed();
        return;
      case "E":
        this.moveTo(0, this.y);
        this.lineFeed();
        return;
      case "M":
        this.reverseIndex();
        return;
      case "c":
        this.leaveAlt();
        this.reset();
        this.eraseLines(0, this.rowCount);
        return;
    }
  }

  // The cursor's line, marked to be drawn again.
  touch(y = this.y) {
    this.buffer.lines[y].dirty = true;
  }

  print(char) {
    if (this.wrapNext && this.modes.wrap) {
      this.wrapNext = false;
      this.x = 0;
      this.lineFeed();
    }
    const cells = this.buffer.lines[this.y].cells;
    if (this.modes.insert) {
      cells.splice(this.x, 0, [char, this.style]);
      cells.length = this.cols;
    } else {
      cells[this.x] = [char, this.style];
    }
    this.touch();
    if (this.x === this.cols - 1) {
      this.wrapNext = true;
    } else {
      this.x += 1;
    }
  }

  moveTo(x, y) {
    this.touch();
    this.x = Math.max(0, Math.min(this.cols - 1, x));
    this.y = Math.max(0, Math.min(this.rowCount - 1, y));
    this.wrapNext = false;
    this.touch();
  }

  lineFeed() {
    if (this.y === this.bottom) {
      this.scrollUp(1);
    } else if (this.y < this.rowCount - 1) {
      this.moveTo(this.x, this.y + 1);
    }
  }

  reverseIndex() {
    if (this.y === this.top) {
      this.scrollDown(1);
    } else if (this.y > 0) {
      this.moveTo(this.x, this.y - 1);
    }
  }

  // Scrolls the region up n lines. On the main screen, with the region the whole screen, each line
  // that leaves the top is kept, and its row goes with it.
  scrollUp(count) {
    const { lines, rows } = this.buffer;
    for (let at = 0; at < Math.min(count, this.bottom - this.top + 1); at += 1) {
      if (this.buffer === this.main && this.top === 0) {
        const gone = lines.shift();
        const row = rows.shift();
        if (gone.dirty || gone === this.cursorDrawn) {
          row.innerHTML = lineHtml(gone, -1);
        }
        this.kept.appendChild(row);
        if (this.kept.childElementCount > KEPT) {
          this.kept.firstElementChild.remove();
        }
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

  scrollDown(count) {
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
  eraseLines(from, to) {
    for (let y = Math.max(0, from); y < Math.min(this.rowCount, to); y += 1) {
      this.buffer.lines[y] = lineOf(this.cols, this.style);
    }
  }

  eraseCells(y, from, to) {
    const cells = this.buffer.lines[y].cells;
    const cell = blank(this.style);
    for (let x = Math.max(0, from); x < Math.min(this.cols, to); x += 1) {
      cells[x] = cell;
    }
    this.touch(y);
  }

  save() {
    this.saved = { x: this.x, y: this.y, style: this.style, wrapNext: this.wrapNext };
  }

  restore() {
    const saved = this.saved ?? { x: 0, y: 0, style: PLAIN, wrapNext: false };
    this.moveTo(saved.x, saved.y);
    this.style = saved.style;
    this.wrapNext = saved.wrapNext;
  }

  enterAlt() {
    if (this.buffer !== this.main) {
      return;
    }
    this.save();
    this.altHost.replaceChildren();
    this.alt = bufferOf(this.cols, this.rowCount, this.altHost);
    this.buffer = this.alt;
    this.kept.hidden = true;
    this.mainHost.hidden = true;
    this.altHost.hidden = false;
    this.top = 0;
    this.bottom = this.rowCount - 1;
  }

  leaveAlt() {
    if (this.buffer === this.main) {
      return;
    }
    this.buffer = this.main;
    this.alt = null;
    this.altHost.hidden = true;
    this.altHost.replaceChildren();
    this.kept.hidden = false;
    this.mainHost.hidden = false;
    this.top = 0;
    this.bottom = this.rowCount - 1;
    this.main.lines.forEach((line) => (line.dirty = true));
    this.restore();
  }

  csi(body, final) {
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

  eraseDisplay(how) {
    if (how === 0) {
      this.eraseCells(this.y, this.x, this.cols);
      this.eraseLines(this.y + 1, this.rowCount);
    } else if (how === 1) {
      this.eraseLines(0, this.y);
      this.eraseCells(this.y, 0, this.x + 1);
    } else if (how === 2) {
      this.eraseLines(0, this.rowCount);
    } else if (how === 3) {
      this.kept.replaceChildren();
    }
  }

  privateMode(mode, on) {
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
  sgr(numbers) {
    const style = { ...this.style };
    for (let at = 0; at < numbers.length; at += 1) {
      const value = numbers[at] ?? 0;
      if (value === 0) {
        Object.assign(style, PLAIN);
      } else if (value === 1) {
        style.bold = true;
      } else if (value === 2) {
        style.dim = true;
      } else if (value === 3) {
        style.italic = true;
      } else if (value === 4) {
        style.underline = true;
      } else if (value === 7) {
        style.inverse = true;
      } else if (value === 9) {
        style.strike = true;
      } else if (value === 22) {
        style.bold = false;
        style.dim = false;
      } else if (value === 23) {
        style.italic = false;
      } else if (value === 24) {
        style.underline = false;
      } else if (value === 27) {
        style.inverse = false;
      } else if (value === 29) {
        style.strike = false;
      } else if (value >= 30 && value <= 37) {
        style.fg = value - 30;
      } else if (value === 39) {
        style.fg = null;
      } else if (value >= 40 && value <= 47) {
        style.bg = value - 40;
      } else if (value === 49) {
        style.bg = null;
      } else if (value >= 90 && value <= 97) {
        style.fg = value - 90 + 8;
      } else if (value >= 100 && value <= 107) {
        style.bg = value - 100 + 8;
      } else if (value === 38 || value === 48) {
        const key = value === 38 ? "fg" : "bg";
        if (numbers[at + 1] === 5) {
          style[key] = numbers[at + 2] ?? 0;
          at += 2;
        } else if (numbers[at + 1] === 2) {
          // Some programs leave an empty color space before the three parts, as 38:2::r:g:b.
          const from = numbers[at + 2] === null ? at + 3 : at + 2;
          const [r, g, b] = numbers.slice(from, from + 3);
          style[key] = `rgb(${r ?? 0}, ${g ?? 0}, ${b ?? 0})`;
          at = from + 2;
        }
      }
    }
    this.style = Object.freeze(style);
  }

  // Gives the screen a new size. Lines are cut or filled out to the new width; a shorter main
  // screen keeps the lines that leave its top, and the cursor stays on the line it was on.
  // The screen the cursor is not on loses lines from its foot.
  resize(cols, rows) {
    if (cols === this.cols && rows === this.rowCount) {
      return;
    }
    for (const buffer of [this.main, this.alt].filter(Boolean)) {
      for (const line of buffer.lines) {
        while (line.cells.length < cols) {
          line.cells.push(blank(PLAIN));
        }
        line.cells.length = cols;
        line.dirty = true;
      }
      while (buffer.lines.length > rows) {
        // A line goes from the top while the cursor would otherwise be cut off the foot, and from
        // the foot after that.
        if (buffer === this.buffer && this.y >= rows) {
          const gone = buffer.lines.shift();
          const row = buffer.rows.shift();
          row.innerHTML = lineHtml(gone, -1);
          if (buffer === this.main) {
            this.kept.appendChild(row);
          } else {
            row.remove();
          }
          this.y -= 1;
        } else {
          buffer.lines.pop();
          buffer.rows.pop().remove();
        }
      }
      while (buffer.lines.length < rows) {
        buffer.lines.push(lineOf(cols));
        buffer.rows.push(buffer.host.appendChild(document.createElement("div")));
      }
    }
    this.cols = cols;
    this.rowCount = rows;
    this.top = 0;
    this.bottom = rows - 1;
    this.x = Math.min(this.x, cols - 1);
    this.y = Math.min(this.y, rows - 1);
    this.wrapNext = false;
    this.cursorDrawn = null;
    this.schedule();
  }

  setFocus(focused) {
    this.focused = focused;
    this.view.classList.toggle("term-focused", focused);
    this.touch();
    this.schedule();
  }

  schedule() {
    if (this.pending) {
      return;
    }
    this.pending = true;
    requestAnimationFrame(() => this.draw());
  }

  // Draws the lines that have changed, and the cursor. A view the reader left at its foot stays
  // there as lines are added.
  draw() {
    this.pending = false;
    const view = this.view;
    const { lines, rows } = this.buffer;
    const cursorLine = this.modes.cursor ? lines[this.y] : null;
    if (this.cursorDrawn && this.cursorDrawn !== cursorLine) {
      this.cursorDrawn.dirty = true;
    }
    if (cursorLine) {
      cursorLine.dirty = true;
    }
    lines.forEach((line, y) => {
      if (line.dirty) {
        rows[y].innerHTML = lineHtml(line, line === cursorLine ? this.x : -1);
        line.dirty = false;
      }
    });
    this.cursorDrawn = cursorLine;
    if (this.atFoot) {
      view.scrollTop = view.scrollHeight;
      this.placed = view.scrollTop;
    }
  }
}
