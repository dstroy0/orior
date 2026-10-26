// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The languages Extract Function has a form for: the name each part of a parameter list declares, the names a line
// sets, a type read from the language server's hover, and a function written and called.

import { LANGUAGES, nameOfPart } from "../../../gui/src-js/extract/languages.js";

const { test, assert } = window.__harness;

test("a parameter's name, past a spread, a default or a type, Rust's self none, and C's last name before an array", () => {
  assert.equal(nameOfPart.default("*args"), "args");
  assert.equal(nameOfPart.default("**options"), "options");
  assert.equal(nameOfPart.default("...rest"), "rest");
  assert.equal(nameOfPart.default("count=0"), "count");
  assert.equal(nameOfPart.default(" name: str = 'x'"), "name");
  assert.equal(nameOfPart.rust("&mut self"), "");
  assert.equal(nameOfPart.rust("&'a self"), "");
  assert.equal(nameOfPart.rust("mut total: u32"), "total");
  assert.equal(nameOfPart.c("const char *name"), "name");
  assert.equal(nameOfPart.c("int values[4]"), "values");
  assert.equal(nameOfPart.c("int count = 0"), "count");
});

test("the names a line sets: assigned, bound, taken by a loop, after as, and stepped", () => {
  const { python, javascript, rust, c } = LANGUAGES;
  assert.deepEqual(python.sets("a, b = pair"), ["a", "b"]);
  assert.deepEqual(python.sets("total += 1"), ["total"]);
  assert.deepEqual(python.sets("for i, row in rows:"), ["i", "row"]);
  assert.deepEqual(python.sets("with open(path) as handle:"), ["handle"]);
  assert.deepEqual(python.sets("if x == 1:"), []);
  assert.deepEqual(javascript.sets("const total = 0;"), ["total"]);
  assert.deepEqual(javascript.sets("count ??= 1;"), ["count"]);
  assert.deepEqual(javascript.sets("for (let i = 0; i < n; i++) {"), ["i"]);
  assert.deepEqual(rust.sets("let mut total = 0;"), ["total"]);
  assert.deepEqual(rust.sets("let (a, mut b) = pair;"), ["a", "b"]);
  assert.deepEqual(rust.sets("for (k, v) in map {"), ["k", "v"]);
  assert.deepEqual(c.sets("int count = 0;"), ["count"]);
  assert.deepEqual(c.sets("x = y;"), ["x"]);
  assert.deepEqual(c.sets("i++;"), ["i"]);
  assert.deepEqual(c.sets("return x;"), []);
});

test("a type from the hover: Rust's with its mut, and C's for a parameter or a local, a field marked as one", () => {
  assert.deepEqual(LANGUAGES.rust.typeIn("```rust\nlet mut total: u32\n```", "total"), { type: "u32", mut: true });
  assert.deepEqual(LANGUAGES.rust.typeIn("```rust\nname: &str\n```", "name"), { type: "&str", mut: false });
  assert.equal(LANGUAGES.rust.typeIn("```rust\nfn other()\n```", "name"), null);
  assert.deepEqual(LANGUAGES.c.typeIn("### param `n`\n\nType: `int`"), { type: "int" });
  assert.deepEqual(LANGUAGES.c.typeIn("### variable `sum`\n\nType: `long`\n\n// In main"), { type: "long" });
  assert.equal(LANGUAGES.c.typeIn("### variable `limit`\n\nType: `int`\n\n// In namespace app"), null, "a variable of a namespace is no local");
  assert.deepEqual(LANGUAGES.c.typeIn("### field `x`\n\nType: `int`"), { field: true });
});

test("a function written in each language's form, and its call", () => {
  const { python, javascript } = LANGUAGES;
  assert.equal(python.define("total", ["a", "b"], ["s = a + b", "", "return s"], "    ", false), "def total(a, b):\n    s = a + b\n\n    return s\n");
  assert.equal(python.call("total", ["a", "b"], true), "await total(a, b)");
  assert.equal(python.assign(["s", "t"], "total(a)"), "s, t = total(a)");
  assert.equal(javascript.define("total", ["a"], ["return a;"], "  ", true), "async function total(a) {\n  return a;\n}\n");
  assert.equal(javascript.assign(["s"], "total(a)", "const"), "const s = total(a);");
  assert.equal(javascript.giveBack(["s", "t"]), "return s;", "a function in JavaScript gives back one name");
});
