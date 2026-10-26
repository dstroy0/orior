// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's opening and its lids: where the eye stands in its canvas, the edge of the opening at an openness, each
// lid's band, and what is drawn under the irises and over them.

import { cssOf } from "../colors.js";
import { mix, smooth } from "./math.js";

// The eye's place in a canvas of this size: its middle, its half width a and its half height b. It
// sits above the canvas's middle, which leaves the room under it for the plasma.
export const placeOf = (width, height) => ({ cx: width / 2, cy: height * 0.45, a: width * 0.33, b: height * 0.18, width, height });

// The sphere of the eye in pixels: its middle and radius, for the eye's half width a and half
// height b.
export const sphereOf = (cx, cy, a, b) => ({ x: cx, y: cy - b * 0.12, radius: a * 1.05 });

// The opening's half height as an angle on the sphere, for a canvas of this size.
export const tallOf = (box) => {
  const { cx, cy, a, b } = placeOf(box.width, box.height);
  return b / sphereOf(cx, cy, a, b).radius;
};

// The edge of the opening, in parts of the eye's half width across and half height down, from x = -1
// at the inner corner to 1 at the outer, at an openness from OPEN_FROM to 1. Both lids come to a
// point at each corner, the outer corner a little higher. The upper lid arches high and is pressed
// down a little toward the inner corner; the lower lid is a shallower arch.
const LID_POINTS = 64;
export function edges(open) {
  const upper = [];
  const lower = [];
  for (let step = 0; step <= LID_POINTS; step += 1) {
    const x = (step / LID_POINTS) * 2 - 1;
    const s = (x + 1) / 2;
    const corner = mix(0.06, -0.06, s);
    const round = Math.max(0, 1 - x * x);
    upper.push([x, corner - mix(0.08, 0.8, open) * round ** 0.85 * (0.8 + 0.2 * smooth(s))]);
    lower.push([x, corner + mix(0.1, 0.5, open) * round ** 0.9]);
  }
  return { upper, lower };
}

// A lid in pixels: each point with its place x along the lid and the way out of the opening there,
// up from the upper lid (`out` 1) and down from the lower (`out` -1).
export function lidOf(points, size, out) {
  const { cx, cy, a, b } = size;
  const pixels = points.map(([x, y]) => [cx + x * a, cy + y * b]);
  return pixels.map((p, at) => {
    const before = pixels[Math.max(0, at - 1)];
    const after = pixels[Math.min(pixels.length - 1, at + 1)];
    const across = after[0] - before[0];
    const down = after[1] - before[1];
    const length = Math.hypot(across, down) || 1;
    return { x: points[at][0], p, n: [(out * down) / length, (-out * across) / length] };
  });
}

// The point of a lid at x, from -1 to 1, and the way out there.
export function along(lid, x) {
  const at = Math.max(0, Math.min(lid.length - 1.001, ((x + 1) / 2) * (lid.length - 1)));
  const low = Math.floor(at);
  const part = at - low;
  const [one, two] = [lid[low], lid[low + 1]];
  const n = [mix(one.n[0], two.n[0], part), mix(one.n[1], two.n[1], part)];
  const length = Math.hypot(...n) || 1;
  return { p: [mix(one.p[0], two.p[0], part), mix(one.p[1], two.p[1], part)], n: [n[0] / length, n[1] / length] };
}

// How thick each lid is at x, in parts of the eye's half height: the upper heavy over the middle,
// the lower thin.
export const UPPER_BAND = (x) => 0.04 + 0.13 * Math.max(0, 1 - x * x) ** 0.7;
export const LOWER_BAND = (x) => 0.025 + 0.05 * Math.max(0, 1 - x * x) ** 0.7;

// A lid's band: the lid's own edge, then back along its outer edge, `band` thick.
function bandPath(pen, lid, band, b) {
  pen.beginPath();
  lid.forEach(({ p }, at) => (at === 0 ? pen.moveTo(p[0], p[1]) : pen.lineTo(p[0], p[1])));
  for (let at = lid.length - 1; at >= 0; at -= 1) {
    const { x, p, n } = lid[at];
    pen.lineTo(p[0] + n[0] * band(x) * b, p[1] + n[1] * band(x) * b);
  }
  pen.closePath();
}

function trace(pen, points, cx, cy, a, b, start = true) {
  points.forEach(([x, y], at) => (at === 0 && start ? pen.moveTo(cx + x * a, cy + y * b) : pen.lineTo(cx + x * a, cy + y * b)));
}

export function openingPath(pen, lid, cx, cy, a, b) {
  pen.beginPath();
  trace(pen, lid.upper, cx, cy, a, b);
  trace(pen, [...lid.lower].reverse(), cx, cy, a, b, false);
  pen.closePath();
}

// Under the irises: the socket, a soft dark round the opening that the eye sits in instead of on the
// page, and the white as the front of a sphere, lit from up and to the left and falling into shadow
// round the edge.
export function drawUnder(pen, size, lid) {
  const { cx, cy, a, b, width, height } = size;
  pen.save();
  pen.filter = `blur(${Math.max(4, b * 0.35)}px)`;
  openingPath(pen, edges(1), cx, cy, a * 1.06, b * 1.5);
  pen.fillStyle = cssOf("--eye-socket");
  pen.fill();
  pen.restore();

  pen.save();
  openingPath(pen, lid, cx, cy, a, b);
  pen.clip();
  const white = pen.createRadialGradient(cx - a * 0.15, cy - b * 0.3, b * 0.1, cx, cy, a * 1.02);
  white.addColorStop(0, cssOf("--eye-white-1"));
  white.addColorStop(0.5, cssOf("--eye-white-2"));
  white.addColorStop(1, cssOf("--eye-white-3"));
  pen.fillStyle = white;
  pen.fillRect(0, 0, width, height);
  pen.restore();
}

// Over the irises: the shadow the upper lid throws onto the eye, deep and wide, then the lids, dark.
export function drawOver(pen, size, lid) {
  const { cx, cy, a, b } = size;
  pen.save();
  openingPath(pen, lid, cx, cy, a, b);
  pen.clip();
  pen.filter = `blur(${Math.max(2, b * 0.16)}px)`;
  pen.lineCap = "round";
  pen.strokeStyle = cssOf("--eye-lid-shadow");
  pen.lineWidth = b * 0.6;
  pen.beginPath();
  trace(pen, lid.upper, cx, cy, a, b);
  pen.stroke();
  pen.restore();

  const upper = lidOf(lid.upper, size, 1);
  const lower = lidOf(lid.lower, size, -1);
  pen.save();
  pen.fillStyle = cssOf("--eye-dark");
  for (const [line, band] of [
    [upper, UPPER_BAND],
    [lower, LOWER_BAND],
  ]) {
    bandPath(pen, line, band, b);
    pen.fill();
  }
  pen.restore();
}
