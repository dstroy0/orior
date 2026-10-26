// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The symbols of a file for the outline, found by each line's shape in its language, as deep as their indent, and
// the regions its comments mark among them.

import { regionsOf, structureOf, symbolsOf } from "../../gui/src-js/outline.js";

const { test, assert } = window.__harness;

const docOf = (text) => {
  const lines = text.split("\n");
  return { count: lines.length, line: (at) => lines[at] };
};
const brief = (symbols) => symbols.map(({ name, kind, depth }) => `${name}:${kind}:${depth}`);

test("Python's functions, classes and constants, each as deep as its indent", () => {
  const doc = docOf("LIMIT = 10\nclass Shape:\n    def area(self):\n        pass\nasync def main():\n    pass");
  assert.deepEqual(brief(symbolsOf("python", doc)), ["LIMIT:constant:0", "Shape:class:0", "area:function:1", "main:function:0"]);
});

test("JavaScript's functions, arrow functions, classes, constants and methods", () => {
  const doc = docOf("export function run() {}\nconst add = (a, b) => a + b;\nconst LIMIT = 3;\nclass View {\n  draw(at) {\n  }\n}");
  assert.deepEqual(brief(symbolsOf("javascript", doc)), ["run:function:0", "add:function:0", "LIMIT:constant:0", "View:class:0", "draw:method:1"]);
});

test("Rust's functions, types, modules and implementations, and a language with no rules has none", () => {
  const doc = docOf("pub struct Point;\nimpl Point {\n    pub fn len(&self) -> f64 {\n    }\n}\nmod tests;");
  assert.deepEqual(brief(symbolsOf("rust", doc)), ["Point:class:0", "Point:class:0", "len:function:1", "tests:module:0"]);
  assert.deepEqual(symbolsOf("no-such-language", doc), []);
});

test("the regions comments mark, nested, one not closed reaching the end, and the symbols inside them a level deeper", () => {
  const doc = docOf("# region Setup\ndef a():\n    pass\n# region Inner\nB = 1\n# endregion\n# endregion\n# region Open\ndef c():\n    pass");
  assert.deepEqual(regionsOf(doc).map(({ name, line, end }) => [name, line, end]), [["Setup", 0, 6], ["Inner", 3, 5], ["Open", 7, 9]]);
  assert.deepEqual(brief(structureOf("python", doc)), ["Setup:region:0", "a:function:1", "Inner:region:1", "B:constant:2", "Open:region:0", "c:function:1"]);
});
