// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Extract Function, in Python, JavaScript, Rust and the C family: the lines selected, or an
// expression on one line, become a function of their own, called where they stood, in one change
// that undo takes back.
//
// The new function takes as parameters the names the selection reads that the function holding it
// had set before it: its own parameters and what its lines above the selection set. It gives back
// the names the selection sets that its function reads after it, as one value, or in Python and
// Rust as a tuple of them. Its name is selected where it is defined and where it is called: the name
// typed next replaces both.
//
// In Python and JavaScript it goes after the outermost definition that holds the selection, or above
// the selection where the selection stands at the top of the file. In Rust and the C family each
// parameter and what the function gives back take their types from the language server's hover, and
// an expression's type from the server's hover on a declaration of it, inferred, that the server is
// shown for the moment of the question alone. In Rust the new function goes after the one
// that holds the selection, a method where the selection reads `self`. A value the selection only
// reads, that its function reads again after it and that is not copied as a number is, it takes by
// reference, and by a mutable reference where it is declared `mut` and the selection reaches into
// it. In the C family the new function goes above the one that holds the selection, where C needs it
// declared, or after it in a class, and it gives back one value at most.
//
// A selection that returns, or that breaks out of a loop it does not hold, stays where it is, as
// does one in Rust that hands an error up with `?`.

import { pos } from "./editor/document.js";
import { endOfSel, startOf } from "./editor/view.js";
import { LANGUAGES, nameOfPart } from "./extract/languages.js";
import { extractTyped } from "./extract/typed.js";
import { say } from "./statusbar.js";

export const indentOf = (text) => text.match(/^\s*/)[0];
export const widthOf = (text, size) => [...indentOf(text)].reduce((sum, char) => sum + (char === "\t" ? size : 1), 0);

// The names a function's parameter list holds.
function paramsOf(list, part = nameOfPart.default) {
  const names = [];
  let depth = 0;
  let pending = "";
  for (const char of `${list},`) {
    if ("([{<".includes(char)) {
      depth += 1;
    } else if (")]}>".includes(char)) {
      depth -= 1;
    }
    if (char === "," && depth === 0) {
      const name = part(pending);
      if (/^[A-Za-z_$][\w$]*$/.test(name) && name !== "void") {
        names.push(name);
      }
      pending = "";
    } else {
      pending += char;
    }
  }
  return names;
}

// The columns of a line that are code inside a string: what the braces of a Python f-string hold,
// what ${ } holds in a JavaScript template, and the name a Rust format string's braces name.
function codeInStrings(text, id) {
  const spans = [];
  const braces = (from, to, open) => {
    let depth = 0;
    let start = -1;
    for (let at = from; at < to; at += 1) {
      if (text.startsWith(open, at) && depth === 0 && text[at + open.length] !== "{" && text[at - 1] !== "{") {
        depth = 1;
        start = at + open.length;
        at += open.length - 1;
      } else if (depth && text[at] === "{") {
        depth += 1;
      } else if (depth && text[at] === "}") {
        depth -= 1;
        if (!depth) {
          spans.push([start, at]);
        }
      }
    }
  };
  if (id === "rust") {
    for (const found of text.matchAll(/"((?:\\.|[^"\\])*)"/g)) {
      for (const named of found[1].matchAll(/(?<!\{)\{([A-Za-z_]\w*)(?::[^}]*)?\}/g)) {
        const start = found.index + 1 + named.index + 1;
        spans.push([start, start + named[1].length]);
      }
    }
    return spans;
  }
  for (const found of text.matchAll(/(?<![\w])[rRbB]?[fF][rRbB]?("|')((?:\\.|(?!\1).)*)\1/g)) {
    const from = found.index + found[0].indexOf(found[1]) + 1;
    braces(from, from + found[2].length, "{");
  }
  for (const found of text.matchAll(/`((?:\\.|[^`])*)`/g)) {
    braces(found.index + 1, found.index + 1 + found[1].length, "${");
  }
  return spans;
}

// Each name in lines `from` to `to` of the document that is code and not a member, with where it
// stands and whether it is called there or named as a keyword argument. In Rust and the C family a
// name in a path, before or after `::`, a macro's name and a lifetime are no names either.
function namesIn(s, from, to, cols = null) {
  const id = s.language?.id;
  const found = [];
  for (let line = from; line <= to; line += 1) {
    const text = s.doc.line(line);
    const start = line === from && cols ? cols.from : 0;
    const end = line === to && cols ? cols.to : text.length;
    const coded = codeInStrings(text, id);
    for (const match of text.slice(start, end).matchAll(/[A-Za-z_$][\w$]*/g)) {
      const col = start + match.index;
      const type = s.highlight.classAt(line, col);
      if (/t-comment/.test(type) || (/t-string/.test(type) && !coded.some(([a, b]) => a <= col && col < b))) {
        continue;
      }
      const before = text.slice(0, col).trimEnd();
      if (before.endsWith(".") && !before.endsWith("..")) {
        continue;
      }
      const after = text.slice(col + match[0].length);
      if ((id === "rust" || id === "c") && (before.endsWith("::") || /^\s*::/.test(after) || /^!/.test(after) || before.endsWith("'") || before.endsWith("->"))) {
        continue;
      }
      const called = /^\s*\(/.test(after);
      const keyword = /^\s*=(?!=)/.test(after) && /[(,]\s*$/.test(before) && id !== "rust" && id !== "c";
      found.push({ name: match[0], line, col, called, keyword });
    }
  }
  return found;
}

// The new function's name: `extracted`, with a number after it where the file uses that already.
export function freshName(text) {
  let name = "extracted";
  for (let count = 2; new RegExp(`(?<![\\w$.])${name}\\b`).test(text); count += 1) {
    name = `extracted${count}`;
  }
  return name;
}

// What the selection holds and what stands about it: its lines or its expression, the function that
// holds it with that function's parameters and its end, the names it takes and the names it gives,
// or null with the reason said.
function readSelection(editor, lang) {
  const s = editor.s;
  const doc = s.doc;
  const size = s.indent.size;
  const sel = s.selections[s.primary];
  const from = startOf(sel);
  const to = endOfSel(sel);
  if (from.line === to.line && from.col === to.col) {
    say("Select the lines or the expression for Extract Function.");
    return null;
  }
  // An expression: part of one line. Lines: every line the selection reaches into.
  const lineText = doc.line(from.line);
  const expression = from.line === to.line && (lineText.slice(0, from.col).trim() !== "" || lineText.slice(to.col).trim() !== "");
  const first = from.line;
  const last = expression ? from.line : to.col === 0 && to.line > from.line ? to.line - 1 : to.line;
  const value = expression ? lineText.slice(from.col, to.col).trim() : "";
  const selected = [];
  for (let line = first; line <= last; line += 1) {
    selected.push(doc.line(line));
  }
  const code = expression ? value : selected.join("\n");
  if (!code.trim()) {
    return null;
  }
  if (/(^|[^\w$.])return\b/.test(code) && !expression) {
    say("The lines selected return from their function. Extract Function takes lines that run through to their end.");
    return null;
  }
  if (/^\s*(?:break|continue)\b/m.test(code) && !/^\s*(?:for|while|do|loop)\b/m.test(code)) {
    say("The lines selected leave a loop they do not hold. Extract Function takes lines that run through to their end.");
    return null;
  }
  const firstWidth = widthOf(doc.line(first), size);

  // The function that holds the selection: its first line, its parameters, and where it ends. Up
  // from the selection, each line less indented than all below it opens a block that holds the
  // selection, and the first of them that heads a function holds it. A brace alone on its line opens
  // the block its line above heads. One at the left edge that heads none leaves the selection
  // outside any function.
  let head = -1;
  let params = [];
  let within = firstWidth + (expression ? 1 : 0);
  for (let line = first - (expression ? 0 : 1); line >= 0; line -= 1) {
    const text = doc.line(line);
    const width = widthOf(text, size);
    if (!text.trim() || width >= within || text.trim() === "{") {
      continue;
    }
    const found = lang.heads.map((pattern) => text.match(pattern)).find(Boolean);
    if (found) {
      head = line;
      params = paramsOf(found[2], lang.part);
      break;
    }
    if (width === 0) {
      break;
    }
    within = width;
  }
  const headWidth = head >= 0 ? widthOf(doc.line(head), size) : -1;
  let end = doc.count;
  if (head >= 0) {
    for (let line = last + 1; line < doc.count; line += 1) {
      const text = doc.line(line);
      if (text.trim() && widthOf(text, size) <= headWidth) {
        end = line;
        break;
      }
    }
  }

  // What the selection reads and sets, and what its function set before it and reads after it.
  const inside = expression ? namesIn(s, first, last, { from: from.col, to: to.col }) : namesIn(s, first, last);
  const setsInside = new Map();
  if (!expression) {
    selected.forEach((text, at) => {
      for (const name of lang.sets(text)) {
        if (!setsInside.has(name)) {
          setsInside.set(name, first + at);
        }
      }
    });
  }
  const before = new Set(params);
  if (head >= 0) {
    for (const found of namesIn(s, head + 1, first - 1)) {
      if (!found.called) {
        before.add(found.name);
      }
    }
  }
  const takes = [];
  for (const found of inside) {
    const { name } = found;
    if (lang.keeps.has(name) || found.keyword || takes.includes(name) || !before.has(name)) {
      continue;
    }
    // A name the selection sets on a line above any it reads it on is not taken in. A set that
    // works from the value before it, as `+=` and `++` do, reads it.
    const setAt = setsInside.get(name);
    const steps = (text) => new RegExp(`(?:^|[^\\w$])${name.replace("$", "\\$")}\\s*(?:(?:[-+*/%&|^@]|\\/\\/|\\*\\*|<<|>>>?|\\?\\?|&&|\\|\\|)=(?!=)|\\+\\+|--)|(?:\\+\\+|--)\\s*${name.replace("$", "\\$")}\\b`).test(text);
    const readFirst = inside.find((one) => one.name === name && !(one.line === setAt && lang.sets(doc.line(one.line)).includes(name) && doc.line(one.line).indexOf(name) === one.col && !steps(doc.line(one.line))));
    if (setAt !== undefined && (!readFirst || readFirst.line > setAt)) {
      continue;
    }
    takes.push(name);
  }
  const after =head >= 0 || !expression ? namesIn(s, last + 1, Math.max(last + 1, end - 1)) : [];
  const gives = [...setsInside.keys()].filter((name) => after.some((found) => found.name === name));
  return { s, doc, size, from, to, expression, first, last, value, selected, code, head, params, end, inside, setsInside, takes, after, gives };
}

export function extractFunction(editor, tab = null) {
  const id = editor.s.language?.id;
  const lang = LANGUAGES[id];
  if (!lang) {
    say(`Extract Function has no form for ${id ?? "this file"}.`);
    return;
  }
  const read = readSelection(editor, lang);
  if (!read) {
    return;
  }
  if (lang.typed) {
    return extractTyped(editor, tab, id, lang, read);
  }
  const { s, doc, from, to, expression, first, last, value, selected, code, head, end, takes, gives } = read;
  const unit = s.indent.tabs ? "\t" : " ".repeat(s.indent.size);
  if (gives.length > 1 && id !== "python") {
    say(`The lines selected set ${gives.join(" and ")}, each read after them, and a JavaScript function gives back one value.`);
    return;
  }
  const isAsync = /(^|[^\w$.])await\b/.test(code);

  // The new function's lines, its body set one step in from its definition.
  const name = freshName(doc.text());
  let body;
  if (expression) {
    body = [lang.expression(value)];
  } else {
    const least = Math.min(...selected.filter((text) => text.trim()).map((text) => indentOf(text).length));
    body = selected.map((text) => (text.trim() ? text.slice(least) : ""));
    if (gives.length) {
      body.push(lang.giveBack(gives));
    }
  }
  const definition = lang.define(name, takes, body, unit, isAsync);
  const call = lang.call(name, takes, isAsync);
  const declaredWith = gives.length === 1 ? selected.join("\n").match(new RegExp(`^\\s*(const|let|var)\\s+${gives[0].replace("$", "\\$")}\\b`, "m"))?.[1] ?? null : null;
  const callLine = gives.length ? lang.assign(gives, call, declaredWith) : lang.statement(call);

  // Where the definition goes: after the outermost definition that holds the selection, or above the
  // selection at the top of the file.
  let insertAt;
  let text;
  if (head < 0) {
    // Above the statement at the file's left edge that holds the selection.
    insertAt = first;
    while (insertAt > 0 && (indentOf(doc.line(insertAt)).length > 0 || !doc.line(insertAt).trim())) {
      insertAt -= 1;
    }
    text = `${definition}${lang.gap}`;
  } else {
    let outer = head;
    while (outer > 0 && indentOf(doc.line(outer)).length > 0) {
      outer -= 1;
    }
    let close = doc.count;
    for (let line = last + 1; line < doc.count; line += 1) {
      const here = doc.line(line);
      if (here.trim() && indentOf(here).length === 0) {
        close = id === "javascript" && here.startsWith("}") ? line + 1 : line;
        break;
      }
    }
    // After the last line of the outer definition that is not blank.
    let lastFilled = close - 1;
    while (lastFilled > last && !doc.line(lastFilled).trim()) {
      lastFilled -= 1;
    }
    insertAt = lastFilled + 1;
    text = `${lang.gap}${definition}`;
    if (insertAt >= doc.count) {
      insertAt = doc.count;
    }
  }
  const defineAt = text.indexOf(`${isAsync ? "async " : ""}${id === "python" ? "def" : "function"} ${name}`);
  const linesBefore = text.slice(0, defineAt).split("\n").length - 1;
  const nameCol = (isAsync ? "async ".length : 0) + (id === "python" ? "def " : "function ").length;
  const callLines = expression ? null : `${indentOf(doc.line(first))}${callLine}`;
  write(editor, { first, last, from, to, expression, call, callLines, callAt: expression ? call.indexOf(name) : callLines.indexOf(name, indentOf(doc.line(first)).length + (gives.length ? callLine.indexOf("=") : 0)), insertAt, text, linesBefore, nameCol, name, insertFirst: head < 0 });
}

// Writes the call over the selection and the definition at `insertAt`, as one change, and selects
// the new name in both.
export function write(editor, { first, last, from, to, expression, call, callLines, callAt, insertAt, text, linesBefore, nameCol, name, insertFirst }) {
  const doc = editor.s.doc;
  const replaced = expression
    ? { from: pos(first, from.col), to: pos(first, to.col), text: call }
    : { from: pos(first, 0), to: last + 1 < doc.count ? pos(last + 1, 0) : pos(last, doc.line(last).length), text: `${callLines}${last + 1 < doc.count ? "\n" : ""}` };
  const atEnd = insertAt >= doc.count;
  const inserted = atEnd
    ? { from: pos(doc.count - 1, doc.line(doc.count - 1).length), to: pos(doc.count - 1, doc.line(doc.count - 1).length), text: `\n${text.replace(/\n$/, "")}` }
    : { from: pos(insertAt, 0), to: pos(insertAt, 0), text };
  const edits = insertFirst ? [inserted, replaced] : [replaced, inserted];
  const linesAbove = linesBefore + (atEnd ? 1 : 0);
  editor.change(edits, null, (map) => {
    const start = map(inserted.from, false);
    const defined = pos(start.line + linesAbove, (atEnd || linesAbove ? 0 : start.col) + nameCol);
    const placed = map(replaced.from, false);
    const callCol = (expression ? placed.col : 0) + callAt;
    return [
      { anchor: defined, head: pos(defined.line, defined.col + name.length), goal: null },
      { anchor: pos(placed.line, callCol), head: pos(placed.line, callCol + name.length), goal: null },
    ];
  });
  editor.focus();
}
