// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A grammar compiled and a line colored from it: its rules tried in order, words picked by their lists, brackets,
// states entered and left, and the state carried from one line to the next.

import { classOf, compile, grammarJson, tokenize, tokenKinds } from "../../../gui/src-js/editor/grammar.js";

const { test, assert } = window.__harness;

const DEF = {
  keywords: ["let", "if"],
  tokenizer: {
    root: [
      [/\/\*/, "comment", "@comment"],
      [/[a-z]+/, { cases: { "@keywords": "keyword", "@default": "identifier" } }],
      [/\d+/, "number.cost"],
      [/[()]/, "@brackets"],
      [/\s+/, ""],
    ],
    comment: [
      [/\*\//, "comment", "@pop"],
      [/./, "comment"],
    ],
  },
};

test("a token's classes name it and each kind above it, as number.cost is t-number and t-number-cost", () => {
  assert.equal(classOf("number.cost"), "t-number t-number-cost");
  assert.equal(classOf("keyword"), "t-keyword");
  assert.equal(classOf(""), "");
});

test("a line's runs: a word by its list, a number, brackets, and what no rule takes uncolored", () => {
  const grammar = compile(DEF);
  const { runs, state } = tokenize(grammar, "let x = 12 (y)", "root");
  assert.deepEqual(runs, [
    [0, "t-keyword"],
    [3, ""],
    [4, "t-identifier"],
    [5, ""],
    [8, "t-number t-number-cost"],
    [10, ""],
    [11, "t-delimiter t-delimiter-bracket"],
    [12, "t-identifier"],
    [13, "t-delimiter t-delimiter-bracket"],
  ]);
  assert.equal(state, "root");
  assert.ok(["comment", "delimiter.bracket", "identifier", "keyword", "number.cost"].every((kind) => tokenKinds().includes(kind)), "each kind compiled is named for the themes");
});

test("each bracket is a token of its own, never joined to the bracket beside it", () => {
  const { runs } = tokenize(compile(DEF), "(((())))", "root");
  assert.deepEqual(runs, Array.from({ length: 8 }, (_, col) => [col, "t-delimiter t-delimiter-bracket"]));
});

test("a state entered on one line is carried to the next, and left where its rule says", () => {
  const grammar = compile(DEF);
  const first = tokenize(grammar, "x /* a", "root");
  assert.deepEqual(first.runs, [[0, "t-identifier"], [1, ""], [2, "t-comment"], [4, "t-comment"], [5, "t-comment"]]);
  assert.equal(first.state, "root/comment");
  const second = tokenize(grammar, "b */ y", first.state);
  assert.deepEqual(second.runs, [[0, "t-comment"], [1, "t-comment"], [2, "t-comment"], [4, ""], [5, "t-identifier"]]);
  assert.equal(second.state, "root");
  const perLine = compile({ ...DEF, perLine: true });
  assert.equal(tokenize(perLine, "x /* a", "root").state, "root", "a grammar read a line at a time starts each in root");
});

test("a grammar as the window's side reads it is JSON, each pattern its source and flags", () => {
  const json = JSON.parse(grammarJson(compile({ tokenizer: { root: [[/ab+/i, "word"]] } })));
  assert.deepEqual(json.tokenizer.root[0], [{ pattern: "ab+", flags: "i" }, "word"]);
});
