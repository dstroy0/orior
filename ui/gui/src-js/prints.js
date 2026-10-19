// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger as simple as print. Print Here (Ctrl+Alt+P) puts on the cursor's line a message
// breakpoint that writes the line's values each time the line runs under the debugger, the file
// itself unchanged: the expression selected, or each name the line reads with its value. The print
// statements a change added (prints.rs) stand in the Commit window, each opening at its line, and
// Take Them Out takes every one of them out in one step: into each tab that holds its file as one
// edit undo takes back, and into each other file as it is written.

import { invoke } from "./bridge.js";
import { setMessageBreakpoint } from "./debug.js";
import { editing } from "./edit.js";
import { applyEdits } from "./intel.js";
import { say } from "./statusbar.js";

// The words of a line that are no value of its own: its languages' keywords and literals.
const WORDS = new Set("and as assert async await break case catch class const continue def del do elif else enum except export extends false final finally fn for from function global if impl import in is lambda let loop match mod mut new nil none None not null or pass pub raise return self Self static struct super switch this throw true True False try type typeof use var void where while with yield".split(" "));

// The message a message breakpoint writes for the line `text`: the expression `selected` where one is
// selected, or each name the line reads, not one called or one after a dot, with its value.
export function messageOf(text, selected = "", label = "") {
  if (selected.trim()) {
    return `${label}${selected.trim()} = {${selected.trim()}}`;
  }
  const names = [];
  for (const match of text.replace(/(["'`]).*?\1/g, "").replace(/(#|\/\/).*$/, "").matchAll(/[A-Za-z_][A-Za-z0-9_]*/g)) {
    const name = match[0];
    const before = text[match.index - 1];
    const after = text.slice(match.index + name.length).trimStart()[0];
    if (!WORDS.has(name) && before !== "." && after !== "(" && !names.includes(name)) {
      names.push(name);
    }
  }
  return `${label}${names.map((name) => `${name}={${name}}`).join(" ")}`.trim();
}

// Print Here: a message breakpoint on the cursor's line that writes the line's values.
export function printHere() {
  const { editor, active } = editing();
  if (!editor?.s || !active) {
    say("Print Here prints the values of a line of a file open in the editor", { failed: true });
    return;
  }
  const s = editor.s;
  const sel = s.selections[s.primary];
  const head = sel.head;
  const line = s.base + head.line;
  const same = sel.anchor.line === head.line;
  const selected = same && sel.anchor.col !== head.col ? s.doc.line(head.line).slice(Math.min(sel.anchor.col, head.col), Math.max(sel.anchor.col, head.col)) : "";
  const message = messageOf(s.doc.line(head.line), selected, `${active.split("/").pop()}:${line + 1} `);
  setMessageBreakpoint(active, line, message);
  say(`Line ${line + 1} prints ${message.split(" ").slice(1).join(" ") || "its line"} each time it runs under the debugger: Run, Debug File`);
}

// The print statements the tree's change against its last commit added.
export function addedPrints() {
  return invoke("prints_added").catch(() => []);
}

// Takes the prints `prints` out, each line where it still stands or the nearest line that reads as it
// did, in one step for each file. Says how many it took out.
export async function takeOutPrints(prints) {
  const files = new Map();
  for (const print of prints) {
    const session = editing().sessionOf(print.path);
    let line = print.line;
    if (session) {
      const doc = session.doc;
      const at = print.line - session.base;
      const found = [0, ...Array.from({ length: 200 }, (_, step) => [step + 1, -(step + 1)]).flat()].map((step) => at + step).find((one) => one >= 0 && one < doc.count && doc.line(one) === print.text);
      if (found === undefined) {
        continue;
      }
      line = found + session.base;
    }
    if (!files.has(print.path)) {
      files.set(print.path, []);
    }
    files.get(print.path).push({ from: { line, col: 0 }, to: { line: line + 1, col: 0 }, text: "" });
  }
  const given = [...files].map(([path, edits]) => ({ path, edits: edits.sort((a, b) => b.from.line - a.from.line) }));
  const done = await applyEdits(given);
  say(`${done.places} print${done.places === 1 ? "" : "s"} taken out of ${done.files} file${done.files === 1 ? "" : "s"}`);
  return done.places;
}
