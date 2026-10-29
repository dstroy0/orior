// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's languages built from the tables the tree holds: a row of a table read by its columns, the word a
// form opens with, a table of bytes told from one of lines, a k-file's language with its hover and completions, and
// a k-file, a .gsm and a .g file colored on orior's side by the words those tables give.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { isBinary, opening, registerLanguages, rowOf } from "../../gui/src-js/languages.js";
import { showView } from "../../gui/src-js/views.js";
import { putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

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
  assert.equal(language.id, "kfile:kx", "the kfile plugin's language for its folder, as orior's side colors it");
  assert.deepEqual(language.comments, { line: "#" }, "a comment form opening with # comments a line");
  assert.notEqual(known.languageOf("a.kb")?.id, "kfile:kb");
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

// The file `path` of `text` opened, its colors from orior's side waited for at each place `wanted` names: `[line, col,
// class]`.
async function colored(path, text, wanted) {
  await invoke("file_write", { path, text });
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path && editing().editor?.s, 10000, `${path} open`);
  const highlight = editing().editor.s.highlight;
  for (const [line, col, cls] of wanted) {
    await ui.waitFor(() => highlight.classAt(line, col).split(" ").includes(cls), 10000, `${path} ${line}:${col} ${cls}, ${highlight.classAt(line, col)}`);
  }
}

// The tables the plugins kfile, gsm and g read their words from, written into the tree at the paths they name.
const TABLES = {
  "src/cu/includes/file_defs/krs/krs.oracle.tsv": "where\twho\tkind\tform\tgloss\nline 1\t-\tcomment\t# <text>\ta note\nline 2\t-\truleset\truleset <name>\tthe ruleset's name\n",
  "src/lng/gnascor_asm_lng.tsv": "mnemonic\ttable\tpast\tpresent\tarrow\ncore\tthe pair\tlead\tvoid\t->\n",
  "src/lng/gnascor_hol_lng.tsv": "word\tkind\ni32_t\ttype\n",
};

test("a k-file, a .gsm and a .g file open colored on orior's side by the words the tree's tables give", async () => {
  for (const [path, text] of Object.entries(TABLES)) {
    const parts = path.split("/");
    for (let depth = 1; depth < parts.length; depth += 1) {
      await invoke("folder_create", { path: parts.slice(0, depth).join("/") }).catch(() => {});
    }
    await invoke("file_write", { path, text });
  }
  await colored("languages_colored.krs", "# a note\nruleset sm_86\n", [[0, 0, "t-comment"], [1, 0, "t-keyword"]]);
  await colored("languages_colored.gsm", "core lead -> void\n", [[0, 0, "t-keyword"], [0, 5, "t-type"], [0, 10, "t-operator"]]);
  await colored("languages_colored.g", "i32_t count = 1;\nint other;\n", [[0, 0, "t-type"], [1, 0, "t-keyword"]]);
});

test("the window is put back as the file found it", putBackAsFound);
