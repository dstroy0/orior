// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A word incised: its Roman capitals drawn as the cuts a stylus makes in clay, each wide where the
// stylus went in and narrowing to its tail.

import { incised } from "../../gui/src-js/incised.js";

const { test, assert } = window.__harness;

// The corners of a cut's outline, from its path: one side from head to tail, the other back.
const corners = (cut) =>
  cut
    .getAttribute("d")
    .match(/-?[\d.]+\s+-?[\d.]+/g)
    .map((pair) => pair.split(/\s+/).map(Number));

const apart = ([ax, ay], [bx, by]) => Math.hypot(ax - bx, ay - by);

test("a word incised is its Roman capitals, one cut for each stroke a stylus makes, with no text of its own", () => {
  const word = incised("TABVLA");
  assert.equal(word.getAttribute("aria-hidden"), "true", "the word read from the name it stands under, not from its cuts");
  assert.equal(word.textContent, "", "no text of its own");
  assert.equal(word.dataset.word, "TABVLA");
  // T two strokes, A three, B a stem and two bowls, V two, L two, A three
  assert.equal(word.querySelectorAll("path.cut").length, 15, "a cut for each stroke");
  assert.equal(word.querySelectorAll("path").length, 15, "nothing drawn but the cuts");
});

test("each cut is widest where the stylus went in and narrows to its tail", () => {
  const word = incised("TABVLA");
  for (const cut of word.querySelectorAll("path.cut")) {
    const outline = corners(cut);
    const half = outline.length / 2;
    const head = apart(outline[0], outline.at(-1));
    const tail = apart(outline[half - 1], outline[half]);
    assert.ok(head > 2 * tail, `head ${head.toFixed(2)} against tail ${tail.toFixed(2)} in ${cut.getAttribute("d").slice(0, 40)}`);
  }
});

test("the letters stand on one line at one height, each apart from the next", () => {
  const word = incised("TABVLA");
  document.body.append(word);
  word.style.cssText = "position: fixed; left: 0; top: 0; height: 60px;";
  try {
    const letters = [...word.querySelectorAll("g.letter")].map((letter) => letter.getBoundingClientRect());
    assert.equal(letters.length, 6, "six letters");
    // a stroke's head may stand out of the line by its own width, and no further
    const head = 60 * 0.1;
    const tops = letters.map((box) => box.top);
    const bottoms = letters.map((box) => box.bottom);
    assert.ok(Math.max(...tops) - Math.min(...tops) < head, `one top line: ${tops.map(Math.round)}`);
    assert.ok(Math.max(...bottoms) - Math.min(...bottoms) < head, `one foot line: ${bottoms.map(Math.round)}`);
    letters.slice(1).forEach((box, index) => assert.ok(box.left > letters[index].right, `letter ${index + 2} apart from the one before`));
  } finally {
    word.remove();
  }
});
