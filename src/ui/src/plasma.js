// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The eye's plasma, drawn by the GPU. Everything in it is a list of triangles laid down in one draw
// a frame: each corner a place in the canvas's own pixels, where it lies across its shape and along
// it, and a color and a strength. The light of each point is the shape's color, strongest down its
// middle and gone at its edges, with a white-hot core along the middle, and fading out along it.
// Light adds to light, and the canvas lies over the eye with each point as clear as it is dark.
//
// A shape is drawn the way it is filled: `across` runs from -1 at one edge through 0 at the middle
// to 1 at the other, and `along` from 0, where the light is full, to 1, where it is gone.

// Each corner's numbers: x and y, across and along, red, green and blue from 0 to 1, and strength.
export const CORNER = 8;

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
varying vec2 at;
varying vec4 light;
void main() {
  float edge = max(0.0, 1.0 - at.x * at.x);
  float fade = pow(max(0.0, 1.0 - at.y), 1.4);
  float body = pow(edge, 1.6) * fade;
  float core = pow(edge, 12.0) * fade;
  vec3 color = (light.rgb * body + vec3(0.93, 0.92, 1.0) * core * 0.6) * light.a;
  gl_FragColor = vec4(color, max(color.r, max(color.g, color.b)));
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
// eye goes on without its plasma there.
export function plasmaOn(canvas) {
  if (drawers.has(canvas)) {
    return drawers.get(canvas);
  }
  const gl = canvas.getContext("webgl", { alpha: true, premultipliedAlpha: true, antialias: false, depth: false, stencil: false });
  if (!gl) {
    drawers.set(canvas, null);
    return null;
  }
  const program = gl.createProgram();
  gl.attachShader(program, compiled(gl, gl.VERTEX_SHADER, PLACE));
  gl.attachShader(program, compiled(gl, gl.FRAGMENT_SHADER, SHADE));
  gl.linkProgram(program);
  gl.useProgram(program);
  const buffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
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
  const room = gl.getUniformLocation(program, "room");
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.ONE, gl.ONE);
  gl.clearColor(0, 0, 0, 0);

  const drawer = {
    // Draws `count` corners of `corners`, three to a triangle, over a canvas `width` by `height` of
    // the page's pixels.
    draw(corners, count, width, height) {
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
