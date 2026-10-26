// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A part comes onto the work as a slat pulled from a fence: a rest at a point of its edge brings it in at
// that point, and as it slides in it grows along its edge away from the point until it spans the edge.
// Each frame of the slide the part is cut to the slat it has grown to and the work gives way round the
// slat (bend.js); once the slat spans its edge the work takes the part's room (style.css). A part going
// away goes as long as it stands. The bar keeps the wordmark and the controls whole, which stand in it.

import { beforeBend, showsOnWork } from "./bend.js";

const byId = (id) => document.getElementById(id);

// The point along each edge its slat is pulled from, null where none has been and the slat grows from
// the edge's middle.
const pulled = { top: null, left: null, bottom: null };

// Where the pointer rested as `edge` is revealed.
export function slatFrom(edge, x, y) {
  if (edge in pulled) {
    pulled[edge] = edge === "left" ? y : x;
  }
}

// The parts that come in as slats: the edge each comes from, the number its slide runs on, the boxes that
// stand in it and stay whole, and the share it has grown to.
const slats = [
  { edge: "top", part: () => document.querySelector("header.bar"), number: "--reveal-top", kept: () => [byId("bar-mark"), byId("window-controls")] },
  { edge: "top", part: () => byId("toolbar"), number: "--reveal-top", kept: () => [] },
  { edge: "left", part: () => byId("strip"), number: "--reveal-left", kept: () => [] },
  { edge: "bottom", part: () => byId("statusbar"), number: "--reveal-bottom", kept: () => [] },
].map((slat) => ({ ...slat, grown: 1, last: 0, shown: null }));

const read = (style, name) => Number.parseFloat(style.getPropertyValue(name)) || 0;

// The box `part`'s slat shows as its last frame left it, or null where it is no slat.
export const slatOf = (part) => slats.find((slat) => slat.part() === part)?.shown ?? null;

// A rectangle as a path, clockwise, rounded `round` where it is wide and tall enough.
function rectangle({ left, top, right, bottom }, round = 0) {
  const r = Math.min(round, (right - left) / 2, (bottom - top) / 2);
  if (!r) {
    return `M${left} ${top}H${right}V${bottom}H${left}Z`;
  }
  return `M${left + r} ${top}H${right - r}A${r} ${r} 0 0 1 ${right} ${top + r}V${bottom - r}A${r} ${r} 0 0 1 ${right - r} ${bottom}H${left + r}A${r} ${r} 0 0 1 ${left} ${bottom - r}V${top + r}A${r} ${r} 0 0 1 ${left + r} ${top}Z`;
}

// The slat cut as the part stands now, and the box it shows kept for the work to give way round.
function cut(slat) {
  const part = slat.part();
  if (!part || part.hidden) {
    slat.shown = null;
    return;
  }
  const box = part.getBoundingClientRect();
  const style = getComputedStyle(part);
  const claim = read(style, "--claim");
  const reveal = read(style, slat.number);
  // Coming in, the slat grows with the slide; going, it stands as long as it grew; outside the claim it
  // is whole.
  if (claim < 1) {
    slat.grown = 1;
  } else if (reveal > slat.last) {
    slat.grown = reveal;
  } else if (!reveal) {
    slat.grown = 0;
  }
  slat.last = reveal;
  const across = slat.edge === "left";
  // The head's slat and the strip's span from the work's own edge, the wordmark's corner theirs, so the
  // work gives way at that corner too and keeps no tail of itself there.
  const work = document.querySelector(".work").getBoundingClientRect();
  const [first, end] = across ? [Math.min(box.top, work.top), box.bottom] : [slat.edge === "top" ? Math.min(box.left, work.left) : box.left, box.right];
  const at = Math.min(end, Math.max(first, pulled[slat.edge] ?? (first + end) / 2));
  const from = at - (at - first) * slat.grown;
  const to = at + (end - at) * slat.grown;
  slat.shown = across ? { left: box.left, top: from, right: box.right, bottom: to } : { left: from, top: box.top, right: to, bottom: box.bottom };
  if (slat.grown >= 1) {
    part.style.clipPath = "";
    return;
  }
  const round = 7 * claim;
  const own = (one) => ({ left: one.left - box.left, top: one.top - box.top, right: one.right - box.left, bottom: one.bottom - box.top });
  const kept = slat
    .kept()
    .filter(Boolean)
    .map((one) => {
      const seen = one.getBoundingClientRect();
      const lip = Math.min(0, Number.parseFloat(getComputedStyle(one, "::before").left) || 0);
      return own({ left: seen.left + lip, top: seen.top, right: seen.right, bottom: seen.bottom });
    })
    .filter((one) => one.right > one.left && one.bottom > one.top);
  const window = own(slat.shown);
  part.style.clipPath = `path("${[rectangle(window, round), ...kept.map((one) => rectangle(one))].join("")}")`;
}

// The slats cut on each frame the work's outline is worked out, each slat's box shown on the work.
export function startSlats() {
  beforeBend(() => slats.forEach(cut));
  for (const slat of slats) {
    showsOnWork(() => slat.shown, slat.edge === "bottom" ? "foot" : slat.edge);
  }
}
