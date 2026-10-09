// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fuse along the foot of a run's output: a line of the eye's plasma that burns from left to
// right while the run goes, on the scale of the time ruler under it.
//
// The fire's head stands where the run's latest moment falls on the ruler, and the fuse burnt
// behind it is the time gone. Zooming or moving the ruler moves the fire with it, and a ruler moved
// back from the latest moment shows the fuse burnt across its whole width. Each line the run writes
// makes the head flare. A run that ends well flashes, at its brightest PEAK seconds after the end
// and gone FLASH seconds after that. One that fails or is stopped sputters out over SPUTTER seconds
// and leaves the fuse burnt dark as far as it came. A reader who asks the system for less motion
// gets the fuse drawn where it stands, with no spark.
//
// The fuse not yet burnt is --fuse-ash, the trail and the embers --fuse-burn and --fuse-fire, and
// the head and its sparks --fuse-fire.
//
// While the run goes, the head twitches where it stands: it wanders as far as JITTER pixels on
// quick waves that never line up, and throws its sparks from wherever it is.

import { rgbOf } from "./colors.js";
import { clock, still as paused, whenMoving } from "./motion.js";
import { addArc, addSpot, cornersOf, plasmaOn } from "./plasma.js";

const PEAK = 0.5;
const FLASH = 0.9;
const SPUTTER = 1.2;
// How fast a flare dies: the part of it left after a second is e to the minus FADE.
const FADE = 5;
// The sparks the head throws at a time, each a crooked line of SPARK_STEPS pieces, thrown anew
// every SPARK_EVERY seconds.
const SPARKS = 4;
const SPARK_STEPS = 3;
const SPARK_EVERY = 0.05;
// The embers burning up off the trail behind the head, each EMBER_GAP pixels behind the last and
// wider than the gap, which runs them together into one flame along the trail. Each stands on the
// trail where it waves, as much as WAVE pixels up and down.
const EMBERS = 24;
const EMBER_GAP = 4;
const WAVE = 1.2;
// Each piece of the fuse's line is at most PIECE pixels long, and NOSE near its ends, where it
// rounds off over TAPER pixels.
const PIECE = 8;
const NOSE = 2;
const TAPER = 12;
// How far the head wanders.
const JITTER = 3;
// How far the canvas reaches past the fuse's box on every side, its glow, its sparks and its embers
// drawn whole there over what lies beside it, and the fuse itself laid out on the box alone.
export const REACH = 24;

const mix = (a, b, x) => a + (b - a) * x;

// Makes a fuse: its canvas, and what points it at a run and makes it flare. `place()` is how far
// across the run's latest moment falls on the ruler, 0 at the left and 1 at the right.
export function makeFuse(place) {
  const canvas = Object.assign(document.createElement("canvas"), { className: "fuse" });
  canvas.setAttribute("aria-hidden", "true");
  const drawer = plasmaOn(canvas);
  const corners = cornersOf();
  const still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const fire = { run: null, flare: 0, endedAt: null, sparks: [], sparkUntil: 0 };
  let frame = 0;
  let last = 0;

  // A line of light from x0 to x1 about the height y, `half` to each side, waving `wave` pixels. Its
  // width rounds off to a point within TAPER of each end, where it is cut into pieces NOSE pixels
  // long to keep the curve smooth.
  const strip = (x0, x1, y, half, color, strength, along, wave, seconds) => {
    const width = (x) => half * Math.sqrt(Math.max(0, Math.min(1, (x - x0) / TAPER, (x1 - x) / TAPER)));
    const at = (x, way) => [x, y + wave * Math.sin(x * 0.05 + seconds * 3) + width(x) * way, way, along(x), color, strength];
    let left = x0;
    while (left < x1) {
      const near = Math.min(left - x0, x1 - left) < TAPER;
      const right = Math.min(x1, left + (near ? NOSE : PIECE));
      corners.four(at(left, -1), at(left, 1), at(right, 1), at(right, -1));
      left = right;
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
    const width = box.width - 2 * REACH;
    const height = box.height - 2 * REACH;
    const y = height * 0.7;
    corners.clear();
    const { run } = fire;
    const DUSK = rgbOf("--fuse-ash");
    const PLUM = rgbOf("--fuse-burn");
    const ORCHID = rgbOf("--fuse-fire");
    if (!run) {
      drawer.draw(corners.list, 0, box.width, box.height);
      return false;
    }
    if (run.done && fire.endedAt === null) {
      fire.endedAt = seconds;
    }
    const ended = fire.endedAt === null ? 0 : seconds - fire.endedAt;
    const well = run.done && !run.stopped && run.code === 0;
    const failed = run.done && !well;
    fire.flare *= Math.exp(-dt * FADE);

    const head = Math.max(0, Math.min(width + TAPER, (place() ?? 0) * width));
    const tail = 0;

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

    // Where the head is drawn: where the fire has come, and, while the run goes, twitching about it.
    let headX = head;
    let headY = y;
    if (!run.done && !still) {
      headX += JITTER * Math.sin(seconds * 13.1) * Math.sin(seconds * 7.3 + 1);
      headY += JITTER * 0.6 * Math.sin(seconds * 17.7 + 2) * Math.sin(seconds * 5.9);
    }

    const along = (x) => 0.15 + 0.8 * (1 - (x - tail) / (head - tail || 1));
    const trail = failed ? DUSK : PLUM;
    if (head < width) {
      strip(head, width, y, 1.3, DUSK, 0.6 * (well ? life : 1), () => 0.35, 0, seconds);
    }
    if (head > tail) {
      strip(tail, head, y, 7, trail, 0.55 * life * boost, along, still ? 0 : WAVE, seconds);
      strip(tail, head, y, 1.8, failed ? PLUM : ORCHID, 0.85 * life * boost, along, still ? 0 : WAVE, seconds);
    }
    if (!run.done && !still) {
      for (let at = 0; at < EMBERS; at += 1) {
        const x = head - (at + 0.5) * EMBER_GAP;
        if (x < tail) {
          break;
        }
        const flicker = 0.5 + 0.3 * Math.sin(seconds * (9 + at * 0.7) + at * 2.1) + 0.2 * Math.sin(seconds * (17 + at * 1.1) + at);
        const base = y + WAVE * Math.sin(x * 0.05 + seconds * 3);
        ember(x, base, (6 + 12 * flicker) * (1 - (at / EMBERS) * 0.75), 4.5, Math.sin(seconds * 2 + at * 0.4) * 3, at % 2 ? PLUM : ORCHID, 0.45);
      }
      if (seconds >= fire.sparkUntil) {
        fire.sparks = sparksAt(headX, headY);
        fire.sparkUntil = seconds + SPARK_EVERY;
      }
      for (const spark of fire.sparks) {
        addArc(corners, spark, 0.9, ORCHID, 0.7 * (0.5 + fire.flare));
      }
    }
    if (headLife > 0 && !(well && ended > PEAK)) {
      addSpot(corners, headX, headY, 12 * (1 + fire.flare * 0.8), ORCHID, (0.7 + 0.6 * fire.flare) * headLife * boost);
    }
    corners.shift(REACH, REACH);
    drawer.draw(corners.list, corners.count, box.width, box.height);
    return going;
  };

  // The fire keeps its time on the animation's clock, and while motion is stopped holds the frame it
  // is at.
  const tick = (moment) => {
    const now = clock(moment);
    const dt = Math.min(0.05, Math.max(0, (now - last) / 1000));
    last = now;
    const going = paint(now / 1000, dt);
    frame = going && !paused() ? requestAnimationFrame(tick) : 0;
    if (going && paused()) {
      whenMoving(wake);
    }
  };

  // Draws the fire now, and keeps drawing it each frame while it has more to do.
  const wake = () => {
    if (frame) {
      return;
    }
    last = clock();
    if (!paint(last / 1000, 0) || still) {
      return;
    }
    if (paused()) {
      whenMoving(wake);
    } else {
      frame = requestAnimationFrame(tick);
    }
  };

  return {
    canvas,
    // Points the fuse at a run, or at nothing. A run already over when the fuse first meets it is
    // drawn as it ended.
    follow(run) {
      if (run !== fire.run) {
        Object.assign(fire, { run, flare: 0, endedAt: run?.done ? -Infinity : null, sparks: [], sparkUntil: 0 });
      }
      wake();
    },
    // Makes the spark flare, as each line the run writes does.
    flare() {
      fire.flare = Math.min(1.5, fire.flare + 0.6);
    },
  };
}
