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
import { clock, idle, motionReduced, still as paused, whenMoving } from "./motion.js";
import { onScheme } from "./scheme.js";

// The spacing of the points, which lays eight times the points of the docs site's lattice in the same room,
// and how far the most disordered point goes from its place, in pixels.
const STEP = 22 / Math.sqrt(3) / Math.sqrt(8);
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

// The pens a point draws with, one a level of disorder, and the share of its ink each puts down. The
// shader blends the two pens either side of a point's disorder, so its shade is continuous; the 2D
// canvas draws each point with the nearest of LEVELS_DRAWN pens, which keeps a step of shade within
// a dot or two of the lattice as the order spreads.
const LEVELS = 24;
const LEVELS_DRAWN = 96;
const DIM = 0.85;

// The longest between two frames of a moving lattice, in milliseconds.
const FRAME = 66;

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

// The pen of each of `count` levels, made again when the scheme or a theme changes.
const pens = new Map();
onScheme(() => pens.clear());
function pensOf(count = LEVELS) {
  if (!pens.has(count)) {
    pens.set(
      count,
      Array.from({ length: count }, (_, level) => {
        const loose = level / (count - 1);
        const kept = 1 - loose;
        const [red, green, blue] = shade(loose);
        return { color: `rgba(${red}, ${green}, ${blue}, ${((0.414 + 0.184 * kept) * DIM).toFixed(3)})`, size: 0.59 + 0.16 * kept };
      }),
    );
  }
  return pens.get(count);
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

// The kinds of stage as the shader reads them: one side ordered at an angle, the order spreading
// from a front, the whole lattice at one disorder, and the order gathering from the top.
const SIDED = 0;
const SPREAD = 1;
const WHOLE = 2;
const GATHER = 3;

// Where the loop stands at `seconds`: the stage's kind, the angle of a side, the front of a spread,
// and the share of the stage done.
function stageAt(seconds) {
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
      return { kind: SIDED, angle: 0, front: 0, part };
    }
    if (name === "spread") {
      return { kind: SPREAD, angle: 0, front: ORDERED + (1 - ORDERED) * part, part };
    }
    if (name === "invert") {
      return { kind: WHOLE, angle: 0, front: 0, part };
    }
    if (name === "gather") {
      return { kind: GATHER, angle: Math.PI / 2, front: 0, part };
    }
    return { kind: SIDED, angle: (Math.PI / 2) * (1 + turns + part), front: 0, part };
  }
  return { kind: SIDED, angle: 0, front: 0, part: 0 };
}

// The loop at `seconds`: a way to read the disorder at any point.
function orderAt(seconds) {
  const { kind, angle, front, part } = stageAt(seconds);
  if (kind === SPREAD) {
    return (u) => clamp((u - front) / RAMP);
  }
  if (kind === WHOLE) {
    return () => part;
  }
  if (kind === GATHER) {
    return (u, v) => Math.max(sided(angle, u, v), 1 - part);
  }
  return (u, v) => sided(angle, u, v);
}

// Each lattice's points, four floats a point in one Float32Array the GPU takes as it is: place across,
// place down, and the way and share of the reach it moves.
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
  // The points stand half a STEP in from every edge, and the spacing across and down each stretches
  // or shrinks from STEP by the least that lands the last point there.
  const spaced = (length) => {
    const gaps = Math.max(1, Math.round((length - STEP) / STEP));
    return { gaps, step: (length - STEP) / gaps };
  };
  const across = spaced(box.width);
  const down = spaced(box.height);
  const points = new Float32Array((down.gaps + 1) * (across.gaps + 1) * 4);
  let at = 0;
  for (let row = 0; row <= down.gaps; row += 1) {
    const y = STEP / 2 + row * down.step;
    for (let col = 0; col <= across.gaps; col += 1) {
      const turn = draw() * Math.PI * 2;
      const reach = draw();
      points[at] = STEP / 2 + col * across.step;
      points[at + 1] = y;
      points[at + 2] = Math.cos(turn) * reach;
      points[at + 3] = Math.sin(turn) * reach;
      at += 4;
    }
  }
  const laid = { width: box.width, height: box.height, scale, points };
  lattices.set(canvas, laid);
  return laid;
}

// The lattice drawn through WebGL: each point one vertex, its place and its way off it handed over
// once as the lattice is laid out, and each frame only where the loop stands. The vertex shader reads
// the disorder at the point as orderAt does, moves the point by its share of the reach, and takes its
// level's pen, a round dot. A pixel's share of a dot is read in one step from how far the pixel's middle
// stands from the dot's edge, a pixel wide ramp from all of it to none, so each pixel a dot touches costs
// one length and one clamp; the radius is taken at FITS of the pen's, which puts down the ink the page's
// own canvas puts down for the same dots. The page's own work a frame is a handful of numbers and one draw.
const FITS = 0.955;
const PI = Math.PI.toFixed(8);
const VERTEX = `
attribute vec4 point;
uniform vec2 size;
uniform float scale;
uniform float stage;
uniform float angle;
uniform float front;
uniform float part;
uniform vec4 pens[${LEVELS}];
uniform float radii[${LEVELS}];
varying vec4 color;
varying vec2 center;
varying float radius;
float sided(float turn, float u, float v) {
  float dx = cos(turn);
  float dy = sin(turn);
  float along = 0.5 + ((u - 0.5) * dx + (v - 0.5) * dy) / (abs(dx) + abs(dy));
  return clamp((along - ${ORDERED.toFixed(4)}) / ${RAMP.toFixed(4)}, 0.0, 1.0);
}
void main() {
  float u = point.x / size.x;
  float v = point.y / size.y;
  float loose;
  if (stage == ${SPREAD.toFixed(1)}) {
    loose = clamp((u - front) / ${RAMP.toFixed(4)}, 0.0, 1.0);
  } else if (stage == ${WHOLE.toFixed(1)}) {
    loose = part;
  } else if (stage == ${GATHER.toFixed(1)}) {
    loose = max(sided(${PI} / 2.0, u, v), 1.0 - part);
  } else {
    loose = sided(angle, u, v);
  }
  vec2 placed = point.xy + point.zw * (loose * loose * ${REACH.toFixed(4)});
  float level = loose * ${(LEVELS - 1).toFixed(1)};
  int low = int(floor(min(level, ${(LEVELS - 2).toFixed(1)})));
  float between = level - float(low);
  color = mix(pens[low], pens[low + 1], between);
  radius = mix(radii[low], radii[low + 1], between) * scale * ${FITS};
  center = vec2(placed.x, size.y - placed.y) * scale;
  gl_Position = vec4(placed.x / size.x * 2.0 - 1.0, 1.0 - placed.y / size.y * 2.0, 0.0, 1.0);
  gl_PointSize = ceil(radius * 2.0) + 2.0;
}`;
const FRAGMENT = `
precision mediump float;
varying vec4 color;
varying vec2 center;
varying float radius;
void main() {
  float inside = clamp(radius - length(gl_FragCoord.xy - center) + 0.5, 0.0, 1.0);
  float alpha = color.a * inside;
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
        stage: gl.getUniformLocation(program, "stage"),
        angle: gl.getUniformLocation(program, "angle"),
        front: gl.getUniformLocation(program, "front"),
        part: gl.getUniformLocation(program, "part"),
        pens: gl.getUniformLocation(program, "pens"),
        radii: gl.getUniformLocation(program, "radii"),
        pensFor: null,
        laidFor: null,
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

// Draws a lattice through WebGL at the loop's stage `at`, its points handed over again only where it
// was laid out anew.
function paintGl(painter, laid, at) {
  const { gl } = painter;
  const { width, height, scale, points } = laid;
  gl.bindBuffer(gl.ARRAY_BUFFER, painter.buffer);
  if (painter.laidFor !== laid) {
    gl.bufferData(gl.ARRAY_BUFFER, points, gl.STATIC_DRAW);
    painter.laidFor = laid;
  }
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
  gl.uniform1f(painter.stage, at.kind);
  gl.uniform1f(painter.angle, at.angle);
  gl.uniform1f(painter.front, at.front);
  gl.uniform1f(painter.part, at.part);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
  gl.enableVertexAttribArray(painter.point);
  gl.vertexAttribPointer(painter.point, 4, gl.FLOAT, false, 0, 0);
  gl.drawArrays(gl.POINTS, 0, points.length / 4);
}

function paint(canvas, laid, seconds) {
  const painter = painterOf(canvas);
  if (painter) {
    paintGl(painter, laid, stageAt(seconds));
    return;
  }
  const { width, height, scale, points } = laid;
  const order = orderAt(seconds);
  const levels = Array.from({ length: LEVELS_DRAWN }, () => []);
  for (let at = 0; at < points.length; at += 4) {
    const x = points[at];
    const y = points[at + 1];
    const loose = order(x / width, y / height);
    const shift = loose * loose * REACH;
    levels[Math.round(loose * (LEVELS_DRAWN - 1))].push(x + points[at + 2] * shift, y + points[at + 3] * shift);
  }
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  pen.clearRect(0, 0, width, height);
  levels.forEach((placed, level) => {
    if (!placed.length) {
      return;
    }
    const { color, size } = pensOf(LEVELS_DRAWN)[level];
    pen.fillStyle = color;
    pen.beginPath();
    for (let at = 0; at < placed.length; at += 2) {
      pen.moveTo(placed[at] + size, placed[at + 1]);
      pen.arc(placed[at], placed[at + 1], size, 0, Math.PI * 2);
    }
    pen.fill();
  });
}

const still = () => motionReduced();

export function drawLattice(canvas) {
  const laid = layOut(canvas);
  if (laid) {
    laid.drawnAt = still() ? 0 : latticeSeconds();
    paint(canvas, laid, laid.drawnAt);
  }
}

// The places across and down, each from 0 to 1, where the loop's change is read to judge a frame, laid
// once into typed arrays, and the disorder at each at the two moments compared.
const PROBES = 24;
const PROBE_COUNT = (PROBES + 1) * (PROBES + 1);
const PROBE_U = new Float32Array(PROBE_COUNT);
const PROBE_V = new Float32Array(PROBE_COUNT);
for (let across = 0, at = 0; across <= PROBES; across += 1) {
  for (let down = 0; down <= PROBES; down += 1, at += 1) {
    PROBE_U[at] = across / PROBES;
    PROBE_V[at] = down / PROBES;
  }
}
const looseThen = new Float32Array(PROBE_COUNT);
const looseNow = new Float32Array(PROBE_COUNT);

// The disorder at every probe at `seconds` into `out`, one flat loop over the typed arrays with the
// stage's numbers read once, as the vertex shader reads them at each point.
function loosenessAt(seconds, out) {
  const { kind, angle, front, part } = stageAt(seconds);
  if (kind === WHOLE) {
    out.fill(part);
    return;
  }
  const dx = Math.cos(angle);
  const dy = Math.sin(angle);
  const across = 1 / (Math.abs(dx) + Math.abs(dy));
  for (let at = 0; at < PROBE_COUNT; at += 1) {
    if (kind === SPREAD) {
      out[at] = clamp((PROBE_U[at] - front) / RAMP);
      continue;
    }
    const along = 0.5 + ((PROBE_U[at] - 0.5) * dx + (PROBE_V[at] - 0.5) * dy) * across;
    const loose = clamp((along - ORDERED) / RAMP);
    out[at] = kind === GATHER ? Math.max(loose, 1 - part) : loose;
  }
}

// The least a point moves, in pixels of the page, between one drawn frame and the next. The points are
// drawn soft-edged, so a move of a quarter pixel shows as a shift of shade, and the slow ends of each
// eased stage glide rather than step: a whole pixel there would be drawn as one jump after a long
// still.
const STEP_SHOWN = 0.25;

// Whether the loop moved, from `then` to `now` in seconds, enough to be seen: some point moved by
// STEP_SHOWN. No one follows the lattice's points one by one, and a frame that moves none of them
// that far shows nothing a reader could see.
export function shows(then, now) {
  loosenessAt(then, looseThen);
  loosenessAt(now, looseNow);
  for (let at = 0; at < PROBE_COUNT; at += 1) {
    const was = looseThen[at];
    const is = looseNow[at];
    if (Math.abs(is * is - was * was) * REACH >= STEP_SHOWN) {
      return true;
    }
  }
  return false;
}

// The lattice's own time: the motion clock less every stretch the window held the lattice off, so the
// loop pauses while the reader works in the window and takes up where it stood, rather than running on
// unseen and jumping to catch up. `since` is when the stretch now holding it began, null where none is.
const held = { lost: 0, since: null };
const latticeSeconds = (now = performance.now()) => (clock(now) - held.lost) / 1000;

// Moves every lattice in sight a frame, no faster than one each FRAME, only in a frame nothing else
// wants, and only where the frame would show: the lattices are the window's lowest work, and their
// frame redraws the whole window.
let last = 0;
function tick(now) {
  if (paused()) {
    whenMoving(() => requestAnimationFrame(tick));
    return;
  }
  requestAnimationFrame(tick);
  const free = idle(now);
  if (!free || document.hidden || still()) {
    held.since ??= now;
    return;
  }
  if (held.since !== null) {
    held.lost += now - held.since;
    held.since = null;
  }
  if (now - last < FRAME) {
    return;
  }
  last = now;
  const seconds = latticeSeconds(now);
  for (const [canvas, laid] of lattices) {
    if (!canvas.isConnected) {
      lattices.delete(canvas);
    } else if (canvas.offsetParent !== null && (laid.recolored || laid.drawnAt === undefined || shows(laid.drawnAt, seconds))) {
      laid.drawnAt = seconds;
      laid.recolored = false;
      paint(canvas, laid, seconds);
    }
  }
}
requestAnimationFrame(tick);

// A scheme or a theme changed, every lattice is drawn again in its colors at the next frame, moving
// or not.
onScheme(() => lattices.forEach((laid) => (laid.recolored = true)));

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
