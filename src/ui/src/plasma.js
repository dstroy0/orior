// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's plasma, drawn by the GPU. Everything in it is a list of triangles laid down in one draw
// a frame: each corner a place in the canvas's own pixels, where it lies across its shape and along
// it, and a color and a strength. The light of each point is the shape's color, strongest down its
// middle and gone at its edges, with a pale core along the middle, and fading out along it. Light
// adds to light. Each point covers what is under it SHADOW times as much as it lights it, which
// leaves the plasma a smoke that darkens the page behind it as it glows.
//
// A shape is drawn the way it is filled: `across` runs from -1 at one edge through 0 at the middle
// to 1 at the other, and `along` from 0, where the light is full, to 1, where it is gone.

import { rgbOf } from "./colors.js";

// Each corner's numbers: x and y, across and along, red, green and blue from 0 to 1, and strength.
export const CORNER = 8;

// The plasma's purples, from the shadow at its edges to the light inside it, each as [red, green,
// blue] and read from the stylesheet's --plasma-dusk, --plasma-plum and --plasma-orchid.
export const dusk = () => rgbOf("--plasma-dusk");
export const plum = () => rgbOf("--plasma-plum");
export const orchid = () => rgbOf("--plasma-orchid");

// The triangles of a frame's plasma, written corner by corner into one list that is kept from frame
// to frame and grows when it fills. A corner is [x, y, across, along, color, strength].
export function cornersOf() {
  let list = new Float32Array(4096 * CORNER);
  let count = 0;
  const add = ([x, y, across, along, color, strength]) => {
    if ((count + 1) * CORNER > list.length) {
      const bigger = new Float32Array(list.length * 2);
      bigger.set(list);
      list = bigger;
    }
    const at = count * CORNER;
    list[at] = x;
    list[at + 1] = y;
    list[at + 2] = across;
    list[at + 3] = along;
    list[at + 4] = color[0] / 255;
    list[at + 5] = color[1] / 255;
    list[at + 6] = color[2] / 255;
    list[at + 7] = strength;
    count += 1;
  };
  return {
    clear: () => (count = 0),
    // A piece with four sides as two triangles, its corners given in order round it.
    four(one, two, three, four) {
      for (const corner of [one, two, three, one, three, four]) {
        add(corner);
      }
    },
    add,
    // Moves every corner written so far by dx across and dy down.
    shift(dx, dy) {
      for (let at = 0; at < count * CORNER; at += CORNER) {
        list[at] += dx;
        list[at + 1] += dy;
      }
    },
    get list() {
      return list;
    },
    get count() {
      return count;
    },
  };
}

// An arc as a thin piece along each of its lines.
export function addArc(corners, points, half, color, strength) {
  for (let at = 0; at + 1 < points.length; at += 1) {
    const [x0, y0] = points[at];
    const [x1, y1] = points[at + 1];
    const length = Math.hypot(x1 - x0, y1 - y0) || 1;
    const side = [(-(y1 - y0) / length) * half, ((x1 - x0) / length) * half];
    corners.four([x0 - side[0], y0 - side[1], -1, 0.1, color, strength], [x0 + side[0], y0 + side[1], 1, 0.1, color, strength], [x1 + side[0], y1 + side[1], 1, 0.1, color, strength], [x1 - side[0], y1 - side[1], -1, 0.1, color, strength]);
  }
}

// A soft round light of `radius` about x, y, as a fan of triangles from it.
export function addSpot(corners, x, y, radius, color, strength) {
  const middle = [x, y, 0, 0, color, strength];
  const rim = (at) => {
    const turn = (at / 16) * Math.PI * 2;
    return [x + Math.cos(turn) * radius, y + Math.sin(turn) * radius, 1, 0, color, strength];
  };
  for (let at = 0; at < 16; at += 1) {
    corners.add(middle);
    corners.add(rim(at));
    corners.add(rim(at + 1));
  }
}

const PLACE = `
attribute vec2 place;
attribute vec2 shape;
attribute vec4 tint;
uniform vec2 room;
varying vec2 at;
varying vec4 light;
void main() {
  at = shape;
  light = tint;
  gl_Position = vec4(place.x / room.x * 2.0 - 1.0, 1.0 - place.y / room.y * 2.0, 0.0, 1.0);
}`;

const SHADE = `
precision mediump float;
const float SHADOW = 3.0;
varying vec2 at;
varying vec4 light;
void main() {
  float edge = max(0.0, 1.0 - at.x * at.x);
  float fade = pow(max(0.0, 1.0 - at.y), 1.4);
  float body = pow(edge, 1.6) * fade;
  float core = pow(edge, 12.0) * fade;
  vec3 color = (light.rgb * body + vec3(0.78, 0.64, 1.0) * core * 0.35) * light.a;
  float lit = max(color.r, max(color.g, color.b));
  gl_FragColor = vec4(color, min(1.0, lit * SHADOW));
}`;

const drawers = new WeakMap();

function compiled(gl, kind, source) {
  const shader = gl.createShader(kind);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    throw new Error(gl.getShaderInfoLog(shader) ?? "a plasma shader did not compile");
  }
  return shader;
}

// The drawer for a canvas, made once and kept with it, or null where the canvas has no WebGL. The
// eye and the fuse go on without their plasma there.
//
// The GPU can take a context back, when its driver restarts or the page holds too many. The drawer
// then draws nothing, asks for the context back, and sets its program up again when it comes.
export function plasmaOn(canvas) {
  if (drawers.has(canvas)) {
    return drawers.get(canvas);
  }
  const gl = canvas.getContext("webgl", { alpha: true, premultipliedAlpha: true, antialias: false, depth: false, stencil: false, preserveDrawingBuffer: true });
  if (!gl) {
    drawers.set(canvas, null);
    return null;
  }
  let room = null;
  const setUp = () => {
    const program = gl.createProgram();
    gl.attachShader(program, compiled(gl, gl.VERTEX_SHADER, PLACE));
    gl.attachShader(program, compiled(gl, gl.FRAGMENT_SHADER, SHADE));
    gl.linkProgram(program);
    gl.useProgram(program);
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    const bytes = CORNER * 4;
    for (const [name, count, offset] of [
      ["place", 2, 0],
      ["shape", 2, 2],
      ["tint", 4, 4],
    ]) {
      const at = gl.getAttribLocation(program, name);
      gl.enableVertexAttribArray(at);
      gl.vertexAttribPointer(at, count, gl.FLOAT, false, bytes, offset * 4);
    }
    room = gl.getUniformLocation(program, "room");
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    gl.clearColor(0, 0, 0, 0);
  };
  setUp();
  canvas.addEventListener("webglcontextlost", (event) => event.preventDefault());
  canvas.addEventListener("webglcontextrestored", setUp);

  const drawer = {
    // Draws `count` corners of `corners`, three to a triangle, over a canvas `width` by `height` of
    // the page's pixels.
    draw(corners, count, width, height) {
      if (gl.isContextLost()) {
        return;
      }
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.clear(gl.COLOR_BUFFER_BIT);
      if (count === 0) {
        return;
      }
      gl.uniform2f(room, width, height);
      gl.bufferData(gl.ARRAY_BUFFER, corners.subarray(0, count * CORNER), gl.STREAM_DRAW);
      gl.drawArrays(gl.TRIANGLES, 0, count);
    },
  };
  drawers.set(canvas, drawer);
  return drawer;
}
