// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A selection drawn as one shape: the span it covers on each row, rows one under the next, traced
// round as a single outline, every corner rounded. A corner where the shape turns out is rounded
// in, and one where it turns in, as where a longer row meets a shorter one, is filled out. Rows
// whose spans do not overlap are shapes of their own.

// The outline of rows that follow one another, each [row, left, right], as the corners in order:
// down the right side and back up the left.
function corners(group, height) {
  const points = [[group[0][1], group[0][0] * height]];
  for (const [row, , right] of group) {
    points.push([right, row * height], [right, (row + 1) * height]);
  }
  for (let at = group.length - 1; at >= 0; at -= 1) {
    const [row, left] = group[at];
    points.push([left, (row + 1) * height], [left, row * height]);
  }
  points.pop();
  // A corner at the same place as the one before it, or on the straight line between its two
  // neighbors, is no corner.
  const kept = points.filter((point, at) => {
    const before = points[(at - 1 + points.length) % points.length];
    return point[0] !== before[0] || point[1] !== before[1];
  });
  return kept.filter((point, at) => {
    const before = kept[(at - 1 + kept.length) % kept.length];
    const after = kept[(at + 1) % kept.length];
    return !((before[0] === point[0] && point[0] === after[0]) || (before[1] === point[1] && point[1] === after[1]));
  });
}

const fixed = (value) => Number(value.toFixed(1));

// The path of an outline with each corner rounded by `radius`, or less where a side is too short.
function rounded(points, radius) {
  const parts = [];
  points.forEach((point, at) => {
    const before = points[(at - 1 + points.length) % points.length];
    const after = points[(at + 1) % points.length];
    const toBefore = Math.hypot(before[0] - point[0], before[1] - point[1]);
    const toAfter = Math.hypot(after[0] - point[0], after[1] - point[1]);
    const r = Math.min(radius, toBefore / 2, toAfter / 2);
    const enter = [point[0] + ((before[0] - point[0]) / toBefore) * r, point[1] + ((before[1] - point[1]) / toBefore) * r];
    const leave = [point[0] + ((after[0] - point[0]) / toAfter) * r, point[1] + ((after[1] - point[1]) / toAfter) * r];
    parts.push(`${at ? "L" : "M"}${fixed(enter[0])} ${fixed(enter[1])}Q${fixed(point[0])} ${fixed(point[1])} ${fixed(leave[0])} ${fixed(leave[1])}`);
  });
  return `${parts.join("")}Z`;
}

// The path of a selection's spans, each [row, left, right] in pixels, the rows in order and each
// `height` tall.
export function selectionPath(spans, height, radius) {
  const shapes = [];
  let group = [];
  for (const span of spans) {
    const last = group.at(-1);
    if (last && (span[0] !== last[0] + 1 || span[1] >= last[2] || last[1] >= span[2])) {
      shapes.push(group);
      group = [];
    }
    if (span[2] > span[1]) {
      group.push(span);
    }
  }
  if (group.length) {
    shapes.push(group);
  }
  return shapes.map((one) => rounded(corners(one, height), radius)).join("");
}
