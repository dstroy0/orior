// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice the docs site draws behind its hero: a grid of points, each moved off its place
// by an amount that is zero where the lattice is ordered and grows where it is not. A point in place
// is signal green, and a moved one passes through the link blue to violet. Each point's way off its
// place is seeded and the same every time; how far it goes is the lattice's order where it stands.
//
// The order moves, slowly, through a loop. From ordered at the left and disordered at the right, the
// order spreads left to right until the whole lattice is in place, then it inverts and the whole
// lattice comes apart. The order comes back with the top ordered and the bottom not, and the ordered
// side turns: to the right, to the bottom, and to the left, where the loop began. Every lattice on the
// page stands at the same point of the loop. Where the reader asks for less motion the lattice holds
// still, ordered at the left.

import { rgbOf } from "./colors.js";
import { clock, still as paused, whenMoving } from "./motion.js";
import { onScheme } from "./scheme.js";

// The spacing of the points, and how far the most disordered point goes from its place, in pixels.
const STEP = 22 / Math.sqrt(3);
const REACH = 22 * 2.4;
const SEED = 1729;
// The colors from ordered to disordered, the stylesheet's --lattice-1 to --lattice-3.
const STOPS = ["--lattice-1", "--lattice-2", "--lattice-3"];

// The loop's stages, each its length in seconds.
const STAGES = [
  ["rest", 4],
  ["spread", 10],
  ["invert", 5],
  ["gather", 7],
  ["turn", 8],
  ["turn", 8],
  ["turn", 8],
];
const LOOP = STAGES.reduce((sum, [, seconds]) => sum + seconds, 0);

// Where one side is ordered: the share of the lattice before its points start to move, and the share
// over which they go from in place to as far as they go. A point moves by the square of how far along
// that share it stands, and stays within three pixels of its place to about 0.23 of the lattice, which
// reads as ordered against the 0.77 that does not: about 0.3 to 1.
const ORDERED = 0.1;
const RAMP = 0.54;

// The shades a point takes, one a level of disorder.
const LEVELS = 24;

// The longest between two frames of a moving lattice, in milliseconds.
const FRAME = 33;

function random(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let mixed = state;
    mixed = Math.imul(mixed ^ (mixed >>> 15), mixed | 1);
    mixed ^= mixed + Math.imul(mixed ^ (mixed >>> 7), mixed | 61);
    return ((mixed ^ (mixed >>> 14)) >>> 0) / 4294967296;
  };
}

function shade(loose) {
  const along = loose * (STOPS.length - 1);
  const low = Math.min(Math.floor(along), STOPS.length - 2);
  const part = along - low;
  const to = rgbOf(STOPS[low + 1]);
  return rgbOf(STOPS[low]).map((from, at) => Math.round(from + (to[at] - from) * part));
}

// The pen of each level, made again when the scheme or a theme changes.
let pens = null;
onScheme(() => {
  pens = null;
});
function pensOf() {
  pens ??= Array.from({ length: LEVELS }, (_, level) => {
    const loose = level / (LEVELS - 1);
    const kept = 1 - loose;
    const [red, green, blue] = shade(loose);
    return { color: `rgba(${red}, ${green}, ${blue}, ${(0.414 + 0.184 * kept).toFixed(3)})`, size: 0.59 + 0.16 * kept };
  });
  return pens;
}

const clamp = (value) => Math.max(0, Math.min(1, value));
const ease = (part) => part * part * (3 - 2 * part);

// The disorder where a point stands, with the ordered side away from `angle`: 0 orders the left,
// a quarter turn the top, a half turn the right, three quarters the bottom. `u` and `v` are the
// point's place across and down, each from 0 to 1.
function sided(angle, u, v) {
  const dx = Math.cos(angle);
  const dy = Math.sin(angle);
  const along = 0.5 + ((u - 0.5) * dx + (v - 0.5) * dy) / (Math.abs(dx) + Math.abs(dy));
  return clamp((along - ORDERED) / RAMP);
}

// The loop at `seconds`: a way to read the disorder at any point.
function orderAt(seconds) {
  let left = seconds % LOOP;
  let turns = 0;
  for (const [name, length] of STAGES) {
    if (left >= length) {
      left -= length;
      turns += name === "turn" ? 1 : 0;
      continue;
    }
    const part = ease(left / length);
    if (name === "rest") {
      return (u, v) => sided(0, u, v);
    }
    if (name === "spread") {
      const front = ORDERED + (1 - ORDERED) * part;
      return (u) => clamp((u - front) / RAMP);
    }
    if (name === "invert") {
      return () => part;
    }
    if (name === "gather") {
      return (u, v) => Math.max(sided(Math.PI / 2, u, v), 1 - part);
    }
    const angle = (Math.PI / 2) * (1 + turns + part);
    return (u, v) => sided(angle, u, v);
  }
  return (u, v) => sided(0, u, v);
}

// Each lattice's points: place across, place down, and the way and share of the reach it moves.
const lattices = new Map();

function layOut(canvas) {
  const box = canvas.getBoundingClientRect();
  if (!box.width || !box.height) {
    return null;
  }
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(box.width * scale);
  canvas.height = Math.round(box.height * scale);
  const draw = random(SEED);
  const points = [];
  // The points stand half a STEP in from every edge, and the spacing across and down each stretches
  // or shrinks from STEP by the least that lands the last point there.
  const spaced = (length) => {
    const gaps = Math.max(1, Math.round((length - STEP) / STEP));
    return { gaps, step: (length - STEP) / gaps };
  };
  const across = spaced(box.width);
  const down = spaced(box.height);
  for (let row = 0; row <= down.gaps; row += 1) {
    const y = STEP / 2 + row * down.step;
    for (let col = 0; col <= across.gaps; col += 1) {
      const x = STEP / 2 + col * across.step;
      const turn = draw() * Math.PI * 2;
      const reach = draw();
      points.push(x, y, Math.cos(turn) * reach, Math.sin(turn) * reach);
    }
  }
  const laid = { width: box.width, height: box.height, scale, points };
  lattices.set(canvas, laid);
  return laid;
}

// The lattice drawn through WebGL: each point one vertex, its place and its level, a round dot of
// its level's pen. A pixel's share of a dot is counted at sixteen places across the pixel, and the
// radius taken at FITS of the pen's, which puts down the ink the page's own canvas puts down for the
// same dots: a dot under a pixel wide stands as large and as soft as it did there. The points go in
// level by level, as the canvas laid its levels one over another, and all of them are drawn at once:
// the page hands the frame on without the thousands of shapes a canvas path would carry.
const FITS = 0.955;
const VERTEX = `
attribute vec3 point;
uniform vec2 size;
uniform float scale;
uniform vec4 pens[${LEVELS}];
uniform float radii[${LEVELS}];
varying vec4 color;
varying vec2 center;
varying float radius;
void main() {
  int level = int(point.z);
  color = pens[level];
  radius = radii[level] * scale * ${FITS};
  center = vec2(point.x, size.y - point.y) * scale;
  gl_Position = vec4(point.x / size.x * 2.0 - 1.0, 1.0 - point.y / size.y * 2.0, 0.0, 1.0);
  gl_PointSize = ceil(radius * 2.0) + 2.0;
}`;
const FRAGMENT = `
precision mediump float;
varying vec4 color;
varying vec2 center;
varying float radius;
void main() {
  float inside = 0.0;
  for (int across = 0; across < 4; across++) {
    for (int down = 0; down < 4; down++) {
      vec2 at = gl_FragCoord.xy - 0.5 + (vec2(float(across), float(down)) + 0.5) / 4.0;
      inside += step(length(at - center), radius);
    }
  }
  float alpha = color.a * inside / 16.0;
  gl_FragColor = vec4(color.rgb * alpha, alpha);
}`;

const painters = new WeakMap();

// A canvas's WebGL painter, made the first time it is drawn, or null where the page has no WebGL.
// A canvas that has drawn through WebGL keeps to it, and one that has not keeps to its 2D canvas.
function painterOf(canvas) {
  if (painters.has(canvas)) {
    return painters.get(canvas);
  }
  const gl = canvas.getContext("webgl", { antialias: false, alpha: true, premultipliedAlpha: true });
  let painter = null;
  if (gl) {
    const shader = (kind, source) => {
      const made = gl.createShader(kind);
      gl.shaderSource(made, source);
      gl.compileShader(made);
      return made;
    };
    const program = gl.createProgram();
    gl.attachShader(program, shader(gl.VERTEX_SHADER, VERTEX));
    gl.attachShader(program, shader(gl.FRAGMENT_SHADER, FRAGMENT));
    gl.linkProgram(program);
    if (gl.getProgramParameter(program, gl.LINK_STATUS)) {
      painter = {
        gl,
        program,
        buffer: gl.createBuffer(),
        point: gl.getAttribLocation(program, "point"),
        size: gl.getUniformLocation(program, "size"),
        scale: gl.getUniformLocation(program, "scale"),
        pens: gl.getUniformLocation(program, "pens"),
        radii: gl.getUniformLocation(program, "radii"),
        pensFor: null,
        placed: new Float32Array(0),
      };
    }
  }
  painters.set(canvas, painter);
  return painter;
}

// Every pen as WebGL takes them: the colors, red, green, blue and alpha each from 0 to 1, and the
// radii.
let glPens = null;
onScheme(() => {
  glPens = null;
});
function glPensOf() {
  glPens ??= (() => {
    const colors = new Float32Array(LEVELS * 4);
    const radii = new Float32Array(LEVELS);
    pensOf().forEach(({ color, size }, level) => {
      const [red, green, blue, alpha] = color.match(/[\d.]+/g).map(Number);
      colors.set([red / 255, green / 255, blue / 255, alpha], level * 4);
      radii[level] = size;
    });
    return { colors, radii };
  })();
  return glPens;
}

function paintGl(painter, laid, levels) {
  const { gl } = painter;
  const { width, height, scale } = laid;
  let count = 0;
  for (const placed of levels) {
    count += placed.length / 2;
  }
  if (painter.placed.length < count * 3) {
    painter.placed = new Float32Array(count * 3);
  }
  let at = 0;
  levels.forEach((placed, level) => {
    for (let one = 0; one < placed.length; one += 2) {
      painter.placed[at] = placed[one];
      painter.placed[at + 1] = placed[one + 1];
      painter.placed[at + 2] = level;
      at += 3;
    }
  });
  gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
  gl.clearColor(0, 0, 0, 0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  gl.useProgram(painter.program);
  const pens = glPensOf();
  if (painter.pensFor !== pens) {
    gl.uniform4fv(painter.pens, pens.colors);
    gl.uniform1fv(painter.radii, pens.radii);
    painter.pensFor = pens;
  }
  gl.uniform2f(painter.size, width, height);
  gl.uniform1f(painter.scale, scale);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  gl.bindBuffer(gl.ARRAY_BUFFER, painter.buffer);
  gl.bufferData(gl.ARRAY_BUFFER, painter.placed.subarray(0, count * 3), gl.DYNAMIC_DRAW);
  gl.enableVertexAttribArray(painter.point);
  gl.vertexAttribPointer(painter.point, 3, gl.FLOAT, false, 0, 0);
  gl.drawArrays(gl.POINTS, 0, count);
}

function paint(canvas, laid, seconds) {
  const { width, height, scale, points } = laid;
  const order = orderAt(seconds);
  const levels = Array.from({ length: LEVELS }, () => []);
  for (let at = 0; at < points.length; at += 4) {
    const x = points[at];
    const y = points[at + 1];
    const loose = order(x / width, y / height);
    const shift = loose * loose * REACH;
    levels[Math.round(loose * (LEVELS - 1))].push(x + points[at + 2] * shift, y + points[at + 3] * shift);
  }
  const painter = painterOf(canvas);
  if (painter) {
    paintGl(painter, laid, levels);
    return;
  }
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  pen.clearRect(0, 0, width, height);
  levels.forEach((placed, level) => {
    if (!placed.length) {
      return;
    }
    const { color, size } = pensOf()[level];
    pen.fillStyle = color;
    pen.beginPath();
    for (let at = 0; at < placed.length; at += 2) {
      pen.moveTo(placed[at] + size, placed[at + 1]);
      pen.arc(placed[at], placed[at + 1], size, 0, Math.PI * 2);
    }
    pen.fill();
  });
}

const still = () => matchMedia("(prefers-reduced-motion: reduce)").matches;

export function drawLattice(canvas) {
  const laid = layOut(canvas);
  if (laid) {
    paint(canvas, laid, still() ? 0 : clock() / 1000);
  }
}

// Moves every lattice in sight a frame, no faster than one each FRAME.
let last = 0;
function tick(now) {
  if (paused()) {
    whenMoving(() => requestAnimationFrame(tick));
    return;
  }
  requestAnimationFrame(tick);
  if (now - last < FRAME || document.hidden || still()) {
    return;
  }
  last = now;
  for (const [canvas, laid] of lattices) {
    if (!canvas.isConnected) {
      lattices.delete(canvas);
    } else if (canvas.offsetParent !== null) {
      paint(canvas, laid, clock(now) / 1000);
    }
  }
}
requestAnimationFrame(tick);

// Lays a lattice out again each time its size changes, which a lattice in a hidden view also does
// when the view is shown. One taken off the page is let go.
const sized = new ResizeObserver((entries) =>
  entries.forEach((entry) => (entry.target.isConnected ? drawLattice(entry.target) : sized.unobserve(entry.target))),
);

export function keepLattice(canvas) {
  sized.observe(canvas);
}

// Keeps every lattice on the page drawn.
export function keepLattices() {
  document.querySelectorAll("canvas.lattice").forEach(keepLattice);
}
