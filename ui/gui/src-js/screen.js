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
// The main screen retains what scrolls off its top, and the second screen retains nothing. A line is one
// row of the page, drawn again only when something in it has changed, and a line that scrolls off
// the top of the main screen takes its row with it into what is retained.
//
// The lines retained are retained as a large file is. Each line that scrolls off goes to a store on disk, and
// the page holds two runs of them: the tail, the two screens of lines just above the screen, which
// grows at the speed of the output and lets its oldest go; and the head, the lines in sight and a
// screen either side where the reader has scrolled back past the tail, read from the store as they
// come into sight. Every line retained in neither is as its height in a gap, above the head or
// between the head and the tail. A reader sitting still retains their place while the output runs
// on below, and the screen always sits under the last line retained however fast the view is scrolled.
// With no store, the page holds the last RETAINED in the tail itself.

import { bufferOf, PLAIN } from "./screen/cells.js";
import * as region from "./screen/region.js";
import * as view from "./screen/view.js";

export { coloredHtml, coloredLines } from "./screen/cells.js";

const TAB = 8;

// The longest operating-system command the screen retains to read.
const OSC_LONGEST = 4096;

export class Screen {
  // `view` is the scrolling element the screen is drawn in, `answer` what the screen says back to
  // the shell, as when it is asked where its cursor is, and `store` where the lines that scroll off
  // go: { retain(lines), read(from, count) }, each answering with a promise.
  constructor(view, cols, rows, answer, store = null) {
    this.view = view;
    this.answer = answer;
    this.store = store;
    // The lines sent to the store; the lines of it the head holds, from its first to the one after
    // its last; the first line the tail holds, which runs on to the last sent; the first line still
    // to be read back after an erase; and the lines on their way.
    this.written = 0;
    this.headFrom = 0;
    this.headTo = 0;
    this.tailFrom = 0;
    this.floor = 0;
    this.outgoing = [];
    this.flushing = null;
    this.loading = false;
    this.atFoot = true;
    // Where the screen last scrolled the view to itself, until the scroll it made is seen, which says
    // nothing of where the reader is; -1 where no scroll of its own is on its way.
    this.placed = -1;
    view.addEventListener("scroll", () => {
      if (view.scrollTop === this.placed) {
        this.placed = -1;
        return;
      }
      this.placed = -1;
      this.atFoot = view.scrollHeight - view.scrollTop - view.clientHeight < 4;
      this.follow();
    });
    this.cols = cols;
    this.rowCount = rows;
    this.above = view.appendChild(Object.assign(document.createElement("div"), { className: "term-gap" }));
    this.head = view.appendChild(Object.assign(document.createElement("div"), { className: "term-retained" }));
    this.between = view.appendChild(Object.assign(document.createElement("div"), { className: "term-gap" }));
    this.tail = view.appendChild(Object.assign(document.createElement("div"), { className: "term-retained" }));
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

  // An operating-system command the shell sent: OSC 7, the folder it is in as a file URL, goes
  // to `onPlace`; any other is read past.
  osc(text) {
    const cut = text.indexOf(";");
    if (cut > 0 && text.slice(0, cut) === "7") {
      this.onPlace?.(text.slice(cut + 1));
    }
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
          this.osc(this.sequence);
        } else if (code === 0x1b) {
          this.state = "string-end";
          this.ending = "osc";
        } else if (this.sequence.length < OSC_LONGEST) {
          this.sequence += char;
        }
        return;
      case "dcs":
        if (code === 0x1b) {
          this.state = "string-end";
          this.ending = "dcs";
        }
        return;
      case "string-end":
        this.state = char === "\\" ? "ground" : "escape";
        if (this.state === "escape") {
          this.escape(char);
        } else if (this.ending === "osc") {
          this.osc(this.sequence);
        }
        this.ending = null;
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
        this.sequence = "";
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

}

// The escape sequences' work and the drawing into the view, retained in screen/ and put on the class as its own methods
// are: not counted among an object's keys.
for (const part of [region, view]) {
  for (const [name, method] of Object.entries(part)) {
    Object.defineProperty(Screen.prototype, name, { value: method, writable: true, configurable: true });
  }
}
