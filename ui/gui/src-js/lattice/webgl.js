// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's WebGL as the lattice's GL driver (ui/gui/src-wasm/lattice_gl.c) calls it: each slot's canvas its own
// context, and every shader, program, buffer and uniform the driver makes kept by number. The driver does the
// drawing; this hands each of its calls to WebGL, reading what it passes from the module's memory.

// Each slot's context, the objects made on it, by number, the first standing for none, and the bytes each buffer holds.
const contexts = new Map();

// The bytes of one dot as the driver lays it (lattice.h's LatticeDot): its place across and down and its radius, three
// floats, then its color, red, green, blue and ink, a byte each.
const DOT = 16;
const RADIUS_AT = 8;
const SHADE_AT = 12;

// The WebGL context of `canvas`, held in `slot`, asked for once, or null where the page has no WebGL. A canvas that
// has drawn through WebGL keeps to it, and one that has not keeps to its 2D canvas.
const asked = new WeakMap();
export function contextOf(slot, canvas) {
  if (!asked.has(canvas)) {
    asked.set(canvas, canvas.getContext("webgl", { antialias: false, alpha: true, premultipliedAlpha: true }));
  }
  const gl = asked.get(canvas);
  if (gl && contexts.get(slot)?.gl !== gl) {
    contexts.set(slot, { gl, objects: [null], held: new Map() });
  }
  return gl;
}

// A slot let go with its canvas, its context and objects with it.
export function forgetContext(slot) {
  contexts.delete(slot);
}

// The calls the driver makes, each on its slot's context, reading what the driver hands over from `memory()`, the
// module's memory as it stands at the call.
export function glCalls(memory) {
  const decoder = new TextDecoder();
  const text = (at, length) => decoder.decode(new Uint8Array(memory(), at, length));
  const on = (slot) => contexts.get(slot);
  const kept = (slot, made) => on(slot).objects.push(made) - 1;
  const object = (slot, id) => on(slot).objects[id];
  return {
    gl_shader: (slot, vertex, source, length) => {
      const { gl } = on(slot);
      const shader = gl.createShader(vertex ? gl.VERTEX_SHADER : gl.FRAGMENT_SHADER);
      gl.shaderSource(shader, text(source, length));
      gl.compileShader(shader);
      return kept(slot, shader);
    },
    gl_program: (slot, vertex, fragment) => {
      const { gl } = on(slot);
      const program = gl.createProgram();
      gl.attachShader(program, object(slot, vertex));
      gl.attachShader(program, object(slot, fragment));
      gl.linkProgram(program);
      return gl.getProgramParameter(program, gl.LINK_STATUS) ? kept(slot, program) : 0;
    },
    gl_buffer: (slot) => kept(slot, on(slot).gl.createBuffer()),
    gl_attribute: (slot, program, name, length) => on(slot).gl.getAttribLocation(object(slot, program), text(name, length)),
    gl_uniform: (slot, program, name, length) => kept(slot, on(slot).gl.getUniformLocation(object(slot, program), text(name, length))),
    // the dots written over the buffer's own where it holds as many bytes, else into a buffer made anew for them
    gl_points: (slot, buffer, dots, bytes) => {
      const { gl, held } = on(slot);
      const view = new Uint8Array(memory(), dots, bytes);
      gl.bindBuffer(gl.ARRAY_BUFFER, object(slot, buffer));
      if (held.get(buffer) === bytes) {
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, view);
      } else {
        gl.bufferData(gl.ARRAY_BUFFER, view, gl.DYNAMIC_DRAW);
        held.set(buffer, bytes);
      }
    },
    gl_begin: (slot, program, buffer) => {
      const { gl } = on(slot);
      gl.bindBuffer(gl.ARRAY_BUFFER, object(slot, buffer));
      gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT);
      gl.useProgram(object(slot, program));
    },
    gl_uniform2f: (slot, uniform, first, second) => on(slot).gl.uniform2f(object(slot, uniform), first, second),
    gl_uniform1f: (slot, uniform, value) => on(slot).gl.uniform1f(object(slot, uniform), value),
    gl_draw: (slot, place, radius, shade, count) => {
      const { gl } = on(slot);
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
      gl.enableVertexAttribArray(place);
      gl.vertexAttribPointer(place, 2, gl.FLOAT, false, DOT, 0);
      gl.enableVertexAttribArray(radius);
      gl.vertexAttribPointer(radius, 1, gl.FLOAT, false, DOT, RADIUS_AT);
      gl.enableVertexAttribArray(shade);
      gl.vertexAttribPointer(shade, 4, gl.UNSIGNED_BYTE, true, DOT, SHADE_AT);
      gl.drawArrays(gl.POINTS, 0, count);
    },
  };
}
