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
import { inferred, statementStart } from "./refactor.js";
import { hoverAt } from "./servers.js";
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

const RUST_KEEPS = new Set(
  (
    "as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while " +
    "bool char str i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 String Vec Box Option Result Some None Ok Err"
  ).split(" "),
);

const C_KEEPS = new Set(
  (
    "auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while " +
    "bool true false nullptr NULL this new delete class public private protected template typename namespace using operator virtual override final constexpr noexcept throw try catch " +
    "size_t ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t std __auto_type"
  ).split(" "),
);

// The names a parameter list holds: each part's last name in the C family, the name before its `:`
// in Rust, and the name before any default or type in Python and JavaScript, spreads set aside.
// Rust's `self` in any of its forms is no name the selection takes.
const nameOfPart = {
  default: (part) => part.trim().replace(/^\*{1,2}|^\.\.\./, "").split(/[=:]/)[0].trim(),
  rust: (part) => (/^&?\s*(?:'\w+\s+)?(?:mut\s+)?self\b/.test(part.trim()) ? "" : part.trim().replace(/^mut\s+/, "").split(":")[0].trim()),
  c: (part) => part.replace(/\[[^\]]*\]/g, "").split("=")[0].match(/([A-Za-z_]\w*)\s*\)?\s*$/)?.[1] ?? "",
};

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
  rust: {
    typed: true,
    keeps: RUST_KEEPS,
    part: nameOfPart.rust,
    heads: [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?(?:const\s+)?(?:async\s+)?(?:unsafe\s+)?(?:extern\s+"[^"]*"\s+)?fn\s+[A-Za-z_]\w*\s*(?:<[^>]*>)?\s*\((.*)\)\s*(?:->[^{]*?)?\s*(?:where\b[^{]*)?\{?\s*$/],
    // What a line sets: the names a `let` binds, one or a tuple of them, the name before an `=`, and
    // those a for loop takes.
    sets: (line) => {
      const names = [];
      const bound = line.match(/^\s*let\s+(?:mut\s+)?([A-Za-z_]\w*)/);
      if (bound) {
        names.push(bound[1]);
      }
      const tuple = line.match(/^\s*let\s+(?:mut\s+)?\(([^)]*)\)/);
      if (tuple) {
        names.push(...tuple[1].split(",").map((name) => name.trim().replace(/^mut\s+/, "")).filter((name) => /^[A-Za-z_]\w*$/.test(name)));
      }
      const assigned = line.match(/^\s*([A-Za-z_]\w*)\s*(?:[-+*/%&|^]|<<|>>)?=(?![=>])/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const loop = line.match(/^\s*for\s+\(?\s*([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s*\)?\s+in\b/);
      if (loop) {
        names.push(...loop[1].split(",").map((name) => name.trim()));
      }
      return names;
    },
    // A name's type as rust-analyzer's hover gives it, `let mut name: Type` or `name: Type`, and
    // whether it is declared `mut`; null for a name that is no local and no parameter.
    typeIn: (said, name) => {
      for (const block of said.matchAll(/```rust\n([\s\S]*?)\n```/g)) {
        for (const line of block[1].split("\n")) {
          const found = line.match(new RegExp(`^(?:let\\s+)?(mut\\s+)?(?:ref\\s+)?${name}\\s*:\\s*(.+?)\\s*$`));
          if (found) {
            return { type: found[2], mut: Boolean(found[1]) };
          }
        }
      }
      return null;
    },
    probe: (name, value) => `let ${name} = ${value};`,
  },
  c: {
    typed: true,
    keeps: C_KEEPS,
    part: nameOfPart.c,
    heads: [/^(\s*)(?!(?:if|for|while|switch|return|else|do|case|sizeof)\b)(?:[A-Za-z_][\w:<>,]*[\s*&]+)+[A-Za-z_~][\w:~]*\s*\(([^;]*)\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{?\s*$/],
    // What a line sets: the name a declaration declares, the name before an `=`, and one stepped by
    // `++` or `--`.
    sets: (line) => {
      const names = [];
      const declared = line.match(/^\s*(?:for\s*\(\s*)?(?!(?:return|else|case|goto|delete|throw)\b)(?:(?:const|static|unsigned|signed|long|short|struct|enum|volatile|register|auto|__auto_type)\s+)*[A-Za-z_][\w:]*(?:\s*<[^;=]*>)?[\s*&]+([A-Za-z_]\w*)\s*(?:=|;|\[|\{)/);
      if (declared) {
        names.push(declared[1]);
      }
      const assigned = line.match(/^\s*([A-Za-z_]\w*)\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const stepped = line.match(/^\s*(?:\+\+|--)?([A-Za-z_]\w*)\s*(?:\+\+|--)?\s*;/);
      if (stepped && /\+\+|--/.test(line)) {
        names.push(stepped[1]);
      }
      return names;
    },
    // A name's type as clangd's hover gives it, its `Type:`, for a parameter or a variable local to
    // a function; a field marked as one; null for any other name.
    typeIn: (said) => {
      const kind = said.match(/^###\s+([\w ]+?)\s+`/m)?.[1] ?? "";
      const type = said.match(/Type:\s*`([^`]+)`/)?.[1] ?? null;
      if (kind === "field") {
        return { field: true };
      }
      const local = kind === "param" || (kind.endsWith("variable") && /\/\/ In (?!namespace\b)/.test(said));
      return local && type ? { type } : null;
    },
  },
};

const indentOf = (text) => text.match(/^\s*/)[0];
const widthOf = (text, size) => [...indentOf(text)].reduce((sum, char) => sum + (char === "\t" ? size : 1), 0);

// The names a function's parameter list holds.
function paramsOf(list, part = nameOfPart.default) {
  const names = [];
  let depth = 0;
  let held = "";
  for (const char of `${list},`) {
    if ("([{<".includes(char)) {
      depth += 1;
    } else if (")]}>".includes(char)) {
      depth -= 1;
    }
    if (char === "," && depth === 0) {
      const name = part(held);
      if (/^[A-Za-z_$][\w$]*$/.test(name) && name !== "void") {
        names.push(name);
      }
      held = "";
    } else {
      held += char;
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
      const kind = s.highlight.classAt(line, col);
      if (/t-comment/.test(kind) || (/t-string/.test(kind) && !coded.some(([a, b]) => a <= col && col < b))) {
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
function freshName(text) {
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
function write(editor, { first, last, from, to, expression, call, callLines, callAt, insertAt, text, linesBefore, nameCol, name, insertFirst }) {
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

// A Rust type that a value of it is copied as and not moved: a number, a bool, a char, a shared
// reference, a raw pointer, the unit, and a fixed-length array whose items are numbers.
const copied = (type) => /^(?:[iu](?:8|16|32|64|128|size)|f32|f64|bool|char|\(\)|&(?!mut\b).*|\*(?:const|mut) .*)$/.test(type.trim()) || /^\[(?:[iu](?:8|16|32|64|128|size)|f32|f64|bool|char); \d+\]$/.test(type.trim());

// A C declaration of `name` with `type`: the name goes inside a pointer to a function's `(*)`,
// before an array's brackets, and after a pointer's or a reference's mark with no space.
function declared(type, name) {
  if (type.includes("(*)")) {
    return type.replace("(*)", `(*${name})`);
  }
  const bracket = type.indexOf("[");
  if (bracket >= 0) {
    return `${type.slice(0, bracket).trimEnd()} ${name}${type.slice(bracket)}`;
  }
  return /[*&]$/.test(type) ? `${type}${name}` : `${type} ${name}`;
}

// The selection's own characters at `line` that stand outside strings and comments and match
// `pattern`.
function codeMatches(s, line, text, pattern) {
  return [...text.matchAll(pattern)].filter((found) => !/t-string|t-comment/.test(s.highlight.classAt(line, found.index + found[0].length - 1)));
}

async function extractTyped(editor, tab, id, lang, read) {
  const { s, doc, from, to, expression, first, last, value, selected, code, head, end, inside, setsInside, takes, gives } = read;
  const unit = s.indent.tabs ? "\t" : " ".repeat(s.indent.size);
  const where = id === "rust" ? "Rust" : "the C family";
  if (!tab?.served) {
    say(`Extract Function in ${where} takes its types from the language server, and no server serves this file.`);
    return;
  }
  if (head < 0) {
    say("The selection stands in no function. Extract Function takes lines or an expression inside one.");
    return;
  }
  const handsUp = selected.some((text, at) => codeMatches(s, first + at, text, /[\w)\]>]\?/g).some((found) => !expression || (found.index >= from.col && found.index < to.col)));
  if (id === "rust" && handsUp) {
    say("The selection hands an error up with ?, which returns from its function. Extract Function takes lines that run through to their end.");
    return;
  }
  const original = doc.text();
  const headText = doc.line(head);
  const headIndent = indentOf(headText);
  const isC = id === "c" && /\.[ch]$/i.test(tab.file ?? "");

  // Each name's type, from the server's hover where the selection first reads or sets it.
  const kinds = new Map();
  const ask = async (name, line, col) => {
    if (!kinds.has(name)) {
      const said = await hoverAt(tab, pos(line, col));
      kinds.set(name, said ? lang.typeIn(said, name) : null);
    }
    return kinds.get(name);
  };
  const taken = [];
  for (const name of takes) {
    const at = inside.find((one) => one.name === name);
    const kind = await ask(name, at.line, at.col);
    if (kind?.field) {
      if (headIndent) {
        continue;
      }
      say(`The selection reads ${name}, a field, which a function outside the class does not reach.`);
      return;
    }
    if (kind) {
      taken.push({ name, ...kind });
    }
  }
  // In a method defined outside its class, a field the selection reads is reached through the
  // object the method is called on, which the new function has not.
  if (id === "c" && !headIndent && /::/.test(headText)) {
    for (const one of inside) {
      if (!lang.keeps.has(one.name) && !one.called && !setsInside.has(one.name) && !takes.includes(one.name)) {
        if ((await ask(one.name, one.line, one.col))?.field) {
          say(`The selection reads ${one.name}, a field, which a function outside the class does not reach.`);
          return;
        }
      }
    }
  }
  const given = [];
  for (const name of gives) {
    const line = setsInside.get(name);
    const col = doc.line(line).search(new RegExp(`\\b${name}\\b`));
    const kind = await ask(name, line, col);
    if (!kind?.type || /\{unknown\}|\{closure/.test(kind.type)) {
      say(`The language server gives no type for ${name}, which the selection sets and its function reads after it.`);
      return;
    }
    given.push({ name, ...kind, declared: id === "rust" ? new RegExp(`^\\s*let\\s+(?:mut\\s+)?(?:\\([^)]*\\b)?${name}\\b`).test(doc.line(line)) : lang.sets(doc.line(line))[0] === name && !new RegExp(`^\\s*${name}\\b`).test(doc.line(line)) });
  }
  const name = freshName(original);

  // An expression's type: the server is shown the text with the expression bound to the new name
  // above the statement that holds it, inferred, and asked of that name.
  let valueType = null;
  if (expression) {
    const top = statementStart(doc, first);
    const indent = indentOf(doc.line(top));
    const binding = id === "rust" ? lang.probe(name, value) : `${inferred(tab.file)} ${name} = (${value});`;
    const lines = original.split("\n");
    lines.splice(top, 0, `${indent}${binding}`);
    const said = await hoverAt(tab, pos(top, indent.length + binding.indexOf(name)), lines.join("\n"));
    valueType = said ? lang.typeIn(said, name)?.type : null;
    if (!valueType || /\{unknown\}|\{closure/.test(valueType)) {
      say("The language server gives no type for the expression selected.");
      return;
    }
  }
  if (doc.text() !== original) {
    say("The file changed while the types were read. Extract Function again.");
    return;
  }
  if (id === "c" && given.length > 1) {
    say(`The lines selected set ${gives.join(" and ")}, each read after them, and a C function gives back one value.`);
    return;
  }
  if (id === "c" && given.some((one) => one.type.includes("["))) {
    say("The lines selected set an array that their function reads after them, and a C function gives back no array.");
    return;
  }
  if (id === "rust" && given.length && given.some((one) => one.declared) && !given.every((one) => one.declared)) {
    say("The lines selected declare some of what their function reads after them and change the rest. Extract Function takes lines that do one or the other.");
    return;
  }

  // How each parameter is taken: in Rust by value, by reference or by a mutable reference; in the C
  // family as declared, or by reference in C++ where the selection writes into a value it takes.
  const reaches = (one) => selected.some((text, at) => codeMatches(s, first + at, text, new RegExp(`\\b${one.name}\\s*(?:\\.|\\[)`, "g")).length);
  const writesInto = (one) => selected.some((text, at) => codeMatches(s, first + at, text, new RegExp(`\\b${one.name}\\s*(?:\\.[\\w.]+|\\[[^\\]]*\\])\\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)`, "g")).length);
  const readAfter = (one) => read.after.some((found) => found.name === one.name);
  const params = [];
  const args = [];
  for (const one of taken) {
    if (id === "rust") {
      const sets = setsInside.has(one.name);
      if (sets || copied(one.type) || !readAfter(one)) {
        params.push(`${sets || (one.mut && reaches(one)) ? "mut " : ""}${one.name}: ${one.type}`);
        args.push(one.name);
      } else if (one.mut && reaches(one)) {
        params.push(`${one.name}: &mut ${one.type}`);
        args.push(`&mut ${one.name}`);
      } else {
        params.push(`${one.name}: &${one.type}`);
        args.push(`&${one.name}`);
      }
      continue;
    }
    const plain = !/[*[&]/.test(one.type);
    if (plain && writesInto(one) && readAfter(one)) {
      if (isC) {
        say(`The lines selected write into ${one.name}, which a C function takes as a copy, and their function reads it after them.`);
        return;
      }
      params.push(declared(`${one.type} &`, one.name));
    } else {
      params.push(declared(one.type, one.name));
    }
    args.push(one.name);
  }

  // A Rust method where the selection reads `self`, called on `self`; an associated function in an
  // impl, called through `Self`.
  let receiver = "";
  let callee = name;
  if (id === "rust") {
    let holder = head - 1;
    while (holder >= 0 && (!doc.line(holder).trim() || widthOf(doc.line(holder), s.indent.size) >= widthOf(headText, s.indent.size))) {
      holder -= 1;
    }
    const inImpl = headIndent.length > 0 && holder >= 0 && /^\s*(?:unsafe\s+)?impl\b/.test(doc.line(holder));
    const readsSelf = (expression ? [value] : selected).some((text) => /\bself\b/.test(text));
    if (readsSelf && !inImpl) {
      say("The selection reads self outside an impl. Extract Function takes a selection whose self it can pass on.");
      return;
    }
    if (readsSelf) {
      const own = headText.match(/\(\s*(&\s*(?:'\w+\s+)?(?:mut\s+)?self|(?:mut\s+)?self)\b/)?.[1] ?? "&self";
      receiver = own.startsWith("&") ? own.replace(/\s+/g, " ").replace("& ", "&") : "&self";
      callee = `self.${name}`;
    } else if (inImpl) {
      callee = `Self::${name}`;
    }
  }

  const isAsync = id === "rust" && /\.await\b/.test(code);
  const inner = headIndent + unit;
  const least = expression ? 0 : Math.min(...selected.filter((text) => text.trim()).map((text) => indentOf(text).length));
  const body = expression ? [] : selected.map((text) => (text.trim() ? inner + text.slice(least) : ""));
  let returns = "";
  if (expression) {
    returns = valueType;
    body.push(id === "rust" ? `${inner}${value}` : `${inner}return ${value};`);
  } else if (given.length) {
    returns = given.length > 1 ? `(${given.map((one) => one.type).join(", ")})` : given[0].type;
    body.push(id === "rust" ? `${inner}${given.length > 1 ? `(${gives.join(", ")})` : gives[0]}` : `${inner}return ${gives[0]};`);
  }
  const list = [receiver, ...params].filter(Boolean).join(", ");
  let opening;
  if (id === "rust") {
    opening = `${headIndent}${isAsync ? "async " : ""}fn ${name}(${list})${returns ? ` -> ${returns}` : ""} {`;
  } else {
    const allman = doc.line(head + 1)?.trim() === "{" || !/\{\s*$/.test(headText);
    const signature = `${headIndent}${headIndent ? "" : "static "}${declared(returns || "void", name)}(${list || (isC ? "void" : "")})`;
    opening = allman ? `${signature}\n${headIndent}{` : `${signature} {`;
  }
  const definition = `${opening}\n${body.join("\n")}\n${headIndent}}\n`;

  // The call, and what takes what the new function gives back.
  const args0 = args.join(", ");
  let call = `${callee}(${args0})${isAsync ? ".await" : ""}`;
  let callLine;
  if (expression) {
    callLine = null;
  } else if (!given.length) {
    callLine = `${call};`;
  } else if (id === "rust") {
    const names = given.map((one) => `${one.declared && one.mut ? "mut " : ""}${one.name}`);
    const bound = given.length > 1 ? `(${names.join(", ")})` : names[0];
    callLine = given[0].declared ? `let ${bound} = ${call};` : `${given.length > 1 ? `(${gives.join(", ")})` : gives[0]} = ${call};`;
  } else {
    callLine = given[0].declared ? `${declared(given[0].type, gives[0])} = ${call};` : `${gives[0]} = ${call};`;
  }

  // Where the definition goes: after the function that holds the selection in Rust and in a C++
  // class, and above it, past the comments that lead into it, at the C family's file level.
  let insertAt;
  let text;
  if (id === "rust" || headIndent) {
    insertAt = end + 1;
    text = `\n${definition}`;
  } else {
    insertAt = head;
    while (insertAt > 0 && /^\s*(?:\/\/|\/\*|\*|template\s*<|\[\[)/.test(doc.line(insertAt - 1))) {
      insertAt -= 1;
    }
    text = `${definition}\n`;
  }
  if (insertAt > doc.count) {
    insertAt = doc.count;
  }
  const linesBefore = id === "rust" || headIndent ? 1 : 0;
  const nameCol = opening.split("\n")[0].indexOf(`${name}(`);
  const indent = indentOf(doc.line(first));
  const callLines = expression ? null : `${indent}${callLine}`;
  const callAt = expression ? call.indexOf(name) : callLines.indexOf(name, indent.length + (given.length ? callLine.indexOf("=") : 0));
  write(editor, { first, last, from, to, expression, call, callLines, callAt, insertAt, text, linesBefore, nameCol, name, insertFirst: insertAt <= first });
}
