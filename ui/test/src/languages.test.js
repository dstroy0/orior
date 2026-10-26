// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's languages built from the tables the tree holds: a row of a table read by its columns, the word a
// form opens with, a table of bytes told from one of lines, and a k-file's language with its hover and completions.

import { isBinary, opening, registerLanguages, rowOf } from "../../gui/src-js/languages.js";

const { test, assert } = window.__harness;

const COLUMNS = ["where", "who", "kind", "form", "gloss"];
const LINES = {
  ext: "kx",
  table: {
    columns: COLUMNS,
    rows: [
      ["line 1", "-", "comment", "# <text>", "a comment"],
      ["line 2", "teacher", "ask", "ask <form> ... = <text>", "an ask of the teacher's"],
    ],
  },
};
const BYTES = { ext: "kb", table: { columns: COLUMNS, rows: [["head 0", "-", "magic", "<bytes>", "the file's mark"]] } };

const docOf = (...lines) => ({ line: (at) => lines[at] });

test("a row is read by its columns' names, and a form opens with its first word or with none", () => {
  assert.deepEqual(rowOf(LINES.table, LINES.table.rows[1]), { where: "line 2", who: "teacher", kind: "ask", form: "ask <form> ... = <text>", gloss: "an ask of the teacher's" });
  assert.deepEqual(rowOf({ columns: ["form"], rows: [] }, ["x"]), { where: "", who: "", kind: "", form: "x", gloss: "" }, "a column the table lacks read as empty");
  assert.equal(opening("  form <name> = <text>"), "form");
  assert.equal(opening("<channel> <answer>"), "");
  assert.ok(isBinary(BYTES));
  assert.ok(!isBinary(LINES));
});

test("a k-file of lines has a language of its own, a table of bytes none, and a path its type", () => {
  const known = registerLanguages({ types: [LINES, BYTES], languages: [] });
  const language = known.languageOf("dir/a.KX");
  assert.equal(language.id, "kx");
  assert.deepEqual(language.comments, { line: "#" }, "a comment form opening with # comments a line");
  assert.notEqual(known.languageOf("a.kb")?.id, "kb");
  assert.equal(known.typeOf("a.kb"), BYTES);
  assert.equal(known.typeOf("a.none"), null);
  assert.deepEqual(known.tablesOf("gsm"), [], "no assembly table given");
});

test("hovering the word a line opens with shows each form it opens, and completing a line's head offers each", () => {
  const language = registerLanguages({ types: [LINES], languages: [] }).languageOf("a.kx");
  const doc = docOf("  ask one = two");
  const hover = language.hover(doc, { line: 0, col: 3 });
  assert.deepEqual([hover.from.col, hover.to.col], [2, 5]);
  assert.equal(hover.parts[0], "**ask**\n\n`ask <form> ... = <text>`\n\nan ask of the teacher's\n\n_where line 2, who teacher_");
  assert.equal(language.hover(doc, { line: 0, col: 9 }), null, "past the line's first word");
  const offered = language.complete(docOf("as"), { line: 0, col: 2 });
  assert.deepEqual(offered.map((one) => [one.label, one.insert]), [["ask", "ask ${1:form} = ${2:text}"]]);
  assert.deepEqual(language.complete(docOf("ask x"), { line: 0, col: 5 }), [], "past the head");
});
