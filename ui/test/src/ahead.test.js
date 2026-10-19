// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tracker of what a view's motion heads into, driven by rows and times given to it.

import { Ahead } from "../../gui/src-js/ahead.js";

const { test, assert } = window.__harness;

test("a view at rest holds the least each way", () => {
  const ahead = new Ahead(10);
  ahead.moved(100, 1000);
  assert.equal(ahead.speedAt(5000), 0);
  assert.deepEqual(ahead.span(100, 130, 5000), [90, 140]);
});

test("a view moving down reads as far ahead as its speed carries it over two reads", () => {
  const ahead = new Ahead(10);
  ahead.read(50);
  ahead.moved(0, 1000);
  for (let at = 1; at <= 10; at += 1) {
    ahead.moved(at * 40, 1000 + at * 20);
  }
  const speed = ahead.speedAt(1200);
  assert.ok(speed > 1 && speed <= 2, `two rows a millisecond at most: ${speed}`);
  const [from, to] = ahead.span(400, 430, 1200);
  assert.equal(from, 390, "the least behind");
  assert.equal(to, 430 + Math.ceil(speed * ahead.readMs * 2), "the reach ahead");
  assert.ok(to - 430 > 10, "farther than the least");
});

test("a view moving up reads ahead above it", () => {
  const ahead = new Ahead(10);
  ahead.moved(1000, 1000);
  ahead.moved(900, 1020);
  ahead.moved(800, 1040);
  assert.equal(ahead.heading, -1);
  const [from, to] = ahead.span(800, 830, 1040);
  assert.equal(to, 840);
  assert.ok(800 - from > 10, `the reach above: ${800 - from}`);
});

test("a slower read reaches farther at the same speed", () => {
  const quick = new Ahead(5);
  const slow = new Ahead(5);
  for (const ahead of [quick, slow]) {
    ahead.moved(0, 1000);
    ahead.moved(50, 1025);
  }
  quick.read(10);
  slow.read(200);
  assert.ok(slow.reach(1025) > quick.reach(1025));
});

test("what lies behind the motion is let go first, the farthest first", () => {
  const ahead = new Ahead(10);
  ahead.moved(0, 1000);
  ahead.moved(500, 1010);
  // The view shows rows 500 to 530 heading down; rows held behind it and far ahead of its reach.
  const held = [100, 200, 300, 495, 510, 5000, 6000];
  const gone = ahead.letGo(held, 500, 530, 4, 1010);
  assert.deepEqual(gone, [100, 200, 300], "the three behind go, the farthest first, before any ahead");
  assert.deepEqual(ahead.letGo(held, 500, 530, 2, 1010), [100, 200, 300, 6000, 5000], "then the farthest ahead");
  assert.deepEqual(ahead.letGo(held, 500, 530, 10, 1010), [], "none goes while all fit");
});
