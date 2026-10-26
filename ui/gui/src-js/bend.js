// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The work gives way to a part that comes onto it, as oil does to water: its outline goes round each part
// on it with the frame's gap clear of the part, and its line bends round the part's side and foot, each
// turn on the work's own corner curve, the rest of its edges standing where they stood. Each frame while a
// part moves, the boxes the parts show are read, the work's outline is worked out from them, set as the
// work's clip and drawn as its line, each stretch of the line in the color and width of the work's edge it
// bends from. With nothing on the work, the work's own rounded box and its line stand.

const SVG = "http://www.w3.org/2000/svg";

// The frame's gap, as --frame-right sets it, and the radius of the work's corners.
const GAP = 12;
const ROUND = 7;

// The work's four edges, each the class its stretch of line is drawn in and the line's width.
const WIDTH = { top: 2, right: 2, foot: 1, left: 1 };

// What draws the box a part shows over the work, and the work's edge that part comes from; claim.js and
// the slats add theirs.
const shows = [];

export function showsOnWork(box, edge) {
  shows.push({ box, edge });
}

// What sets the parts as they stand on each frame, before the work's outline is read from them.
const before = [];

export function beforeBend(set) {
  before.push(set);
}

// The parts' boxes on the work, each grown by the claim's share of the gap, in the work's own pixels: a
// part standing beside the work the gap away from it takes nothing of it. A part come within two gaps of
// one of the work's edges takes the work to that edge, so no sliver of the work too thin to round stands
// between the two as a tail.
function holesIn(work) {
  const room = work.getBoundingClientRect();
  const gap = GAP * (Number.parseFloat(getComputedStyle(document.querySelector(".frame")).getPropertyValue("--claim")) || 0);
  const near = 2 * GAP;
  return shows
    .map(({ box, edge }) => ({ box: box(), edge }))
    .filter(({ box }) => box && box.right > box.left && box.bottom > box.top)
    .map(({ box, edge }) => ({
      left: Math.max(0, box.left - gap - room.left),
      top: Math.max(0, box.top - gap - room.top),
      right: Math.min(room.width, box.right + gap - room.left),
      bottom: Math.min(room.height, box.bottom + gap - room.top),
      edge,
    }))
    .filter((hole) => hole.right - hole.left > 0.5 && hole.bottom - hole.top > 0.5)
    .map((hole) => ({
      ...hole,
      left: hole.left < near ? 0 : hole.left,
      top: hole.top < near ? 0 : hole.top,
      right: room.width - hole.right < near ? room.width : hole.right,
      bottom: room.height - hole.bottom < near ? room.height : hole.bottom,
    }));
}

// The corners round the region of a `width` by `height` box less the holes, clockwise from the top left,
// each corner where the outline turns.
function cornersOf(width, height, holes) {
  const cuts = (ends) => [...new Set(ends)].sort((one, other) => one - other);
  const xs = cuts([0, width, ...holes.flatMap((hole) => [hole.left, hole.right])]);
  const ys = cuts([0, height, ...holes.flatMap((hole) => [hole.top, hole.bottom])]);
  const inside = (column, row) => {
    if (column < 0 || row < 0 || column >= xs.length - 1 || row >= ys.length - 1) {
      return false;
    }
    const x = (xs[column] + xs[column + 1]) / 2;
    const y = (ys[row] + ys[row + 1]) / 2;
    return !holes.some((hole) => x > hole.left && x < hole.right && y > hole.top && y < hole.bottom);
  };
  // Each stretch of the outline from one grid point to the next, clockwise round the region, kept by the
  // point it starts at.
  const next = new Map();
  const at = (x, y) => `${x},${y}`;
  const add = (fromX, fromY, toX, toY) => next.set(at(fromX, fromY), [toX, toY]);
  for (let column = 0; column < xs.length - 1; column += 1) {
    for (let row = 0; row < ys.length - 1; row += 1) {
      if (!inside(column, row)) {
        continue;
      }
      const [left, right, top, bottom] = [xs[column], xs[column + 1], ys[row], ys[row + 1]];
      if (!inside(column, row - 1)) {
        add(left, top, right, top);
      }
      if (!inside(column + 1, row)) {
        add(right, top, right, bottom);
      }
      if (!inside(column, row + 1)) {
        add(right, bottom, left, bottom);
      }
      if (!inside(column - 1, row)) {
        add(left, bottom, left, top);
      }
    }
  }
  const first = [...next.keys()][0];
  if (!first) {
    return [];
  }
  const points = [];
  let key = first;
  do {
    const [x, y] = key.split(",").map(Number);
    points.push([x, y]);
    const [toX, toY] = next.get(key);
    key = at(toX, toY);
  } while (key !== first && points.length <= next.size);
  // A point where the outline runs straight on is no corner.
  return points.filter((point, index) => {
    const before = points[(index + points.length - 1) % points.length];
    const after = points[(index + 1) % points.length];
    return (before[0] - point[0]) * (after[1] - point[1]) - (before[1] - point[1]) * (after[0] - point[0]) !== 0;
  });
}

const unit = ([fromX, fromY], [toX, toY]) => {
  const length = Math.hypot(toX - fromX, toY - fromY);
  return [(toX - fromX) / length, (toY - fromY) / length];
};
const apart = ([fromX, fromY], [toX, toY]) => Math.hypot(toX - fromX, toY - fromY);
const point = ([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`;

// The work's edge a stretch of the outline belongs to: the box's own edge it lies on, or the edge of the
// part whose hole it runs round.
function edgeOf(from, to, width, height, holes) {
  const [x, y] = [(from[0] + to[0]) / 2, (from[1] + to[1]) / 2];
  const near = (one, other) => Math.abs(one - other) < 0.5;
  if (near(from[1], to[1]) && near(y, 0)) {
    return "top";
  }
  if (near(from[0], to[0]) && near(x, width)) {
    return "right";
  }
  if (near(from[1], to[1]) && near(y, height)) {
    return "foot";
  }
  if (near(from[0], to[0]) && near(x, 0)) {
    return "left";
  }
  const hole = holes.find((one) => x >= one.left - 0.5 && x <= one.right + 0.5 && y >= one.top - 0.5 && y <= one.bottom + 0.5);
  return hole?.edge ?? "top";
}

// The outline through the corners, each rounded ROUND or half its shorter side, as the clip's path, and
// the line of each edge inside it as that edge's path.
function drawn(corners, width, height, holes) {
  const count = corners.length;
  const turns = corners.map((corner, index) => {
    const before = corners[(index + count - 1) % count];
    const after = corners[(index + 1) % count];
    const inward = unit(before, corner);
    const outward = unit(corner, after);
    const radius = Math.min(ROUND, apart(before, corner) / 2, apart(corner, after) / 2);
    const start = [corner[0] - inward[0] * radius, corner[1] - inward[1] * radius];
    const end = [corner[0] + outward[0] * radius, corner[1] + outward[1] * radius];
    const center = [start[0] + outward[0] * radius, start[1] + outward[1] * radius];
    const sweep = inward[0] * outward[1] - inward[1] * outward[0] > 0 ? 1 : 0;
    return { start, end, center, radius, sweep };
  });
  let clip = `M${point(turns[0].end)}`;
  for (let index = 1; index <= count; index += 1) {
    const turn = turns[index % count];
    clip += `L${point(turn.start)}A${turn.radius} ${turn.radius} 0 0 ${turn.sweep} ${point(turn.end)}`;
  }
  const lines = { top: "", right: "", foot: "", left: "" };
  for (let index = 0; index < count; index += 1) {
    const from = turns[index];
    const to = turns[(index + 1) % count];
    const edge = edgeOf(from.end, to.start, width, height, holes);
    const [dx, dy] = unit(from.end, to.start).map((part) => (Number.isFinite(part) ? part : 0));
    const depth = WIDTH[edge] / 2;
    // A point moved in from the outline by the line's half width, to the right of the way round.
    const shift = ([x, y]) => [x - dy * depth, y + dx * depth];
    // Half of each turn belongs to the stretch it ends or starts, drawn at the stretch's own depth: a point
    // moved in from a turn's end stays on the ray from the turn's center, so the half runs to the middle
    // of the turn at the moved point's distance from the center.
    const half = (turn, moved) => {
      const radius = apart(moved, turn.center);
      if (!radius) {
        return null;
      }
      const [ax, ay] = unit(turn.center, turn.start);
      const [bx, by] = unit(turn.center, turn.end);
      const [mx, my] = unit([0, 0], [ax + bx, ay + by]);
      return { radius, middle: [turn.center[0] + mx * radius, turn.center[1] + my * radius] };
    };
    const start = shift(from.end);
    const end = shift(to.start);
    const opening = from.radius ? half(from, start) : null;
    const closing = to.radius ? half(to, end) : null;
    let path = opening ? `M${point(opening.middle)}A${opening.radius} ${opening.radius} 0 0 ${from.sweep} ${point(start)}` : `M${point(start)}`;
    path += `L${point(end)}`;
    if (closing) {
      path += `A${closing.radius} ${closing.radius} 0 0 ${to.sweep} ${point(closing.middle)}`;
    }
    lines[edge] += path;
  }
  return { clip: `${clip}Z`, lines };
}

// The work's outline and line as the parts on it stand now.
export function bendWork() {
  const work = document.querySelector(".work");
  const line = work?.querySelector(":scope > .work-line");
  if (!line) {
    return;
  }
  before.forEach((set) => set());
  const holes = holesIn(work);
  if (!holes.length) {
    if (work.hasAttribute("data-bent")) {
      work.removeAttribute("data-bent");
      work.style.clipPath = "";
      line.querySelectorAll("path").forEach((path) => path.setAttribute("d", ""));
    }
    return;
  }
  const { width, height } = work.getBoundingClientRect();
  const corners = cornersOf(width, height, holes);
  if (corners.length < 4) {
    return;
  }
  const { clip, lines } = drawn(corners, width, height, holes);
  work.setAttribute("data-bent", "");
  work.style.clipPath = `path("${clip}")`;
  for (const [edge, d] of Object.entries(lines)) {
    line.querySelector(`.${edge}`).setAttribute("d", d);
  }
}

// The transitions under way on the parts and the work, each kept by what runs it, and the frame drawn next. An element
// taken out of the page while it moves hears no end of its transition, so it is let go once it stands outside the page.
const moving = new Map();
let frame = 0;

const keyOf = (event) => `${event.propertyName}${event.pseudoElement}`;

function run() {
  if (frame) {
    return;
  }
  const step = () => {
    for (const target of [...moving.keys()]) {
      if (!target.isConnected) {
        moving.delete(target);
      }
    }
    bendWork();
    frame = moving.size ? requestAnimationFrame(step) : 0;
  };
  frame = requestAnimationFrame(step);
}

function ended(event) {
  const keys = moving.get(event.target);
  if (!keys) {
    return;
  }
  keys.delete(keyOf(event));
  if (!keys.size) {
    moving.delete(event.target);
  }
  if (!moving.size) {
    bendWork();
  }
}

// Draws the work's line and watches every transition of the page for one that moves a part.
export function startBend() {
  const work = document.querySelector(".work");
  const line = document.createElementNS(SVG, "svg");
  line.setAttribute("class", "work-line");
  line.setAttribute("aria-hidden", "true");
  for (const edge of Object.keys(WIDTH)) {
    const path = document.createElementNS(SVG, "path");
    path.setAttribute("class", edge);
    line.append(path);
  }
  work.append(line);
  document.addEventListener("transitionrun", (event) => {
    if (!moving.has(event.target)) {
      moving.set(event.target, new Set());
    }
    moving.get(event.target).add(keyOf(event));
    run();
  });
  document.addEventListener("transitionend", ended);
  document.addEventListener("transitioncancel", ended);
  new ResizeObserver(() => bendWork()).observe(work);
}
