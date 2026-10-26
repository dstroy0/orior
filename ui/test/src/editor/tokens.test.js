// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tokens of a text's lines, worked out as they are asked for from the state the line above ends in, and
// forgotten from an edited line on.

import { compile, Highlight } from "../../../gui/src-js/editor/tokens.js";

const { test, assert } = window.__harness;

const GRAMMAR = compile({
  tokenizer: {
    root: [
      [/\/\*/, "comment", "@comment"],
      [/[a-z]+/, "identifier"],
      [/\s+/, ""],
    ],
    comment: [
      [/\*\//, "comment", "@pop"],
      [/./, "comment"],
    ],
  },
});

const docOf = (lines) => ({ lines, get count() { return this.lines.length; }, line(at) { return this.lines[at]; } });

test("a line's tokens are worked out from the state the lines above end in, however far down it is asked for first", () => {
  const doc = docOf(["let a /* open", "inside", "still inside", "close */ b"]);
  const tokens = new Highlight(doc, GRAMMAR);
  assert.equal(tokens.classAt(2, 0), "t-comment", "inside a comment two lines down");
  assert.equal(tokens.classAt(3, 9), "t-identifier", "after the comment closes");
  assert.equal(tokens.stateAt(1), "root/comment");
});

test("an edit forgets the tokens from its line on, and the lines below are worked out again from it", () => {
  const doc = docOf(["let a /* open", "inside", "close */ b"]);
  const tokens = new Highlight(doc, GRAMMAR);
  assert.equal(tokens.classAt(1, 0), "t-comment");
  doc.lines[0] = "let a";
  tokens.forget(0);
  assert.equal(tokens.classAt(1, 0), "t-identifier", "no comment opens above it now");
});

test("a text with no grammar is one uncolored run a line", () => {
  const tokens = new Highlight(docOf(["anything"]), null);
  assert.deepEqual(tokens.runsNow(0), [[0, ""]]);
  assert.equal(tokens.stateAt(0), "root");
});
