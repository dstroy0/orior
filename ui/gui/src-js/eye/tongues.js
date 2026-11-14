// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The plasma that burns round the opening: its tongues along both lids, the rim they rise from, and the arcs that
// crawl out from it.

import { addArc, dusk, orchid, plum } from "../plasma.js";
import { LOWER_BAND, UPPER_BAND, along, lidOf } from "./lids.js";
import { mix, smooth } from "./math.js";

// The plasma's tongues, drawn from a seed once: ABOVE along the upper lid and BELOW along the lower,
// spread evenly from corner to corner with a little jitter. A tongue is at x along its lid, from
// -1 to 1. It is `length` long and `width` wide at its widest, both parts of the eye's half height,
// and longest over the middle; those of the upper lid are shorter, which retains them inside the
// canvas. It flickers longer and shorter on two waves, `rates` and `phases`, and wavers side to side
// on two more, `waves`. `color` is its outer glow, `heart` the light inside that, and the core is
// white hot.
const ABOVE = 30;
const BELOW = 26;
export function tonguesOf(draw) {
  const spaced = (count, at) => -0.96 + (1.92 * (at + 0.5 + (draw() - 0.5) * 0.6)) / count;
  const tongue = (x, reach) => {
    const round = Math.max(0, 1 - x * x);
    const heat = draw();
    return {
      x,
      length: reach(round) * (0.7 + draw() * 0.4),
      width: 0.13 + draw() * 0.08,
      rates: [2 + draw() * 3, 5 + draw() * 5],
      phases: [draw() * Math.PI * 2, draw() * Math.PI * 2],
      waves: [
        [1 + draw() * 1.5, 3 + draw() * 3, draw() * Math.PI * 2],
        [2.5 + draw() * 2, 6 + draw() * 5, draw() * Math.PI * 2],
      ],
      color: heat < 0.55 ? plum() : heat < 0.85 ? dusk() : orchid(),
      heart: heat < 0.5 ? orchid() : plum(),
    };
  };
  const upper = Array.from({ length: ABOVE }, (_, at) => tongue(spaced(ABOVE, at), (round) => 0.4 + 0.9 * round ** 1.2));
  const lower = Array.from({ length: BELOW }, (_, at) => tongue(spaced(BELOW, at), (round) => 0.6 + 1.4 * round ** 1.2));
  return { upper, lower };
}

// The rim the plasma burns from: the outer edge of the upper lid from corner to corner and back
// along the outer edge of the lower, one closed line round the eye. Each point has the way out of
// the eye there, smoothed with its neighbors, which turns the two lids' ways into one at each corner.
function rimOf(upper, lower, b) {
  const edge = (lid, band) => lid.map(({ x, p, n }) => ({ p: [p[0] + n[0] * band(x) * b, p[1] + n[1] * band(x) * b], n }));
  const rim = [...edge(upper, UPPER_BAND), ...edge(lower, LOWER_BAND).reverse().slice(1, -1)];
  for (let pass = 0; pass < 3; pass += 1) {
    const ways = rim.map((_, at) => {
      const before = rim[(at - 1 + rim.length) % rim.length].n;
      const after = rim[(at + 1) % rim.length].n;
      const own = rim[at].n;
      const sum = [before[0] + own[0] * 2 + after[0], before[1] + own[1] * 2 + after[1]];
      const length = Math.hypot(...sum) || 1;
      return [sum[0] / length, sum[1] / length];
    });
    ways.forEach((way, at) => (rim[at].n = way));
  }
  return rim;
}

// The plasma is drawn by the GPU on a canvas of its own over the eye, in plasma.js. Each tongue is a
// line of STRANDS pieces from the rim, leaving it square to it and turning, by BEND of the way, to
// straight up above the eye and straight down below it. Its glow reaches GLOW times its own width to
// each side and its heart, in a second color, HEART times. The rim burns along its whole length:
// a narrow bright line on it, RIM_LINE wide to each side, in a wide faint glow, RIM_GLOW wide and
// mostly outside it. ARCS sparks crawl out from the rim at a time, each living a span of SPARK
// seconds.
const STRANDS = 12;
const BEND = 0.85;
const GLOW = 2;
const HEART = 0.9;
const RIM_LINE = 0.07;
const RIM_GLOW = 0.3;
const ARCS = 2;
const SPARK = [0.06, 0.16];

// A tongue's half width at each of its points, as a part of its widest: narrow where it leaves the
// rim, full a quarter of the way out, and tapering to nothing at the tip.
const SHAPE = Array.from({ length: STRANDS + 1 }, (_, step) => {
  const s = step / STRANDS;
  return (1 - s) ** 0.8 * (0.55 + 0.45 * smooth(Math.min(1, s * 4)));
});

// A new set of arcs: each a crooked line out from the root of a tongue, leaning the way its tongues
// turn, up or down, and sometimes forked. A start is the root, that way as an angle, and how far an
// arc from it may reach, as a part of the farthest.
function sparksOf(starts, b, draw) {
  const arcs = [];
  for (let at = 0; at < ARCS; at += 1) {
    const { p, home, reach: far } = starts[Math.floor(draw() * starts.length)];
    let heading = home + (draw() - 0.5) * 1.2;
    const points = [p];
    const steps = Math.round((8 + Math.floor(draw() * 7)) * far);
    for (let step = 0; step < steps; step += 1) {
      heading += (draw() - 0.5) * 1.4;
      heading = mix(heading, home, 0.2);
      const last = points[points.length - 1];
      const reach = b * (0.08 + draw() * 0.1);
      points.push([last[0] + Math.cos(heading) * reach, last[1] + Math.sin(heading) * reach]);
    }
    arcs.push(points);
    if (draw() < 0.4) {
      const from = points[Math.floor(points.length / 2)];
      const fork = [from];
      let turn = heading + (draw() < 0.5 ? -0.9 : 0.9);
      for (let step = 0; step < 5; step += 1) {
        turn += (draw() - 0.5) * 1.2;
        const last = fork[fork.length - 1];
        fork.push([last[0] + Math.cos(turn) * b * 0.08, last[1] + Math.sin(turn) * b * 0.08]);
      }
      arcs.push(fork);
    }
  }
  return arcs;
}

// A tongue as pieces down its length, each `half` times its own width to either side of the middle.
function addTongue(corners, middle, sides, half, color, strength) {
  for (let step = 0; step < STRANDS; step += 1) {
    const edge = (at, way) => {
      const out = half * SHAPE[at] * way;
      return [middle[at][0] + sides[at][0] * out, middle[at][1] + sides[at][1] * out, way, at / STRANDS, color, strength];
    };
    corners.four(edge(step, -1), edge(step, 1), edge(step + 1, 1), edge(step + 1, -1));
  }
}

// A strip of light along the whole rim, `half` wide to each side of a line `out` from the rim.
function addRim(corners, rim, out, half, color, strength) {
  const edge = (at, way) => {
    const { p, n } = rim[at % rim.length];
    const off = out + half * way;
    return [p[0] + n[0] * off, p[1] + n[1] * off, way, 0.05, color, strength];
  };
  for (let at = 0; at < rim.length; at += 1) {
    corners.four(edge(at, -1), edge(at, 1), edge(at + 1, 1), edge(at + 1, -1));
  }
}

// Draws the plasma for this moment. `grown` is how far it has come up, from 0 while the eye is a
// slit to 1 once it is open.
export function drawPlasma(size, lid, state, grown) {
  if (!state.plasma) {
    return;
  }
  const { width, height, b } = size;
  const corners = state.corners;
  corners.clear();
  if (grown > 0) {
    const seconds = state.seconds;
    const upper = lidOf(lid.upper, size, 1);
    const lower = lidOf(lid.lower, size, -1);
    const rim = rimOf(upper, lower, b);
    // The rim, breathing a little: its glow pushed out from it by the majority of its width, and the
    // bright line on it.
    const breath = grown * (0.8 + 0.2 * Math.sin(seconds * 3.1) * Math.sin(seconds * 1.7 + 1));
    addRim(corners, rim, b * RIM_GLOW * 0.6, b * RIM_GLOW, dusk(), 0.5 * breath);
    addRim(corners, rim, 0, b * RIM_LINE, orchid(), 0.55 * breath);
    const starts = [];
    for (const [lid, band, tongues, home, reach] of [
      [upper, UPPER_BAND, state.tongues.upper, -Math.PI / 2, 0.55],
      [lower, LOWER_BAND, state.tongues.lower, Math.PI / 2, 1],
    ]) {
      const up = [Math.cos(home), Math.sin(home)];
      for (const tongue of tongues) {
        const { p, n } = along(lid, tongue.x);
        const root = [p[0] + n[0] * band(tongue.x) * b, p[1] + n[1] * band(tongue.x) * b];
        starts.push({ p: root, home, reach });
        const [slow, fast] = tongue.rates;
        const length = tongue.length * b * grown * (0.82 + 0.12 * Math.sin(slow * seconds + tongue.phases[0]) + 0.08 * Math.sin(fast * seconds + tongue.phases[1]));
        // The tongue's middle line, and the way square to it at each point.
        const middle = [root];
        const sides = [];
        for (let step = 1; step <= STRANDS; step += 1) {
          const turned = Math.min(1, ((step / STRANDS) * 1.2)) * BEND;
          const bent = [mix(n[0], up[0], turned), mix(n[1], up[1], turned)];
          const norm = Math.hypot(...bent) || 1;
          const last = middle[middle.length - 1];
          middle.push([last[0] + (bent[0] / norm) * (length / STRANDS), last[1] + (bent[1] / norm) * (length / STRANDS)]);
        }
        for (let step = 0; step <= STRANDS; step += 1) {
          const s = step / STRANDS;
          const before = middle[Math.max(0, step - 1)];
          const after = middle[Math.min(STRANDS, step + 1)];
          const norm = Math.hypot(after[0] - before[0], after[1] - before[1]) || 1;
          const side = [-(after[1] - before[1]) / norm, (after[0] - before[0]) / norm];
          const [one, two] = tongue.waves;
          const sway = 0.22 * length * s ** 1.3 * (0.65 * Math.sin(one[0] * s * Math.PI * 2 - one[1] * seconds + one[2]) + 0.35 * Math.sin(two[0] * s * Math.PI * 2 - two[1] * seconds + two[2]));
          middle[step] = [middle[step][0] + side[0] * sway, middle[step][1] + side[1] * sway];
          sides.push(side);
        }
        const wide = tongue.width * b * grown;
        addTongue(corners, middle, sides, wide * GLOW, tongue.color, 0.4);
        addTongue(corners, middle, sides, wide * HEART, tongue.heart, 0.3);
      }
    }

    if (grown >= 1 && !state.still) {
      if (seconds >= state.sparkUntil) {
        state.arcs = sparksOf(starts, b, state.draw);
        state.sparkUntil = seconds + mix(SPARK[0], SPARK[1], state.draw());
      }
      for (const arc of state.arcs) {
        addArc(corners, arc, b * 0.04, orchid(), 0.6);
      }
    }
  }
  state.plasma.draw(corners.list, corners.count, width, height);
}
