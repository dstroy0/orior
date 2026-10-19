// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A list of rows of one height, as many as a tree of any size holds, of which only the rows in sight
// and those the scroll heads into are on the page. Each row is built from what it stands for as it
// comes into sight and taken off once the scroll has gone past it; the list keeps the height every
// row would take, and its scroll stands where it would. The rows ahead are as many as the scroll's
// speed carries it over two frames' drawing, never fewer than LEAST and never more than two screens,
// marked `ahead`. A row that holds
// the keys stays on the page wherever the scroll goes. A row's place is its number times its height,
// and a list can set one gap above a row of its own, which every row past it stands below.
//
// lists.js steps the keys through every row by the list's model: how many rows there are, each one's
// key, depth, name and whether it is open, and a row's element, put on the page and scrolled to.

import { Ahead } from "./ahead.js";
import { frameMs } from "./motion.js";

const LEAST = 8;

export class RowList {
  // `build(row)` makes a row's element from what it stands for; `height` is each row's, and `gap`
  // the room above the row a list's rows name with `gapAt`.
  constructor(list, { height, gap = 0, build }) {
    this.list = list;
    this.height = height;
    this.gap = gap;
    this.build = build;
    this.rows = [];
    this.at = new Map();
    this.gapAt = -1;
    this.shown = new Map();
    this.ahead = new Ahead(LEAST);
    this.frame = 0;
    // Told each time rows come onto the page, as a search typed in the list marks its matches.
    this.onDraw = null;
    this.size = document.createElement("div");
    this.size.className = "row-list-size";
    list.classList.add("row-list");
    list.replaceChildren(this.size);
    list.rowModel = this;
    list.addEventListener("scroll", () => this.later(), { passive: true });
    new ResizeObserver(() => this.later()).observe(list);
  }

  // The rows, each { key, depth, name, open } and whatever `build` reads, and the row the gap stands
  // above, or -1. The rows on the page are built again: what each shows may have changed. The row that
  // holds the keys hands them to its new element before its old one goes: an element taken off while it
  // holds the keys has the page let them go, and that costs a frame.
  set(rows, gapAt = -1) {
    const holding = this.list.contains(document.activeElement) ? document.activeElement : null;
    const focused = holding?.dataset.key ?? null;
    const old = [...this.shown.values()];
    this.rows = rows;
    this.gapAt = gapAt;
    this.at = new Map(rows.map((row, at) => [row.key, at]));
    this.shown.clear();
    old.forEach((element) => element !== holding && element.remove());
    this.size.style.height = `${this.topOf(rows.length)}px`;
    this.draw();
    if (focused !== null && this.at.has(focused)) {
      this.element(this.at.get(focused)).focus({ preventScroll: true });
    }
    if (holding && old.includes(holding)) {
      holding.remove();
    }
  }

  get count() {
    return this.rows.length;
  }

  topOf(at) {
    return at * this.height + (this.gapAt >= 0 && at >= this.gapAt ? this.gap : 0);
  }

  indexOf(key) {
    return this.at.get(key) ?? -1;
  }

  key(at) {
    return this.rows[at].key;
  }

  depth(at) {
    return this.rows[at].depth;
  }

  name(at) {
    return this.rows[at].name;
  }

  open(at) {
    return this.rows[at].open;
  }

  // The element of row `at` where it is on the page, and null where not.
  shownAt(at) {
    return this.shown.get(this.rows[at]?.key) ?? null;
  }

  // The element of row `at`, put on the page; scrolled into sight where `reveal`.
  element(at, { reveal = false } = {}) {
    const row = this.rows[at];
    if (!row) {
      return null;
    }
    if (reveal) {
      const top = this.topOf(at);
      if (top < this.list.scrollTop) {
        this.list.scrollTop = top;
      } else if (top + this.height > this.list.scrollTop + this.list.clientHeight) {
        this.list.scrollTop = top + this.height - this.list.clientHeight;
      }
      this.draw();
    }
    return this.shown.get(row.key) ?? this.place(at);
  }

  // The element of the row of `key`, put on the page and scrolled into sight, or null where no row has
  // that key.
  reveal(key) {
    const at = this.indexOf(key);
    return at < 0 ? null : this.element(at, { reveal: true });
  }

  place(at) {
    const row = this.rows[at];
    const element = this.build(row);
    element.dataset.key = row.key;
    element.style.top = `${this.topOf(at)}px`;
    // In the order of the rows, as the list's keys step through the page's.
    let before = null;
    for (const [key, one] of this.shown) {
      const there = this.at.get(key);
      if (there > at && (!before || there < this.at.get(before.dataset.key))) {
        before = one;
      }
    }
    this.shown.set(row.key, element);
    this.list.insertBefore(element, before);
    return element;
  }

  later() {
    this.frame ||= requestAnimationFrame(() => {
      this.frame = 0;
      this.draw();
    });
  }

  // The rows in sight and those ahead put on the page, and those the scroll has left taken off, all
  // but the row that holds the keys.
  draw() {
    const began = performance.now();
    const top = Math.max(0, this.list.scrollTop - (this.gapAt >= 0 && this.list.scrollTop > this.topOf(this.gapAt) ? this.gap : 0));
    const first = Math.floor(top / this.height);
    const last = Math.ceil((top + this.list.clientHeight) / this.height);
    this.ahead.moved(first);
    // A jump reads as a great speed: ahead is never more than two screens of rows past the edge.
    const most = 2 * (last - first);
    const [reached, to] = this.ahead.span(first, last);
    const from = Math.max(0, reached, first - most);
    const end = Math.min(this.rows.length, to, last + most);
    const focused = this.list.contains(document.activeElement) ? document.activeElement : null;
    for (const [key, element] of this.shown) {
      const at = this.at.get(key);
      if ((at === undefined || at < from || at >= end) && element !== focused) {
        element.remove();
        this.shown.delete(key);
      }
    }
    for (let at = from; at < end; at += 1) {
      const element = this.shown.get(this.rows[at].key) ?? this.place(at);
      element.classList.toggle("ahead", this.ahead.heading >= 0 ? at >= last : at < first);
    }
    this.ahead.read(Math.max(frameMs(), performance.now() - began));
    this.onDraw?.();
  }
}
