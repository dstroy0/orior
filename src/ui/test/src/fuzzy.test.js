// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { fuzzy, marked } from "../../src/fuzzy.js";

const { test, assert } = window.__harness;

test("an empty query matches with no places", () => {
  assert.deepEqual(fuzzy("", "main.js"), { score: 0, hits: [] });
});

test("the query's letters in order match, case aside", () => {
  const found = fuzzy("MJ", "main.js");
  assert.ok(found, "MJ matches main.js");
  assert.deepEqual(found.hits, [0, 5]);
});

test("letters out of order do not match", () => {
  assert.equal(fuzzy("jm", "main.js"), null);
});

test("spaces in the query are left out", () => {
  assert.deepEqual(fuzzy("ma in", "main.js")?.hits, [0, 1, 2, 3]);
});

test("a letter starting a word is taken over an earlier one inside a word", () => {
  assert.deepEqual(fuzzy("v", "move/view.js", 5).hits, [5]);
});

test("a query torn into more pieces than half its letters or three does not match", () => {
  assert.equal(fuzzy("abcd", "a_x_b_x_c_x_d"), null);
  assert.ok(fuzzy("abc", "a_x_b_x_c"), "three pieces of three letters match");
});

test("a match in the name outranks one spread over the folders", () => {
  const inName = fuzzy("view", "src/editor/view.js", 11);
  const spread = fuzzy("view", "src/vi/e/w.js", 9);
  assert.ok(inName && spread, "both match");
  assert.ok(inName.score > spread.score, `${inName.score} over ${spread.score}`);
});

test("a run of letters scores more than the same letters apart inside a word", () => {
  assert.ok(fuzzy("abc", "xabcx").score > fuzzy("abc", "xaxbxcx").score);
});

test("letters that start words score more than a run inside one", () => {
  assert.ok(fuzzy("abc", "a_b_c").score > fuzzy("abc", "xabcx").score);
});

test("marked text marks the places matched and keeps the rest as text", () => {
  const nodes = marked("main.js", [0, 1, 5]);
  assert.deepEqual(
    nodes.map((node) => [node.nodeName, node.textContent]),
    [
      ["MARK", "ma"],
      ["#text", "in."],
      ["MARK", "j"],
      ["#text", "s"],
    ],
  );
});

test("marked text with no places is one node of text", () => {
  const nodes = marked("main.js", []);
  assert.equal(nodes.length, 1);
  assert.equal(nodes[0].nodeName, "#text");
});
