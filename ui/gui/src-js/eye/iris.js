// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// An iris: its plasma torus and its pupil, drawn as one picture that each iris positions down turned and squeezed.

import { cssOf, rgbOf, withAlpha } from "../colors.js";
import { mix, smooth } from "./math.js";

// The iris is a plasma torus: a ring of plasma round the pupil, TORUS of the iris's radius out and
// TUBE of it thick, over a glow from yellow at the pupil to a dark red at the iris's edge. FILAMENTS
// strands wind round the ring's tube as they run round the ring, each `winds` times, and flow round
// the tube as the fire goes, which carries the windings round the ring. Each strand is a wide glow
// with a hot core, bright where it runs over the front of the tube and dim round the back, drawn in
// PIECES pieces a turn.
//
// The fire is drawn once into BURNS pictures over a loop of LOOP seconds, each the first time it is
// displayed, and from then on each frame positions down the picture for its moment in the loop. Each strand
// flows a whole number of turns in a loop, which joins the loop's end to its start.
const TORUS = 0.6;
const TUBE = 0.17;
const FILAMENTS = 7;
const PIECES = 160;
const LOOP = 2;
const BURNS = 48;

function filamentsOf(draw) {
  return Array.from({ length: FILAMENTS }, (_, at) => ({
    winds: 5 + Math.floor(draw() * 4),
    phase: (at / FILAMENTS) * Math.PI * 2 + draw() * 0.4,
    flows: (draw() < 0.5 ? 1 : 2) * ((Math.PI * 2) / LOOP),
    width: 0.6 + draw() * 0.8,
  }));
}

// A radial fill from `stops`, each a place from the middle out and an rgba color.
function glowOf(pen, r, stops) {
  const glow = pen.createRadialGradient(0, 0, 0, 0, 0, r);
  for (const [at, color] of stops) {
    glow.addColorStop(at, color);
  }
  return glow;
}

// The fire's stops, each a place along the radius and the stylesheet's color for it.
const stopsOf = (stops) => stops.map(([at, name]) => [at, cssOf(name)]);
const GROUND = [
  [0, "--eye-iris-1"],
  [0.3, "--eye-iris-2"],
  [0.65, "--eye-iris-3"],
  [1, "--eye-iris-4"],
];
// The torus `seconds` into the fire, in an iris of radius r, positioned over what is under it: the ring's
// body, a soft band strongest along its middle, then each strand piece by piece.
function drawTorus(pen, filaments, r, seconds) {
  const ring = TORUS * r;
  const tube = TUBE * r;
  const body = pen.createRadialGradient(0, 0, Math.max(0, ring - tube * 1.6), 0, 0, ring + tube * 1.6);
  body.addColorStop(0, withAlpha("--eye-torus-glow", 0));
  body.addColorStop(0.5, cssOf("--eye-torus-glow"));
  body.addColorStop(1, withAlpha("--eye-torus-glow", 0));
  pen.fillStyle = body;
  pen.fillRect(-r, -r, r * 2, r * 2);
  pen.lineCap = "round";
  const strand = cssOf("--eye-torus-strand");
  const core = cssOf("--eye-torus-core");
  for (const filament of filaments) {
    const point = (step) => {
      const turn = (step / PIECES) * Math.PI * 2;
      const twist = filament.winds * turn + filament.phase - filament.flows * seconds;
      const out = ring + tube * Math.cos(twist);
      return [Math.cos(turn) * out, Math.sin(turn) * out, Math.sin(twist)];
    };
    let from = point(0);
    for (let step = 1; step <= PIECES; step += 1) {
      const to = point(step);
      pen.globalAlpha = 0.06 + 0.7 * smooth((from[2] + to[2] + 2) / 4) ** 1.5;
      pen.beginPath();
      pen.moveTo(from[0], from[1]);
      pen.lineTo(to[0], to[1]);
      pen.strokeStyle = strand;
      pen.lineWidth = tube * 0.22 * filament.width;
      pen.stroke();
      pen.strokeStyle = core;
      pen.lineWidth = Math.max(0.5, tube * 0.045 * filament.width);
      pen.stroke();
      from = to;
    }
  }
  pen.globalAlpha = 1;
}

// The pupil is the dark where the bodies' summed pull, each body's size squared over the squared
// distance to it, comes to 1 or more. It is worked out on a FIELD by FIELD grid over the middle of
// the iris, REACH of its radius each way, and drawn scaled up, which leaves its edge soft.
const FIELD = 72;
const REACH = 0.62;

// Each pupil leans toward the mouse, as far as LOOK of its iris's radius when the mouse is three
// radii off or more and less when it is nearer, closing LEAN of the gap a second.
export const LOOK = 0.3;
export const LEAN = 6;

// The pupils shine back as a dog's or a cat's do, from the layer behind the retina that throws the
// light that comes in back out through it, the tapetum lucidum. The shine is deep in the pupil: none
// where the pull is GLOWS[0] or less, near the pupil's edge, and full where it is GLOWS[1] or more.
// It goes from --eye-pupil-edge out there to --eye-pupil-shine in the middle. A reflection is brightest when the eye
// looks straight at whoever is looking, and the pupils come up to full shine while the eye stares at
// the reader and fall to DIM of it while it rolls, closing GLINT of the gap a second.
const GLOWS = [1.1, 3];
export const DIM = 0.25;
export const GLINT = 4;

// The bodies of the pupil. Each wanders on two slow waves, one across and one down, and swells and
// shrinks a little. The first is large and near the middle; the others swing out far enough to part
// from it.
export function lavaOf(draw) {
  const body = (wide, size) => ({
    across: wide * (0.7 + draw() * 0.3),
    down: wide * (0.5 + draw() * 0.4),
    rates: [0.35 + draw() * 0.7, 0.3 + draw() * 0.7, 0.8 + draw() * 0.8],
    phases: [draw() * Math.PI * 2, draw() * Math.PI * 2],
    size,
  });
  return [body(0.06, 0.3), body(0.3, 0.13 + draw() * 0.05), body(0.32, 0.12 + draw() * 0.05), body(0.26, 0.11 + draw() * 0.04)];
}

// Every iris is one picture at the size of a whole iris, positioned down for each iris turned by its own
// angle, squeezed and scaled. A divided eye costs a frame what one iris costs, however many irises
// it holds.
export function irisPicture(draw) {
  const filaments = filamentsOf(draw);
  let burns = [];
  let burnSide = 0;
  const field = document.createElement("canvas");
  field.width = FIELD;
  field.height = FIELD;
  const fieldPen = field.getContext("2d");
  const image = fieldPen.createImageData(FIELD, FIELD);

  // The pupil this moment, shining `shine` of its fullest.
  const drawLava = (lava, seconds, shine) => {
    const bodies = lava.map((one) => [
      one.across * Math.sin(one.rates[0] * seconds + one.phases[0]),
      one.down * Math.sin(one.rates[1] * seconds + one.phases[1]),
      (one.size * (1 + 0.1 * Math.sin(one.rates[2] * seconds + one.phases[1]))) ** 2,
    ]);
    const data = image.data;
    const DARK = rgbOf("--eye-dark");
    const SHINE = rgbOf("--eye-pupil-shine");
    const SHINE_EDGE = rgbOf("--eye-pupil-edge");
    for (let row = 0; row < FIELD; row += 1) {
      const v = ((row + 0.5) / FIELD) * 2 * REACH - REACH;
      for (let column = 0; column < FIELD; column += 1) {
        const u = ((column + 0.5) / FIELD) * 2 * REACH - REACH;
        let pull = 0;
        for (const [x, y, size] of bodies) {
          pull += size / ((u - x) ** 2 + (v - y) ** 2 + 1e-6);
        }
        const at = (row * FIELD + column) * 4;
        const glow = Math.max(0, Math.min(1, (pull - GLOWS[0]) / (GLOWS[1] - GLOWS[0]))) * shine;
        const out = smooth(glow);
        const deep = glow * glow;
        for (let part = 0; part < 3; part += 1) {
          data[at + part] = mix(mix(DARK[part], SHINE_EDGE[part], out), SHINE[part], deep);
        }
        data[at + 3] = 255 * Math.max(0, Math.min(1, (pull - 0.85) / 0.3));
      }
    }
    fieldPen.putImageData(image, 0, 0);
  };

  // The fire `seconds` into its loop, drawn onto a canvas of its own `side` pixels wide.
  const burn = (side, seconds) => {
    const canvas = document.createElement("canvas");
    canvas.width = side;
    canvas.height = side;
    const pen = canvas.getContext("2d");
    const r = side / 2;
    pen.setTransform(1, 0, 0, 1, r, r);
    pen.beginPath();
    pen.arc(0, 0, r, 0, Math.PI * 2);
    pen.clip();
    pen.fillStyle = glowOf(pen, r, stopsOf(GROUND));
    pen.fillRect(-r, -r, side, side);
    pen.globalCompositeOperation = "lighter";
    drawTorus(pen, filaments, r, seconds);
    pen.globalCompositeOperation = "source-over";
    const rim = pen.createRadialGradient(0, 0, r * 0.72, 0, 0, r);
    rim.addColorStop(0, withAlpha("--eye-iris-rim", 0));
    rim.addColorStop(1, cssOf("--eye-iris-rim"));
    pen.fillStyle = rim;
    pen.fillRect(-r, -r, side, side);
    return canvas;
  };

  const picture = { canvas: null, pupil: field };
  // Brings the picture to the moment `seconds` in, `across` pixels wide. The fire's pictures are
  // drawn again at a new size.
  picture.paint = (across, lava, seconds, shine) => {
    const side = Math.max(16, Math.ceil(across));
    if (side !== burnSide) {
      burns = [];
      burnSide = side;
    }
    const at = Math.floor((seconds / LOOP) * BURNS) % BURNS;
    burns[at] ??= burn(side, (at / BURNS) * LOOP);
    picture.canvas = burns[at];
    drawLava(lava, seconds, shine);
  };
  return picture;
}

// An iris on the sphere: the discs turned by the iris's own angle, then squeezed along the line out
// from the sphere's middle through it, `turn`, by `face`, how squarely the disc faces the reader, as
// a disc on a sphere is. The pupil lies over them shifted by the iris's `look`, inside the iris. The
// highlight on the cornea over it is not squeezed or turned, because a reflection stays where the
// light is.
export function drawIris(pen, picture, iris, x, y, r, turn, face) {
  pen.save();
  pen.translate(x, y);
  pen.rotate(turn);
  pen.scale(face, 1);
  pen.rotate(-turn);
  pen.save();
  pen.rotate(iris.phase);
  pen.drawImage(picture.canvas, -r, -r, r * 2, r * 2);
  pen.restore();
  pen.beginPath();
  pen.arc(0, 0, r * 0.97, 0, Math.PI * 2);
  pen.clip();
  pen.translate(iris.look[0] * r, iris.look[1] * r);
  pen.rotate(iris.phase);
  pen.drawImage(picture.pupil, -REACH * r, -REACH * r, 2 * REACH * r, 2 * REACH * r);
  pen.restore();

  const hx = x - r * 0.32 * face;
  const hy = y - r * 0.36;
  const shine = pen.createRadialGradient(hx, hy, 0, hx, hy, r * 0.11);
  shine.addColorStop(0, cssOf("--eye-gleam"));
  shine.addColorStop(1, withAlpha("--eye-gleam", 0));
  pen.fillStyle = shine;
  pen.beginPath();
  pen.arc(hx, hy, r * 0.11, 0, Math.PI * 2);
  pen.fill();
}
