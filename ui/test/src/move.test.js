// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Move: the lines a declaration takes, its decorators and comments above it and the line that closes it, and the
// path an import of its new file writes.

import { declarationLines, relativePath } from "../../gui/src-js/move.js";

const { test, assert } = window.__harness;

// A session over `text` whose regions end where `ends` says.
const sessionOf = (text, ends) => {
  const lines = text.split("\n");
  return { doc: { count: lines.length, line: (at) => lines[at] }, endOf: (line) => ends[line] ?? line };
};

test("a declaration takes the decorators and comments right above it, to the last line of its region", () => {
  const s = sessionOf("x = 0\n\n# adds two\n@cached\ndef add(a, b):\n    return a + b\n\ny = 1", { 4: 5 });
  assert.deepEqual(declarationLines(s, 4), { first: 2, last: 5 });
});

test("a block's closing line, which its region leaves out, goes with it", () => {
  const s = sessionOf("// one\nfunction one() {\n  return 1;\n}\none();", { 1: 2 });
  assert.deepEqual(declarationLines(s, 1), { first: 0, last: 3 });
  const single = sessionOf("const a = 1;\n}", {});
  assert.deepEqual(declarationLines(single, 0), { first: 0, last: 0 }, "a line alone takes no closing line below it");
});

test("an import's path runs from the folder of the file it is written in", () => {
  assert.equal(relativePath("src/a/b.js", "src/c/d.js"), "../c/d.js");
  assert.equal(relativePath("src/a.js", "src/b.js"), "./b.js");
  assert.equal(relativePath("a.js", "x/y/z.js"), "./x/y/z.js");
  assert.equal(relativePath("x/y/z.js", "a.js"), "../../a.js");
});
