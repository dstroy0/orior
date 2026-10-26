// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A statement run from a .sql file of the tree: the one the cursor stands in or the text selected, or each of the
// file's in turn.

import { invoke } from "../bridge.js";
import { readConnections, runOn } from "../database.js";
import { say } from "../statusbar.js";
import { connectionForFile, connectionOf, state } from "./state.js";

// The statement of the .sql file shown that the cursor stands in, or the text selected, and its
// place in the file by character.
function statementHere() {
  const editor = state.hooks.editor();
  const s = editor?.s;
  if (!s) {
    return null;
  }
  const lines = [];
  for (let line = 0; line < s.doc.count; line += 1) {
    lines.push(s.doc.line(line));
  }
  const text = lines.join("\n");
  const offsetOf = (p) => lines.slice(0, p.line).reduce((sum, one) => sum + one.length + 1, 0) + p.col;
  const selection = editor.primary();
  const from = offsetOf(selection.anchor);
  const to = offsetOf(selection.head);
  return { text, lines, from: Math.min(from, to), to: Math.max(from, to), editor };
}

function placeAt(here, offset) {
  let left = offset;
  for (let line = 0; line < here.lines.length; line += 1) {
    if (left <= here.lines[line].length) {
      here.editor.goTo(line, left);
      return;
    }
    left -= here.lines[line].length + 1;
  }
}

const kindOf = (name) => connectionOf(name)?.kind ?? "postgres";

// The page counts text in UTF-16 units and the window's side and the servers count it in characters: the
// characters before a place the page gives, and the units before a count of characters.
const charsBefore = (text, units) => [...text.slice(0, units)].length;
const unitsBefore = (text, chars) => [...text].slice(0, chars).join("").length;

// Run, Run Statement: the statement the cursor stands in, or the text selected, on the file's
// connection.
export async function runStatement() {
  const file = state.hooks.file();
  const here = statementHere();
  if (!file || !here) {
    say("Run Statement runs a statement of the file shown", { failed: true });
    return;
  }
  const name = connectionForFile(file);
  if (!state.connections.length) {
    await readConnections();
  }
  // Where the statement starts, in characters.
  let start = charsBefore(here.text, here.from);
  let statement = here.text.slice(here.from, here.to);
  if (!statement.trim()) {
    const span = await invoke("db_statement_at", { text: here.text, offset: start, kind: kindOf(name) });
    if (!span) {
      say("The cursor stands in no statement: Run File runs them all");
      return;
    }
    [start] = span;
    statement = [...here.text].slice(span[0], span[1]).join("");
  }
  const said = await runOn(name, statement);
  if (said?.failure?.position) {
    placeAt(here, unitsBefore(here.text, start + said.failure.position - 1));
  }
}

// Run, Run SQL File: each statement of the file in turn, stopping at the first that fails.
export async function runSqlFile() {
  const file = state.hooks.file();
  const here = statementHere();
  if (!file || !here) {
    return;
  }
  const name = connectionForFile(file);
  if (!state.connections.length) {
    await readConnections();
  }
  const spans = await invoke("db_statements", { text: here.text, kind: kindOf(name) });
  const characters = [...here.text];
  let done = 0;
  for (const [from, to] of spans) {
    const said = await runOn(name, characters.slice(from, to).join(""));
    if (!said || said.failure) {
      if (said?.failure?.position) {
        placeAt(here, unitsBefore(here.text, from + said.failure.position - 1));
      }
      say(`${done} of ${spans.length} statements ran; the next ${said ? "failed" : "did not run"}`, { failed: true });
      return;
    }
    done += 1;
  }
  say(`Each of the ${spans.length} statements of ${file} ran on ${name}`);
}
