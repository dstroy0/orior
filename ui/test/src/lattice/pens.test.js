// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The pens a lattice's points draw with: the ordered end in the first of the lattice's colors, the disordered in the
// last, the ordered putting down more ink and drawing larger.

import { DIM, inkOf, pensOf } from "../../../gui/src-js/lattice/pens.js";
import { rgbOf } from "../../../gui/src-js/colors.js";

const { test, assert } = window.__harness;

const partsOf = (color) => color.match(/[\d.]+/g).map(Number);

test("a point's ink grows with its order, from the faintest dimmed by DIM to the brightest at 0.42", () => {
  assert.equal(inkOf(0), 0.426 * DIM);
  assert.equal(inkOf(1), 0.42);
  assert.ok(inkOf(1) > inkOf(0.5) && inkOf(0.5) > inkOf(0));
});

test("the pens run from the first of the lattice's colors, ordered, to the last, disordered, and are kept once made", () => {
  const pens = pensOf(5);
  assert.equal(pens.length, 5);
  const first = partsOf(pens[0].color);
  const last = partsOf(pens[4].color);
  assert.deepEqual(first.slice(0, 3), rgbOf("--lattice-1"));
  assert.deepEqual(last.slice(0, 3), rgbOf("--lattice-3"));
  assert.deepEqual(partsOf(pens[2].color).slice(0, 3), rgbOf("--lattice-2"), "the middle pen is the middle color");
  assert.equal(first[3], Number(inkOf(1).toFixed(3)));
  assert.ok(pens[0].size > pens[4].size, "an ordered point draws larger");
  assert.equal(pensOf(5), pens);
});
