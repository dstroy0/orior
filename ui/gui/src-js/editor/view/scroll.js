// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { most } from "../document.js";
import { pressed, status, write } from "../../status.js";
import { enter, leave } from "../../trace.js";
import { coloringReach, FLICK_KEY, FLICK_LEAST, FLICK_REST, FLICK_SLOWS, FLICK_STOP, GLIDE_PIECE, GLIDE_RATE, LINE, REST, SMOOTH_KEY } from "../view.js";

// Drawing.
//
// Each frame draws every row on screen in its one form whatever the speed: its colors, its
// brackets, guides and marks, and its gutter, the same at speed as at rest, so that a scroll
// animates the whole way and no row is drawn again as the view slows. Once the view rests, the
// lines ahead of where it was heading are colored a slice at a time while the page is idle.

// A wheel over the text or the gutter. A wheel's step glides where smooth scrolling is on; a
// touchpad's movement, in pixels and not in steps, follows the fingers and is tracked for the
// run-on. A wheel across, and one with Ctrl, which zooms, are left to the page.
export function onWheel(event) {
  if (event.ctrlKey || Math.abs(event.deltaX) > Math.abs(event.deltaY)) {
    return;
  }
  event.preventDefault();
  const unit = event.deltaMode === 1 ? LINE : event.deltaMode === 2 ? this.scroller.clientHeight : 1;
  const by = event.deltaY * unit;
  const stepped = event.deltaMode !== 0 || (event.wheelDeltaY !== 0 && event.wheelDeltaY % 120 === 0);
  this.flick = null;
  if (stepped && this.smoothOn) {
    this.glideTo((this.glide?.to ?? this.scroller.scrollTop) + by);
    return;
  }
  this.glide = null;
  this.scroller.scrollTop += by;
  if (!stepped && this.flickOn) {
    this.track(by);
  }
}

// Glides the scrolling space to `to`: from where it stands, or where a glide under way heads
// there instead, keeping its speed.
export function glideTo(to) {
  const most = this.scroller.scrollHeight - this.scroller.clientHeight;
  const target = Math.max(0, Math.min(most, to));
  if (this.glide) {
    this.glide.to = target;
    return;
  }
  const glide = { to: target, at: this.scroller.scrollTop, speed: 0, last: null };
  this.glide = glide;
  // Each glide moves the space by its own frames alone, and stops once another takes its place.
  const step = (now) => {
    if (this.glide !== glide) {
      return;
    }
    const call = enter("glide");
    try {
      this.glided(glide, now, step);
    } finally {
      leave(call);
    }
  };
  requestAnimationFrame(step);
}

// One frame of a glide: the spring moved on by the time since the frame before, and the space
// scrolled to where it stands, the glide ended once it stands at its end.
export function glided(glide, now, step) {
  let left = glide.last === null ? GLIDE_PIECE * 2 : Math.min(64, now - glide.last);
  glide.last = now;
  while (left > 0) {
    const piece = Math.min(GLIDE_PIECE, left) / 1000;
    left -= piece * 1000;
    glide.speed += (GLIDE_RATE * GLIDE_RATE * (glide.to - glide.at) - 2 * GLIDE_RATE * glide.speed) * piece;
    glide.at += glide.speed * piece;
  }
  if (Math.abs(glide.to - glide.at) < 0.5 && Math.abs(glide.speed) < 20) {
    this.scroller.scrollTop = glide.to;
    this.glide = null;
    return;
  }
  this.scroller.scrollTop = glide.at;
  requestAnimationFrame(step);
}

// The touchpad's speed, the latest movement weighed most, and the run-on once it rests.
export function track(by) {
  const now = performance.now();
  const spent = Math.max(1, now - (this.flickAt ?? now - 16));
  this.flickSpeed = spent > 100 ? by / spent : 0.8 * (by / spent) + 0.2 * (this.flickSpeed ?? 0);
  this.flickAt = now;
  window.clearTimeout(this.flickWait);
  this.flickWait = window.setTimeout(() => this.runOn(), FLICK_REST);
}

export function runOn() {
  let speed = this.flickSpeed ?? 0;
  if (Math.abs(speed) < FLICK_LEAST) {
    return;
  }
  let last = performance.now();
  const step = (now) => {
    if (this.flick !== step) {
      return;
    }
    const spent = Math.max(0, now - last);
    last = now;
    const was = this.scroller.scrollTop;
    this.scroller.scrollTop += speed * spent;
    speed *= Math.exp(-spent / FLICK_SLOWS);
    if (Math.abs(speed) < FLICK_STOP || (spent > 0 && this.scroller.scrollTop === was)) {
      this.flick = null;
      return;
    }
    requestAnimationFrame(step);
  };
  this.flick = step;
  requestAnimationFrame(step);
}

// Whether lines wrap.
export function wrapped() {
  return this.wrapOn;
}

export function setSmooth(on) {
  this.smoothOn = on;
  localStorage.setItem(SMOOTH_KEY, String(on));
}

export function setFlick(on) {
  this.flickOn = on;
  localStorage.setItem(FLICK_KEY, String(on));
}

// Where the view is heading: the end of the glide under way, or where it stands.
export function headingY() {
  return this.glide ? this.glide.to - this.pad : this.scrollY();
}

// Scrolls the view to `top`, the end of a glide under way where one is, and at once where not.
export function scrollToY(top) {
  if (this.glide) {
    this.glideTo(top + this.pad);
  } else {
    this.place(top);
  }
}

// Scrolls the view to `top` itself, for a jump to a line or a page, or to hold the text in place
// as lines are read in above it. The scroll that follows is not counted as speed, and a glide or a
// run-on under way ends.
export function place(top) {
  this.glide = null;
  this.flick = null;
  const was = this.scroller.scrollTop;
  this.scroller.scrollTop = top + this.pad;
  this.placedTop = this.scroller.scrollTop === was ? null : this.scroller.scrollTop;
  // where the view stands from now, before the scroll the placing makes is heard: a paged file's window moved under the
  // view changes the padding above it, and the scroll last heard stands for another line until then
  this.lastTop = this.scroller.scrollTop;
}

export function onScroll() {
  // Where the view stands, read before anything changes the page.
  const top = this.scroller.scrollTop;
  const left = this.scroller.scrollLeft;
  this.scrolledNow = { top, left };
  this.hover.hide();
  const now = performance.now();
  if (top === this.placedTop) {
    // The view's own jump: however far it went, nothing moved fast.
    this.placedTop = null;
    this.lastTop = top;
    this.lastScroll = now;
    this.follow(top, left);
    this.rest();
    this.schedule();
    return;
  }
  this.placedTop = null;
  const moved = top - (this.lastTop ?? top);
  const spent = Math.max(1, now - (this.lastScroll ?? now - 16));
  this.lastTop = top;
  this.lastScroll = now;
  // Smoothed: one long jump between two events does not throw the speed up and back.
  const speed = 0.6 * status.scroll.speed + 0.4 * (Math.abs(moved) / spent);
  // The speed the view had as it came to rest, which the idle coloring reaches ahead by.
  this.lastSpeed = speed;
  write("scroll", { speed, heading: moved ? Math.sign(moved) : status.scroll.heading });
  // The sheet and the gutter stand outside the scrolling space. They follow now and not a frame late, except into the
  // lines a paged file's window has not read: there they stand as last drawn until the window comes to the view.
  if (!(this.heldFrame === this.s && this.pastWindow(top))) {
    this.follow(top, left);
  }
  this.rest();
  this.schedule();
}

// Once the view has rested REST, its speed falls to nothing, the work held back while it moved
// goes on, and the lines ahead of where it was heading are colored while the page is idle.
export function rest() {
  window.clearTimeout(this.restWait);
  this.restWait = window.setTimeout(() => {
    write("scroll", { speed: 0 });
    this.warm();
  }, REST);
}

// Colors the lines ahead of the view in the way it was heading, while the page is idle, as far as
// coloringReach says for the speed the view had as it came to rest. Scrolling on finds them done.
export function warm() {
  const s = this.s;
  if (!s?.highlight.grammar) {
    return;
  }
  const rows = this.rows();
  const shown = Math.ceil(this.scroller.clientHeight / LINE);
  const first = Math.floor(this.scrollY() / LINE);
  const heading = status.scroll.heading;
  const from = heading > 0 ? first + shown : Math.max(0, first - 1);
  let row = from;
  const reach = coloringReach(shown, this.lastSpeed ?? 0, this.screenColorMs ?? 0);
  const end = heading > 0 ? Math.min(rows.size, from + reach) : Math.max(0, from - reach);
  const idle = (run) => (window.requestIdleCallback ? window.requestIdleCallback(run) : window.setTimeout(() => run({ timeRemaining: () => 8 }), 16));
  const slice = (deadline) => {
    if (this.s !== s || pressed()) {
      return;
    }
    const began = performance.now();
    const was = row;
    while (deadline.timeRemaining() > 2 && (heading > 0 ? row < end : row >= end)) {
      s.highlight.runsOf(rows.lineOf(row));
      row += heading;
    }
    // How long a screen of lines takes to color, kept smoothed over the slices.
    const colored = Math.abs(row - was);
    if (colored > 0) {
      const screen = ((performance.now() - began) / colored) * shown;
      this.screenColorMs = this.screenColorMs === undefined ? screen : 0.7 * this.screenColorMs + 0.3 * screen;
    }
    write("colors", { ahead: Math.abs(row - from) });
    if (heading > 0 ? row < end : row >= end) {
      idle(slice);
    }
  };
  idle(slice);
}
