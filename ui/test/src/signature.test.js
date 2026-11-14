// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Change Signature's reading and writing: a bracket's list read to its close, the name a parameter declares in each
// language, a call's arguments as its parameters moved, and the calls of a function in a text.

import { argumentsFor, callsIn, listAt, paramName } from "../../gui/src-js/signature.js";

const { test, assert } = window.__harness;

test("a bracket's list is read to its close, past nested brackets and strings, each part trimmed", () => {
  const text = 'f(a, g(b, c),  "x,y" ) + 1';
  const list = listAt(text, 1);
  assert.equal(list.close, text.indexOf(")", text.indexOf('"x,y"')));
  assert.deepEqual(list.parts.map(([from, to]) => text.slice(from, to)), ["a", "g(b, c)", '"x,y"']);
  assert.deepEqual(listAt("f()", 1).parts, [], "an empty list has no parts");
  const generic = "f(Map<K, V>, b)";
  assert.deepEqual(listAt(generic, 1, true).parts.map(([from, to]) => generic.slice(from, to)), ["Map<K, V>", "b"], "a generic's comma inside its angles");
  assert.deepEqual(listAt(generic, 1).parts.map(([from, to]) => generic.slice(from, to)), ["Map<K", "V>", "b"], "and split where angles are not read");
});

test("the name a parameter declares, by its language, and none for a pattern taken apart", () => {
  assert.equal(paramName("count: int = 3", "python"), "count");
  assert.equal(paramName("*args", "python"), "args");
  assert.equal(paramName("...rest", "javascript"), "rest");
  assert.equal(paramName("{ a, b }", "javascript"), null);
  assert.equal(paramName("mut total: usize", "rust"), "total");
  assert.equal(paramName("&'a mut self", "rust"), "self");
  assert.equal(paramName("const char *names[]", "c"), "names");
  assert.equal(paramName("int n = 4", "cpp"), "n");
});

test("a call's arguments follow their parameters: moved, taken out, added with their value, and a gap filled", () => {
  const python = { language: "python", keywords: true };
  assert.deepEqual(argumentsFor(["1", "2"], [{ from: 1, text: "b" }, { from: 0, text: "a" }, { from: null, text: "c", value: "3" }], python), ["2", "1", "3"]);
  assert.deepEqual(argumentsFor(["1", "2"], [{ from: 1, text: "b" }], python), ["2"]);
  assert.deepEqual(argumentsFor(["1"], [{ from: 0, text: "a" }, { from: 1, text: "b=5" }, { from: null, text: "c", value: "9" }], python), ["1", "5", "9"], "a gap takes the parameter's default");
  assert.deepEqual(argumentsFor(["1"], [{ from: 0, text: "a" }, { from: 1, text: "b" }, { from: null, text: "c", value: "9" }], python), ["1", "None", "9"], "or the language's own empty value");
  assert.deepEqual(argumentsFor(["1"], [{ from: 0, text: "a" }, { from: 1, text: "b" }, { from: null, text: "c", value: "9" }], { language: "rust" }), ["1"], "a gap Rust cannot fill ends the arguments");
});

test("Python's keyword arguments retain their places and take a parameter's new name, and a method's receiver stays first", () => {
  assert.deepEqual(argumentsFor(["1", "b=2"], [{ from: 0, text: "a", oldName: "a" }, { from: 1, text: "bee", oldName: "b" }], { language: "python", keywords: true }), ["1", "bee=2"]);
  assert.deepEqual(argumentsFor(["1", "2"], [{ from: 0, text: "self" }, { from: 2, text: "y" }, { from: 1, text: "x" }], { language: "python", receiver: true }), ["2", "1"]);
});

test("the calls of a function in a text, its declaration not among them, a dotted call marked, and a C prototype a declaration", () => {
  const python = "def f(a):\n    f(1)\n    obj.f(2)\n";
  assert.deepEqual(callsIn(python, "f", "python").map(({ open, dotted, declares }) => [python.slice(open - 1, open + 2), dotted, declares]), [["f(1", false, false], ["f(2", true, false]]);
  const c = "int f(int a);\nint main(void) { return f(1); }";
  assert.deepEqual(callsIn(c, "f", "c").map(({ declares }) => declares), [true, false]);
});
