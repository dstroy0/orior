// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fuse along the foot of a run's output: a line of the eye's plasma that burns from left to
// right while the run goes.
//
// A job of more than one step burns its fuse a step at a time: when the nth of its N steps starts,
// the fire has come (n - 1)/N of the way. A job of one step gives no measure of how far it has come,
// and its spark crawls the fuse's length in CRAWL seconds with a trail TRAIL pixels long behind it,
// and starts again at the left. Each line the run writes makes the spark flare. A run that ends well
// burns out to the end and flashes, at its brightest PEAK seconds after the end and gone FLASH
// seconds after that. One that fails or is stopped sputters out over SPUTTER seconds and leaves the
// fuse burnt dark as far as it came. A reader who asks the system for less motion gets the fuse
// drawn where it stands, with no spark.

import { DUSK, ORCHID, PLUM, addArc, addSpot, cornersOf, plasmaOn } from "./plasma.js";

const CRAWL = 4;
const TRAIL = 180;
const PEAK = 0.5;
const FLASH = 0.9;
const SPUTTER = 1.2;
// How much of the gap between where the fire is and where it should be closes a second.
const CATCH = 5;
// How fast a flare dies: the part of it left after a second is e to the minus FADE.
const FADE = 5;
// The sparks the head throws at a time, each a crooked line of SPARK_STEPS pieces, thrown anew
// every SPARK_EVERY seconds.
const SPARKS = 4;
const SPARK_STEPS = 3;
const SPARK_EVERY = 0.05;
// The embers burning up off the trail behind the head, each EMBER_GAP pixels behind the last.
const EMBERS = 6;
const EMBER_GAP = 13;
// Each piece of the fuse's line is at most PIECE pixels long.
const PIECE = 8;

const mix = (a, b, x) => a + (b - a) * x;

// Makes a fuse: its canvas, and what points it at a run and makes it flare.
export function makeFuse() {
  const canvas = Object.assign(document.createElement("canvas"), { className: "fuse" });
  canvas.setAttribute("aria-hidden", "true");
  const drawer = plasmaOn(canvas);
  const corners = cornersOf();
  const still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const fire = { run: null, steps: 1, burn: 0, crawl: 0, flare: 0, endedAt: null, sparks: [], sparkUntil: 0 };
  let frame = 0;
  let last = 0;

  // How far the fire should have come, from 0 to 1, or null where the run gives no measure.
  const aimed = () => {
    const { run, steps } = fire;
    if (run.done && !run.stopped && run.code === 0) {
      return 1;
    }
    if (steps > 1) {
      return Math.max(0, Math.min(1, (run.started - 1) / steps));
    }
    return null;
  };

  // A line of light from x0 to x1 about the height y, `half` to each side, waving `wave` pixels.
  const strip = (x0, x1, y, half, color, strength, along, wave, seconds) => {
    const pieces = Math.max(1, Math.ceil((x1 - x0) / PIECE));
    const at = (x, way) => [x, y + wave * Math.sin(x * 0.05 + seconds * 3) + half * way, way, along(x), color, strength];
    for (let piece = 0; piece < pieces; piece += 1) {
      const left = mix(x0, x1, piece / pieces);
      const right = mix(x0, x1, (piece + 1) / pieces);
      corners.four(at(left, -1), at(left, 1), at(right, 1), at(right, -1));
    }
  };

  // A small tongue burning up from x, y, `height` tall, leaning `sway` pixels at its tip.
  const ember = (x, y, height, half, sway, color, strength) => {
    const at = (s, way) => [x + sway * s * s + half * (1 - s) * way, y - height * s, way, s, color, strength];
    for (let piece = 0; piece < 3; piece += 1) {
      const low = piece / 3;
      const high = (piece + 1) / 3;
      corners.four(at(low, -1), at(low, 1), at(high, 1), at(high, -1));
    }
  };

  // New sparks off the head: crooked lines, most of them up and ahead.
  const sparksAt = (head, y) =>
    Array.from({ length: SPARKS }, () => {
      let turn = -Math.PI / 2 + (Math.random() - 0.3) * 2.4;
      const points = [[head, y]];
      for (let step = 0; step < SPARK_STEPS; step += 1) {
        turn += (Math.random() - 0.5) * 1.2;
        const [x, y0] = points[points.length - 1];
        const reach = 4 + Math.random() * 4;
        points.push([x + Math.cos(turn) * reach, y0 + Math.sin(turn) * reach]);
      }
      return points;
    });

  // Draws the fire `seconds` in, `dt` after the last frame, and says whether it has more to do.
  const paint = (seconds, dt) => {
    const box = canvas.getBoundingClientRect();
    if (!box.width || !drawer) {
      return false;
    }
    const scale = window.devicePixelRatio || 1;
    const wide = Math.round(box.width * scale);
    const high = Math.round(box.height * scale);
    if (canvas.width !== wide || canvas.height !== high) {
      canvas.width = wide;
      canvas.height = high;
    }
    const { width, height } = box;
    const y = height * 0.7;
    corners.clear();
    const { run } = fire;
    if (!run) {
      drawer.draw(corners.list, 0, width, height);
      return false;
    }
    if (run.done && fire.endedAt === null) {
      fire.endedAt = seconds;
    }
    const ended = fire.endedAt === null ? 0 : seconds - fire.endedAt;
    const aim = aimed();
    const well = run.done && aim === 1;
    const failed = run.done && !well;
    fire.flare *= Math.exp(-dt * FADE);

    let head;
    let tail = 0;
    if (aim === null && !run.done) {
      fire.crawl = still ? 0 : (fire.crawl + dt / CRAWL) % 1;
      fire.burn = fire.crawl;
      head = fire.crawl * width;
      tail = Math.max(0, head - TRAIL);
    } else {
      const target = aim ?? fire.burn;
      fire.burn += (target - fire.burn) * (still ? 1 : 1 - Math.exp(-dt * CATCH));
      head = fire.burn * width;
    }

    // How bright the whole fire is, how bright its head, and whether it has more to do.
    let life = 1;
    let boost = 1;
    let headLife = 1;
    let going = !still;
    if (well) {
      boost = 1 + 1.5 * Math.max(0, 1 - Math.abs(ended - PEAK) / 0.3);
      life = Math.max(0, 1 - Math.max(0, ended - PEAK) / FLASH);
      going = going && ended < PEAK + FLASH;
    } else if (failed) {
      const out = Math.min(1, ended / SPUTTER);
      life = mix(1, 0.35, out);
      headLife = (1 - out) * (Math.random() < 0.5 ? 1 : 0.3);
      going = going && out < 1;
    }

    const along = (x) => 0.15 + 0.8 * (1 - (x - tail) / (head - tail || 1));
    const trail = failed ? DUSK : PLUM;
    strip(aim === null && !run.done ? 0 : head, width, y, 1.3, DUSK, 0.6 * (well ? life : 1), () => 0.35, 0, seconds);
    if (head > tail) {
      strip(tail, head, y, 7, trail, 0.55 * life * boost, along, still ? 0 : 1.2, seconds);
      strip(tail, head, y, 1.8, failed ? PLUM : ORCHID, 0.85 * life * boost, along, still ? 0 : 1.2, seconds);
    }
    if (!run.done && !still) {
      for (let at = 0; at < EMBERS; at += 1) {
        const x = head - (at + 0.5) * EMBER_GAP;
        if (x < tail) {
          break;
        }
        const flicker = 0.5 + 0.5 * Math.sin(seconds * (7 + at * 1.3) + at * 2.1);
        ember(x, y, (8 + 9 * flicker) * (1 - (at / EMBERS) * 0.5), 3.5, Math.sin(seconds * 2 + at) * 3, at % 2 ? PLUM : ORCHID, 0.6);
      }
      if (seconds >= fire.sparkUntil) {
        fire.sparks = sparksAt(head, y);
        fire.sparkUntil = seconds + SPARK_EVERY;
      }
      for (const spark of fire.sparks) {
        addArc(corners, spark, 0.9, ORCHID, 0.7 * (0.5 + fire.flare));
      }
    }
    if (headLife > 0 && !(well && ended > PEAK)) {
      addSpot(corners, head, y, 12 * (1 + fire.flare * 0.8), ORCHID, (0.7 + 0.6 * fire.flare) * headLife * boost);
    }
    drawer.draw(corners.list, corners.count, width, height);
    return going;
  };

  const tick = (now) => {
    const dt = Math.min(0.05, Math.max(0, (now - last) / 1000));
    last = now;
    frame = paint(now / 1000, dt) ? requestAnimationFrame(tick) : 0;
  };

  // Draws the fire now, and keeps drawing it each frame while it has more to do.
  const wake = () => {
    if (frame) {
      return;
    }
    last = performance.now();
    if (paint(last / 1000, 0) && !still) {
      frame = requestAnimationFrame(tick);
    }
  };

  return {
    canvas,
    // Points the fuse at a run of a job of `steps` steps, or at nothing. A run already over when the
    // fuse first meets it is drawn as it ended.
    follow(run, steps) {
      if (run !== fire.run) {
        Object.assign(fire, { run, steps: Math.max(1, steps || 1), burn: 0, crawl: 0, flare: 0, endedAt: null, sparks: [], sparkUntil: 0 });
        if (run?.done) {
          fire.endedAt = -Infinity;
          fire.burn = fire.steps > 1 ? Math.max(0, Math.min(1, (run.started - 1) / fire.steps)) : 0;
        }
      }
      wake();
    },
    // Makes the spark flare, as each line the run writes does.
    flare() {
      fire.flare = Math.min(1.5, fire.flare + 0.6);
    },
  };
}
