// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a view's motion heads into. A view tells its tracker where it stands as it scrolls, in rows,
// and how long each of its reads takes; the tracker keeps the way the view heads and how fast, and
// gives the stretch to read and hold: the rows in view, as many more the way it heads as its speed
// carries it over a read, twice over, and a few the other way. What lies behind the motion is let go
// first, and what lies ahead is held longest.
//
// Motion says where the view goes next: the view a moment later lies the way it has been heading.

// How long a view stands still before its speed counts as none, in milliseconds.
const RESTS = 400;

// The share of a new measure that goes into the speed and the read time kept.
const NEW = 0.4;

export class Ahead {
  constructor(least = 10) {
    // The fewest rows held each way.
    this.least = least;
    this.at = 0;
    this.when = 0;
    this.speed = 0;
    this.heading = 1;
    this.readMs = 50;
  }

  // Takes the view's first row in view, `at`, at the time `now`.
  moved(at, now = performance.now()) {
    const gap = now - this.when;
    if (this.when && gap > 0 && gap < RESTS && at !== this.at) {
      this.speed = this.speed * (1 - NEW) + (Math.abs(at - this.at) / gap) * NEW;
    } else if (!this.when || gap >= RESTS) {
      this.speed = 0;
    }
    if (at !== this.at) {
      this.heading = Math.sign(at - this.at);
    }
    this.at = at;
    this.when = now;
  }

  // Takes the time a read took, in milliseconds.
  read(ms) {
    this.readMs = this.readMs * (1 - NEW) + ms * NEW;
  }

  // The speed in rows a millisecond, none once the view has stood still for a while.
  speedAt(now = performance.now()) {
    return now - this.when >= RESTS ? 0 : this.speed;
  }

  // How many rows ahead of the view a read reaches: as far as the speed carries the view over two
  // reads, and never fewer than the least.
  reach(now = performance.now()) {
    return Math.max(this.least, Math.ceil(this.speedAt(now) * this.readMs * 2));
  }

  // The rows to read and hold, [from, to), for a view showing `first` to `last`: the reach the way
  // the view heads, and the least the other way.
  span(first, last, now = performance.now()) {
    const reach = this.reach(now);
    return this.heading >= 0 ? [Math.max(0, first - this.least), last + reach] : [Math.max(0, first - reach), last + this.least];
  }

  // Of the rows `held`, those to let go to keep at most `keep` of them, for a view showing `first`
  // to `last`: those outside the span, the ones behind the motion first, each side farthest first.
  letGo(held, first, last, keep, now = performance.now()) {
    const [from, to] = this.span(first, last, now);
    const outside = held.filter((row) => row < from || row >= to);
    const behind = (row) => (this.heading >= 0 ? row < from : row >= to);
    const away = (row) => (row < from ? from - row : row - to);
    outside.sort((a, b) => Number(behind(b)) - Number(behind(a)) || away(b) - away(a));
    return outside.slice(0, Math.max(0, held.length - keep));
  }
}
