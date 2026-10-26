// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's WebGL as the lattice's GL driver calls it: each slot's canvas its own context, asked for once, and each
// shader, program and buffer the driver makes kept by number, its text read from the module's memory.

import { contextOf, forgetContext, glCalls } from "../../../gui/src-js/lattice/webgl.js";

const { test, assert } = window.__harness;

const SLOT = 6;
const VERTEX = "attribute vec4 point; void main() { gl_Position = vec4(point.xy, 0.0, 1.0); gl_PointSize = 1.0; }";
const FRAGMENT = "precision mediump float; void main() { gl_FragColor = vec4(1.0); }";

test("a canvas's context is asked for once and kept, and a canvas that drew in 2D keeps to it", () => {
  const canvas = Object.assign(document.createElement("canvas"), { width: 8, height: 8 });
  const gl = contextOf(SLOT, canvas);
  assert.ok(gl, "WebGL to draw with");
  assert.equal(contextOf(SLOT, canvas), gl);
  const flat = document.createElement("canvas");
  flat.getContext("2d");
  assert.equal(contextOf(SLOT + 1, flat), null);
  forgetContext(SLOT);
  forgetContext(SLOT + 1);
});

test("the driver's shaders, program and buffer are made from its memory and kept by number, the first number none", () => {
  const encoded = new TextEncoder().encode(VERTEX + FRAGMENT);
  const buffer = new ArrayBuffer(encoded.length);
  new Uint8Array(buffer).set(encoded);
  const calls = glCalls(() => buffer);
  const canvas = Object.assign(document.createElement("canvas"), { width: 8, height: 8 });
  const gl = contextOf(SLOT, canvas);
  try {
    const vertex = calls.gl_shader(SLOT, 1, 0, VERTEX.length);
    const fragment = calls.gl_shader(SLOT, 0, VERTEX.length, FRAGMENT.length);
    assert.deepEqual([vertex, fragment], [1, 2]);
    const program = calls.gl_program(SLOT, vertex, fragment);
    assert.equal(program, 3, "linked and kept");
    assert.equal(calls.gl_buffer(SLOT), 4);
    assert.equal(calls.gl_attribute(SLOT, program, VERTEX.indexOf("point"), "point".length), 0, "the program's one attribute, its name read from memory");
    assert.equal(gl.isContextLost(), false);
  } finally {
    forgetContext(SLOT);
  }
});
