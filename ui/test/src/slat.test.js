// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The strip and the bar brought in as slats: a rest at a point of an edge grows the part from that point, the work
// giving way round it.

import { slatOf } from "../../gui/src-js/slat.js";
import { revealed } from "../../gui/src-js/claim.js";
import { editingHello, GAP, mark, markAt, markSaid, middle, SLIDE } from "../claims.js";

const { test, assert, ui } = window.__harness;

test("a rest at a point of the left edge brings the strip in as a slat growing from that point, the work giving way round it, and once it spans its edge the work takes its room", async () => {
  await editingHello();
  const work = document.querySelector(".work");
  const strip = document.getElementById("strip");
  const before = work.getBoundingClientRect().left;
  const at = Math.round(window.innerHeight * 0.6);
  await ui.hover({ x: 4, y: at });
  await ui.waitFor(() => revealed() === "left", 3000, "the strip revealed");
  await ui.rest(120);
  const slat = slatOf(strip);
  const whole = strip.getBoundingClientRect();
  assert.ok(slat && slat.top < at && slat.bottom > at, `the slat about the pointer at ${at}: ${slat?.top} to ${slat?.bottom}`);
  assert.ok(slat.bottom - slat.top < whole.height - 40, `the slat shorter than the strip: ${slat.bottom - slat.top} of ${whole.height}`);
  assert.ok(Math.abs(work.getBoundingClientRect().left - before) < 0.5, `the work where it was while the slat grows: ${work.getBoundingClientRect().left}`);
  const inWork = (x, y) => work.contains(document.elementFromPoint(x, y));
  assert.ok(!inWork(slat.right + GAP / 2, at), "beside the slat the work has given way");
  assert.ok(inWork(before + 4, whole.top + 8), "away from the slat the work is at its edge");
  await ui.rest(SLIDE);
  assert.ok(Math.abs(work.getBoundingClientRect().left - (44 + 2 * GAP)) < 1, `the work's left past the strip once it spans: ${work.getBoundingClientRect().left}`);
  assert.equal(strip.style.clipPath, "", "the strip whole");
  assert.ok(Math.abs(slatOf(strip).top - work.getBoundingClientRect().top) < 1, `the strip's slat spans to the work's top, the wordmark's corner with it: ${slatOf(strip).top}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest at a point of the head brings the bar in as a slat growing from that point, the wordmark whole in its corner", async () => {
  await editingHello();
  const bar = document.querySelector("header.bar");
  const work = document.querySelector(".work");
  const top = work.getBoundingClientRect().top;
  const at = Math.round(window.innerWidth * 0.4);
  await ui.hover({ x: at, y: 4 });
  await ui.waitFor(() => revealed() === "top", 3000, "the bar revealed");
  await ui.rest(120);
  const slat = slatOf(bar);
  const whole = bar.getBoundingClientRect();
  assert.ok(slat && slat.left < at && slat.right > at, `the slat about the pointer at ${at}: ${slat?.left} to ${slat?.right}`);
  assert.ok(slat.right - slat.left < whole.width - 40, `the slat shorter than the bar: ${slat.right - slat.left} of ${whole.width}`);
  assert.ok(Math.abs(work.getBoundingClientRect().top - top) < 0.5, "the work where it was while the slat grows");
  assert.ok(mark().getBoundingClientRect().width < 4, `the wordmark not grown in its corner before the work gives it room: ${markSaid()}`);
  await ui.rest(SLIDE);
  assert.equal(bar.style.clipPath, "", "the bar whole once it spans");
  assert.ok(work.getBoundingClientRect().top > whole.bottom, "the work's top below the bar once it spans");
  assert.ok(Math.abs(slatOf(bar).left - work.getBoundingClientRect().left) < 1, `the head's slat spans to the work's left edge, the wordmark's corner with it: ${slatOf(bar).left}`);
  assert.ok(markAt(true, GAP), `the wordmark grown whole in its corner with the work's room: ${markSaid()}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});
