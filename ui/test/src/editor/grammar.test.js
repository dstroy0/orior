// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A grammar as the page holds it: the classes a token's name is drawn in, a pair's depth among them, and every kind of
// token a grammar's rules name noted for the themes.

import { classOf, compile, tokenKinds } from "../../../gui/src-js/editor/grammar.js";

const { test, assert } = window.__harness;

test("a token's classes name it and each kind above it, as number.cost is t-number and t-number-cost", () => {
  assert.equal(classOf("number.cost"), "t-number t-number-cost");
  assert.equal(classOf("keyword"), "t-keyword");
  assert.equal(classOf(""), "");
});

test("a token named with the depth its pair stands at takes t-depth and the depth's own class, and no other name does", () => {
  assert.equal(
    classOf("delimiter.bracket.depth.open.3"),
    "t-delimiter t-delimiter-bracket t-delimiter-bracket-depth t-delimiter-bracket-depth-open t-delimiter-bracket-depth-open-3 t-depth t-depth-3",
  );
  assert.equal(classOf("delimiter.bracket.depth"), "t-delimiter t-delimiter-bracket t-delimiter-bracket-depth");
  assert.equal(classOf("number.3"), "t-number t-number-3");
});

test("every kind of token a grammar's rules name is noted for the themes, a bracket's among them", () => {
  const grammar = compile({
    keywords: ["let"],
    perLine: true,
    tokenizer: {
      root: [
        [/\/\*/, "comment.block", "@comment"],
        [/[a-z]+/, { cases: { "@keywords": "keyword.flow", "@default": "identifier.plain" } }],
        [/[()]/, "@brackets"],
      ],
      comment: [[/\*\//, { token: "comment.block", next: "@pop" }]],
    },
  });
  assert.ok(grammar.perLine, "the grammar's own settings are kept");
  assert.ok(["comment.block", "delimiter.bracket", "identifier.plain", "keyword.flow"].every((kind) => tokenKinds().includes(kind)), "each kind compiled is named for the themes");
});
