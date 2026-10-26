// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A screen drawn into its view: a new size, the focus, the lines drawn as they change, the view's scroll, and the lines
// kept as they scroll off, in the tail and the head and read back from the store. Each is a method of Screen
// (screen.js), `this` the screen it runs for.

import { PLAIN, blank, lineHtml, lineOf } from "./cells.js";

const KEPT = 5000;
// The most screens of lines the head holds.
const HEAD_SCREENS = 4;
// How long the view rests, in milliseconds, before the lines come into sight are read.
const SETTLES = 50;

// Gives the screen a new size. Lines are cut or filled out to the new width; a shorter main
// screen keeps the lines that leave its top, and the cursor stays on the line it was on.
// The screen the cursor is not on loses lines from its foot.
export function resize(cols, rows) {
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
          this.keepRow(row);
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
  this.rowTall = 0;
  this.trimTail();
  this.top = 0;
  this.bottom = rows - 1;
  this.x = Math.min(this.x, cols - 1);
  this.y = Math.min(this.y, rows - 1);
  this.wrapNext = false;
  this.cursorDrawn = null;
  this.schedule();
}

export function setFocus(focused) {
  this.focused = focused;
  this.view.classList.toggle("term-focused", focused);
  this.touch();
  this.schedule();
}

export function schedule() {
  if (this.pending) {
    return;
  }
  this.pending = true;
  requestAnimationFrame(() => this.draw());
}

// Draws the lines that have changed, and the cursor. A view the reader left at its foot stays
// there as lines are added.
export function draw() {
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
    this.fill();
    this.place(view.scrollHeight);
  }
}

// Scrolls the view itself, marking the scroll as its own where the view moves.
export function place(top) {
  const before = this.view.scrollTop;
  this.view.scrollTop = top;
  if (this.view.scrollTop !== before) {
    this.placed = this.view.scrollTop;
  }
}

// Keeps a row that scrolled off the top of the main screen. With a store its line goes there and
// its row to the tail, whose first row goes on into the head where the head runs on to the tail and
// the reader is not being read for, and otherwise out of the page, its height into the gap.
export function keepRow(row) {
  if (!this.store) {
    this.tail.appendChild(row);
    if (this.tail.childElementCount > KEPT) {
      this.tail.firstElementChild.remove();
    }
    return;
  }
  this.outgoing.push(row.innerHTML);
  this.written += 1;
  this.tail.appendChild(row);
  this.trimTail();
  this.flushSoon();
}

// Lets the tail's first rows go past the tail's size, into the head or out of the page.
export function trimTail() {
  const size = this.store ? 2 * this.inView() : KEPT;
  while (this.tail.childElementCount > size) {
    const first = this.tail.firstElementChild;
    const headEmpty = this.headFrom === this.headTo;
    if (!headEmpty && this.headTo === this.tailFrom && this.head.childElementCount < HEAD_SCREENS * this.inView() && !this.loading) {
      this.head.appendChild(first);
      this.headTo += 1;
    } else {
      first.remove();
      if (headEmpty && this.headTo === this.tailFrom) {
        this.headFrom += 1;
        this.headTo += 1;
      }
    }
    this.tailFrom += 1;
  }
  this.sizeGaps();
}

// Sends the lines on their way to the store, in order. A store that fails is let go, and from
// then on the page holds the last KEPT itself.
export function flushSoon() {
  this.flushing ??= Promise.resolve().then(async () => {
    try {
      while (this.outgoing.length && this.store) {
        await this.store.keep(this.outgoing.splice(0));
      }
    } catch {
      this.store = null;
      this.sizeGaps();
    }
    this.flushing = null;
  });
}

export async function readBack(from, count) {
  await this.flushing;
  return this.store && count > 0 ? this.store.read(from, count) : [];
}

// Reads what comes into sight once the view stops moving for SETTLES milliseconds. A fast scroll
// reads only where it stops and not every place it passes.
export function follow() {
  window.clearTimeout(this.settling);
  this.settling = window.setTimeout(() => this.fill(), SETTLES);
}

export function rowsOf(lines) {
  return lines.map((html) => Object.assign(document.createElement("div"), { innerHTML: html }));
}

// The height of one row, read from the screen's own rows.
export function rowHeight() {
  this.rowTall ||= this.mainHost.firstElementChild?.getBoundingClientRect().height || 0;
  return this.rowTall;
}

// How many rows the view shows at once, and the screen's own rows at the least.
export function inView() {
  const tall = this.rowHeight();
  return Math.max(this.rowCount, tall ? Math.ceil(this.view.clientHeight / tall) : 0);
}

// Gives each line of the store not held its row's height, above the head and between the head
// and the tail. The view then scrolls over every line kept, and the screen stays under the last
// of it.
export function sizeGaps() {
  const tall = this.store ? this.rowHeight() : 0;
  this.above.style.height = `${(this.headFrom - this.floor) * tall}px`;
  this.between.style.height = `${(this.tailFrom - this.headTo) * tall}px`;
}

// Holds the lines in sight in the head, and a screen of them either side, read from the store,
// where they are not in the tail. Where what is wanted meets the head, the head reaches on to it
// within HEAD_SCREENS screens around those in sight; where it does not, it is read whole in its
// place. At the foot, or with every line in sight in the tail, the head holds nothing.
export async function fill() {
  if (!this.store || this.loading || this.head.hidden) {
    return;
  }
  const tall = this.rowHeight();
  if (!tall) {
    return;
  }
  const view = this.view;
  const screenful = this.inView();
  const clamp = (line) => Math.max(this.floor, Math.min(this.tailFrom, line));
  const top = this.above.offsetTop;
  const first = this.floor + Math.floor((view.scrollTop - top) / tall);
  const last = this.floor + Math.ceil((view.scrollTop + view.clientHeight - top) / tall);
  if (this.atFoot || first >= this.tailFrom) {
    if (this.headFrom !== this.headTo || this.headFrom !== this.tailFrom) {
      this.head.replaceChildren();
      this.headFrom = this.tailFrom;
      this.headTo = this.tailFrom;
      this.sizeGaps();
      if (this.atFoot) {
        this.place(view.scrollHeight);
      }
    }
    return;
  }
  const margin = Math.floor(screenful / 2);
  const held = this.headFrom <= Math.max(this.floor, first - margin) && this.headTo >= Math.min(this.tailFrom, last + margin);
  if (held && this.head.childElementCount <= HEAD_SCREENS * screenful) {
    return;
  }
  let from = clamp(first - screenful);
  let to = clamp(last + screenful);
  if (from <= this.headTo && to >= this.headFrom && this.headFrom !== this.headTo) {
    from = Math.min(from, this.headFrom);
    to = Math.max(to, this.headTo);
    const most = HEAD_SCREENS * screenful;
    if (to - from > most) {
      from = clamp(Math.round((first + last) / 2) - Math.floor(most / 2));
      to = clamp(from + most);
      from = clamp(to - most);
    }
  }
  if (from === this.headFrom && to === this.headTo) {
    return;
  }
  this.loading = true;
  try {
    await this.hold(from, to);
  } finally {
    this.loading = false;
  }
  // The view may have moved on while the lines were read.
  requestAnimationFrame(() => this.fill());
}

// Makes the head the lines of the store from `from` up to `to`, reading only those not held.
export async function hold(from, to) {
  if (from >= this.headTo || to <= this.headFrom) {
    const lines = await this.readBack(from, to - from);
    this.head.replaceChildren(...this.rowsOf(lines));
    this.headFrom = from;
    this.headTo = from + lines.length;
  } else {
    const above = await this.readBack(from, this.headFrom - from);
    const below = await this.readBack(this.headTo, to - this.headTo);
    while (this.headFrom < from && this.head.firstElementChild) {
      this.head.firstElementChild.remove();
      this.headFrom += 1;
    }
    while (this.headTo > to && this.head.lastElementChild) {
      this.head.lastElementChild.remove();
      this.headTo -= 1;
    }
    this.head.prepend(...this.rowsOf(above));
    this.headFrom -= above.length;
    this.head.append(...this.rowsOf(below));
    this.headTo += below.length;
  }
  this.sizeGaps();
}
