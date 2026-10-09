// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Extract Function, in Python and JavaScript: the lines selected, or an expression on one line,
// become a function of their own, called where they stood, in one change that undo takes back.
//
// The new function takes as parameters the names the selection reads that the function holding it
// had set before it: its own parameters and what its lines above the selection set. It gives back
// the names the selection sets that its function reads after it, as one value, or in Python as a
// tuple of them. It goes after the outermost definition that holds the selection, or above the
// selection where the selection stands at the top of the file. Its name is selected where it is
// defined and where it is called: the name typed next replaces both.
//
// A selection that returns, or that breaks out of a loop it does not hold, stays where it is. So
// does one in Rust or the C family, whose parameters need types orior does not read.

import { pos } from "./editor/document.js";
import { endOfSel, startOf } from "./editor/view.js";
import { say } from "./statusbar.js";

const PYTHON_KEEPS = new Set(
  (
    "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield " +
    "abs all any bool bytes callable chr dict dir divmod enumerate filter float format frozenset getattr hasattr hash hex id input int isinstance issubclass iter len list map max min next object oct open ord pow print range repr reversed round set setattr slice sorted str sum super tuple type vars zip " +
    "Exception ValueError TypeError KeyError IndexError RuntimeError StopIteration OSError NotImplementedError self cls"
  ).split(" "),
);
PYTHON_KEEPS.delete("self");
PYTHON_KEEPS.delete("cls");

const SCRIPT_KEEPS = new Set(
  (
    "await break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof undefined var void while with yield async " +
    "Array Boolean Date Error JSON Map Math Number Object Promise RegExp Set String Symbol WeakMap console document globalThis window Infinity NaN isNaN parseFloat parseInt"
  ).split(" "),
);

const LANGUAGES = {
  python: {
    keeps: PYTHON_KEEPS,
    heads: [/^(\s*)(?:async\s+)?def\s+\w+\s*\((.*)\)\s*(?:->[^:]*)?:\s*$/],
    tops: /^(?:async\s+def|def|class)\b/,
    // What a line sets: the names before its `=`, those a for loop takes, and the name after `as`.
    sets: (line) => {
      const names = [];
      const assigned = line.match(/^\s*([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s*(?:[-+*/%&|^@]|\/\/|\*\*|>>|<<)?=(?!=)/);
      if (assigned) {
        names.push(...assigned[1].split(",").map((name) => name.trim()));
      }
      const loop = line.match(/^\s*(?:async\s+)?for\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s+in\b/);
      if (loop) {
        names.push(...loop[1].split(",").map((name) => name.trim()));
      }
      for (const found of line.matchAll(/\bas\s+([A-Za-z_]\w*)/g)) {
        names.push(found[1]);
      }
      return names;
    },
    define: (name, params, body, unit, isAsync) => `${isAsync ? "async " : ""}def ${name}(${params.join(", ")}):\n${body.map((line) => (line ? unit + line : "")).join("\n")}\n`,
    giveBack: (names) => `return ${names.join(", ")}`,
    expression: (value) => `return ${value}`,
    call: (name, params, isAsync) => `${isAsync ? "await " : ""}${name}(${params.join(", ")})`,
    assign: (names, call) => `${names.join(", ")} = ${call}`,
    statement: (call) => call,
    gap: "\n\n",
  },
  javascript: {
    keeps: SCRIPT_KEEPS,
    heads: [
      /^(\s*)(?:export\s+)?(?:default\s+)?(?:async\s+)?function\s*\*?\s*[\w$]*\s*\(([^)]*)\)\s*\{\s*$/,
      /^(\s*)(?:static\s+)?(?:async\s+)?(?!(?:if|for|while|switch|catch|function)\b)[\w$]+\s*\(([^)]*)\)\s*\{\s*$/,
      /^(\s*)(?:export\s+)?(?:const|let|var)\s+[\w$]+\s*=\s*(?:async\s*)?\(([^)]*)\)\s*=>\s*\{\s*$/,
    ],
    tops: /^(?:export\s+)?(?:default\s+)?(?:async\s+)?(?:function|class|const|let|var)\b/,
    sets: (line) => {
      const names = [];
      const declared = line.match(/^\s*(?:const|let|var)\s+([A-Za-z_$][\w$]*)/);
      if (declared) {
        names.push(declared[1]);
      }
      const assigned = line.match(/^\s*([A-Za-z_$][\w$]*)\s*(?:[-+*/%&|^]|\*\*|>>>?|<<|\?\?|&&|\|\|)?=(?!=)/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const loop = line.match(/^\s*for\s*\(\s*(?:const|let|var)\s+([A-Za-z_$][\w$]*)/);
      if (loop) {
        names.push(loop[1]);
      }
      return names;
    },
    define: (name, params, body, unit, isAsync) => `${isAsync ? "async " : ""}function ${name}(${params.join(", ")}) {\n${body.map((line) => (line ? unit + line : "")).join("\n")}\n}\n`,
    giveBack: (names) => `return ${names[0]};`,
    expression: (value) => `return ${value};`,
    call: (name, params, isAsync) => `${isAsync ? "await " : ""}${name}(${params.join(", ")})`,
    assign: (names, call, declared) => `${declared ? `${declared} ` : ""}${names[0]} = ${call};`,
    statement: (call) => `${call};`,
    gap: "\n",
  },
};

const indentOf = (text) => text.match(/^\s*/)[0];
const widthOf = (text, size) => [...indentOf(text)].reduce((sum, char) => sum + (char === "\t" ? size : 1), 0);

// The names a function's parameter list holds, defaults, types and spreads set aside.
function paramsOf(list) {
  const names = [];
  let depth = 0;
  let part = "";
  for (const char of `${list},`) {
    if ("([{".includes(char)) {
      depth += 1;
    } else if (")]}".includes(char)) {
      depth -= 1;
    }
    if (char === "," && depth === 0) {
      const name = part.trim().replace(/^\*{1,2}|^\.\.\./, "").split(/[=:]/)[0].trim();
      if (/^[A-Za-z_$][\w$]*$/.test(name)) {
        names.push(name);
      }
      part = "";
    } else {
      part += char;
    }
  }
  return names;
}

// The columns of a line that are code inside a string: what the braces of a Python f-string hold,
// and what ${ } holds in a JavaScript template.
function codeInStrings(text) {
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
// stands and whether it is called there or named as a keyword argument.
function namesIn(s, from, to, cols = null) {
  const found = [];
  for (let line = from; line <= to; line += 1) {
    const text = s.doc.line(line);
    const start = line === from && cols ? cols.from : 0;
    const end = line === to && cols ? cols.to : text.length;
    const coded = codeInStrings(text);
    for (const match of text.slice(start, end).matchAll(/[A-Za-z_$][\w$]*/g)) {
      const col = start + match.index;
      const kind = s.highlight.classAt(line, col);
      if (/t-comment/.test(kind) || (/t-string/.test(kind) && !coded.some(([a, b]) => a <= col && col < b))) {
        continue;
      }
      const before = text.slice(0, col).trimEnd();
      if (before.endsWith(".") && !before.endsWith("..")) {
        continue;
      }
      const after = text.slice(col + match[0].length);
      const called = /^\s*\(/.test(after);
      const keyword = /^\s*=(?!=)/.test(after) && /[(,]\s*$/.test(before);
      found.push({ name: match[0], line, col, called, keyword });
    }
  }
  return found;
}

// The new function's name: `extracted`, with a number after it where the file uses that already.
function freshName(text) {
  let name = "extracted";
  for (let count = 2; new RegExp(`(?<![\\w$.])${name}\\b`).test(text); count += 1) {
    name = `extracted${count}`;
  }
  return name;
}

export function extractFunction(editor) {
  const s = editor.s;
  const id = s.language?.id;
  const lang = LANGUAGES[id];
  if (!lang) {
    say(id === "rust" || id === "c" ? "Extract Function in Rust and the C family needs the parameters' types, and orior does not read them yet." : `Extract Function has no form for ${id ?? "this file"}.`);
    return;
  }
  const doc = s.doc;
  const size = s.indent.size;
  const unit = s.indent.tabs ? "\t" : " ".repeat(size);
  const sel = s.selections[s.primary];
  const from = startOf(sel);
  const to = endOfSel(sel);
  if (from.line === to.line && from.col === to.col) {
    say("Select the lines or the expression for Extract Function.");
    return;
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
    return;
  }
  if (/(^|[^\w$.])return\b/.test(code) && !expression) {
    say("The lines selected return from their function. Extract Function takes lines that run through to their end.");
    return;
  }
  if (/^\s*(?:break|continue)\b/m.test(code) && !/^\s*(?:for|while|do)\b/m.test(code)) {
    say("The lines selected leave a loop they do not hold. Extract Function takes lines that run through to their end.");
    return;
  }
  const firstWidth = widthOf(doc.line(first), size);

  // The function that holds the selection: its first line, its parameters, and where it ends. Up
  // from the selection, each line less indented than all below it opens a block that holds the
  // selection, and the first of them that heads a function holds it. One at the left edge that
  // heads none leaves the selection outside any function.
  let head = -1;
  let params = [];
  let within = firstWidth + (expression ? 1 : 0);
  for (let line = first - (expression ? 0 : 1); line >= 0; line -= 1) {
    const text = doc.line(line);
    const width = widthOf(text, size);
    if (!text.trim() || width >= within) {
      continue;
    }
    const found = lang.heads.map((pattern) => text.match(pattern)).find(Boolean);
    if (found) {
      head = line;
      params = paramsOf(found[2]);
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
    // A name the selection sets on a line above any it reads it on is not taken in.
    const setAt = setsInside.get(name);
    const readFirst = inside.find((one) => one.name === name && !(one.line === setAt && lang.sets(doc.line(one.line)).includes(name) && doc.line(one.line).indexOf(name) === one.col));
    if (setAt !== undefined && (!readFirst || readFirst.line > setAt)) {
      continue;
    }
    takes.push(name);
  }
  const after = head >= 0 || !expression ? namesIn(s, last + 1, Math.max(last + 1, end - 1)) : [];
  const gives = [...setsInside.keys()].filter((name) => after.some((found) => found.name === name));
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

  const indent = indentOf(doc.line(first));
  const replaced = expression
    ? { from: pos(first, from.col), to: pos(first, to.col), text: call }
    : { from: pos(first, 0), to: last + 1 < doc.count ? pos(last + 1, 0) : pos(last, doc.line(last).length), text: `${indent}${callLine}${last + 1 < doc.count ? "\n" : ""}` };
  const atEnd = insertAt >= doc.count;
  const inserted = atEnd
    ? { from: pos(doc.count - 1, doc.line(doc.count - 1).length), to: pos(doc.count - 1, doc.line(doc.count - 1).length), text: `\n${text.replace(/\n$/, "")}` }
    : { from: pos(insertAt, 0), to: pos(insertAt, 0), text };
  const edits = head < 0 ? [inserted, replaced] : [replaced, inserted];
  const defineAt = text.indexOf(`${isAsync ? "async " : ""}${id === "python" ? "def" : "function"} ${name}`);
  const linesBefore = (atEnd ? `\n${text}` : text).slice(0, defineAt + (atEnd ? 1 : 0)).split("\n").length - 1;
  const nameCol = (isAsync ? "async ".length : 0) + (id === "python" ? "def " : "function ").length;
  editor.change(edits, null, (map) => {
    const start = map(inserted.from, false);
    const defined = pos(start.line + linesBefore, (atEnd ? 0 : start.col) + nameCol);
    const placed = map(replaced.from, false);
    const callText = expression ? call : `${indent}${callLine}`;
    const callCol = (expression ? placed.col : 0) + callText.indexOf(name, expression ? 0 : indent.length + (gives.length ? callLine.indexOf("=") : 0));
    return [
      { anchor: defined, head: pos(defined.line, defined.col + name.length), goal: null },
      { anchor: pos(placed.line, callCol), head: pos(placed.line, callCol + name.length), goal: null },
    ];
  });
  editor.focus();
}
