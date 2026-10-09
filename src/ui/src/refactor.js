// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The refactorings orior makes itself in the file open, each one change that undo takes back whole,
// and each working where no language server offers one: Extract Variable, Extract Constant and
// Inline Variable. Each reads the code by its lines in the file's language, as the outline does.
//
// Extract Variable puts the expression selected on one line into a variable declared on a line of
// its own above the statement that held it. Extract Constant puts it into a constant declared at the
// head of the file, after what the file brings in. Both leave the new name selected where it is
// declared and where it is used: the name typed next replaces both. Inline Variable takes a
// variable declared on one line, puts its value where each use of it stands, in parentheses where the
// value is more than a name, a call or a literal, and takes the declaration out.

import { cmp, pos } from "./editor/document.js";
import { endOfSel, startOf } from "./editor/view.js";
import { say } from "./statusbar.js";

// The C family's type that stands for whatever the value is: C++ and CUDA infer it with auto, and C
// with GNU C's __auto_type.
const inferred = (path) => (/\.[ch]$/i.test(path ?? "") ? "__auto_type" : "auto");

// Each language's forms: how a variable and a constant are declared, how a name is written where
// it is used, the case its names take, and how a line that declares `name` reads.
const FORMS = {
  javascript: {
    variable: (name, value) => `const ${name} = ${value};`,
    constant: (name, value) => `const ${name} = ${value};`,
    declares: (name) => new RegExp(`^(\\s*)(?:export\\s+)?(?:const|let|var)\\s+${name}\\s*=\\s*(.+?);?\\s*$`),
    camel: true,
  },
  python: {
    variable: (name, value) => `${name} = ${value}`,
    constant: (name, value) => `${name} = ${value}`,
    declares: (name) => new RegExp(`^(\\s*)${name}\\s*(?::[^=]*)?=(?!=)\\s*(.+?)\\s*$`),
  },
  rust: {
    variable: (name, value) => `let ${name} = ${value};`,
    constant: (name, value) => {
      const type = rustTypeOf(value);
      return type ? `const ${name}: ${type} = ${value};` : null;
    },
    declares: (name) => new RegExp(`^(\\s*)let\\s+(?:mut\\s+)?${name}(?:\\s*:\\s*[^=]+)?\\s*=\\s*(.+?);\\s*$`),
  },
  c: {
    variable: (name, value, path) => `${inferred(path)} ${name} = ${value};`,
    constant: (name, value) => `#define ${name} (${value})`,
    declares: (name) => new RegExp(`^(\\s*)(?!return\\b|if\\b|while\\b)[A-Za-z_][\\w\\s*:<>,]*?\\b${name}\\s*=\\s*(.+?);\\s*$`),
  },
  ruby: {
    variable: (name, value) => `${name} = ${value}`,
    constant: (name, value) => `${name} = ${value}`,
    declares: (name) => new RegExp(`^(\\s*)${name}\\s*=(?!=)\\s*(.+?)\\s*$`),
  },
  r: {
    variable: (name, value) => `${name} <- ${value}`,
    constant: (name, value) => `${name} <- ${value}`,
    declares: (name) => new RegExp(`^(\\s*)${name}\\s*(?:<-|=(?!=))\\s*(.+?)\\s*$`),
  },
  matlab: {
    variable: (name, value) => `${name} = ${value};`,
    constant: (name, value) => `${name} = ${value};`,
    declares: (name) => new RegExp(`^(\\s*)${name}\\s*=(?!=)\\s*(.+?);?\\s*$`),
  },
  powershell: {
    variable: (name, value) => `$${name} = ${value}`,
    constant: (name, value) => `$${name} = ${value}`,
    used: (name) => `$${name}`,
    declares: (name) => new RegExp(`^(\\s*)\\$${name}\\s*=\\s*(.+?)\\s*$`, "i"),
  },
};
FORMS.octave = FORMS.matlab;

const used = (forms, name) => (forms.used ? forms.used(name) : name);

// The Rust type a literal value has, for a constant's declaration, or null for a value that is not
// one literal.
function rustTypeOf(value) {
  if (/^-?\d[\d_]*$/.test(value)) {
    return "i64";
  }
  if (/^-?\d[\d_]*\.\d[\d_]*(?:e[+-]?\d+)?$/i.test(value)) {
    return "f64";
  }
  if (/^"(?:[^"\\]|\\.)*"$/.test(value)) {
    return "&str";
  }
  if (value === "true" || value === "false") {
    return "bool";
  }
  return null;
}

const KEYWORDS = new Set(["if", "for", "while", "return", "new", "await", "async", "not", "and", "or", "in", "is", "self", "this", "true", "false", "null", "none", "nil"]);

const snake = (word) => word.replace(/([a-z0-9])([A-Z])/g, "$1_$2").toLowerCase();
const camel = (word) => word.replace(/_([a-z0-9])/g, (_, next) => next.toUpperCase());

// A name for a value: for a value that joins others with operators, `value`; for a call, the name
// of the function called, without a leading get, read or make; for a member, the member's name; or
// else its last word; each in the file's case. A name the file uses already takes a number after it.
function nameFor(value, forms, text, upper) {
  // The value with its strings emptied and what its brackets hold taken out, so that only the
  // operators that join its parts are left to see.
  let bare = value.replace(/"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'/g, '""');
  for (let inner = bare.replace(/\([^()]*\)|\[[^[\]]*\]/g, ""); inner !== bare; inner = bare.replace(/\([^()]*\)|\[[^[\]]*\]/g, "")) {
    bare = inner;
  }
  const joined = /[-+*/%<>=&|^?]|\b(?:and|or|not)\b/.test(bare.replace(/->|=>|::/g, ""));
  const call = value.match(/([A-Za-z_]\w*)\s*\((?:[^()]|\([^()]*\))*\)\s*$/);
  const words = bare.match(/[A-Za-z_]\w*/g) ?? [];
  // A string is named by its first words.
  const string = value.match(/^(?:"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)')$/);
  const spoken = string ? ((string[1] ?? string[2]).match(/[A-Za-z][A-Za-z0-9]*/g) ?? []).slice(0, 3).join("_") : "";
  let base = spoken || (joined ? "value" : call ? call[1].replace(/^(?:get|read|make)_?(?=[A-Za-z])/i, "") : (words.filter((word) => !KEYWORDS.has(word.toLowerCase())).at(-1) ?? ""));
  if (base.length < 2 || /^\d/.test(base)) {
    base = "value";
  }
  base = upper ? snake(base).toUpperCase() : forms.camel ? camel(base[0].toLowerCase() + base.slice(1)) : snake(base);
  // A name is taken where the file uses it as a name of its own, and not only as a member after a
  // dot or a path's colons.
  const taken = (name) => new RegExp(`(?<![\\w$.]|::)${name}\\b`).test(text);
  let name = base;
  for (let count = 2; taken(name); count += 1) {
    name = `${base}${count}`;
  }
  return name;
}

// The line a statement holding line `line` starts on: up past each line that runs on to the next,
// ending in an operator, a comma, an open parenthesis or square bracket, or a backslash. A brace or a
// colon that ends a line opens a block, and the statement starts below it.
function statementStart(doc, line) {
  let top = line;
  while (top > 0) {
    const before = doc.line(top - 1).trimEnd();
    if (!/[([,+\-*/=&|?\\]$/.test(before) && !/^\s*[.)\]]/.test(doc.line(top))) {
      break;
    }
    top -= 1;
  }
  return top;
}

// The selected expression, trimmed, on one line, or null with the reason said.
function selectedValue(editor, what) {
  const s = editor.s;
  const sel = s.selections[s.primary];
  const from = startOf(sel);
  const to = endOfSel(sel);
  if (cmp(from, to) === 0) {
    say(`Select the expression for ${what}.`);
    return null;
  }
  if (from.line !== to.line) {
    say(`${what} takes an expression on one line.`);
    return null;
  }
  const line = s.doc.line(from.line);
  const raw = line.slice(from.col, to.col);
  const lead = raw.length - raw.trimStart().length;
  const value = raw.trim();
  if (!value) {
    return null;
  }
  return { value, from: pos(from.line, from.col + lead), to: pos(from.line, from.col + lead + value.length) };
}

function formsOf(editor, what) {
  const id = editor.s.language?.id;
  const forms = FORMS[id];
  if (!forms) {
    say(`${what} has no form for ${id ?? "this file"}.`);
  }
  return forms ?? null;
}

// Inserts `declaration` as the line before line `at`, at `indent`, puts the name where the value
// stood, and selects the name in both.
function declareAndUse(editor, { at, indent, declaration, name, use, from, to }) {
  const inserted = `${indent}${declaration}\n`;
  const nameIn = declaration.indexOf(name);
  const useName = use.indexOf(name);
  const edits = same(from, pos(at, 0))
    ? [{ from, to, text: `${inserted}${use}` }]
    : [
        { from: pos(at, 0), to: pos(at, 0), text: inserted },
        { from, to, text: use },
      ];
  editor.change(edits, null, (map) => {
    const declared = pos(at, indent.length + nameIn);
    const placed = map(from, false);
    const usedAt = edits.length === 1 ? pos(at + 1, from.col + useName) : pos(placed.line, placed.col + useName);
    return [
      { anchor: declared, head: pos(declared.line, declared.col + name.length), goal: null },
      { anchor: usedAt, head: pos(usedAt.line, usedAt.col + name.length), goal: null },
    ];
  });
  editor.focus();
}

const same = (a, b) => cmp(a, b) === 0;

export function extractVariable(editor, path) {
  const forms = formsOf(editor, "Extract Variable");
  const picked = forms && selectedValue(editor, "Extract Variable");
  if (!picked) {
    return;
  }
  const doc = editor.s.doc;
  const at = statementStart(doc, picked.from.line);
  const indent = doc.line(at).match(/^\s*/)[0];
  const name = nameFor(picked.value, forms, doc.text(), false);
  declareAndUse(editor, { at, indent, declaration: forms.variable(name, picked.value, path), name, use: used(forms, name), ...picked });
}

// The line a constant goes on: after the file's opening comments and blank lines and what it brings
// in, or for the C family after its #include lines.
function headOf(doc) {
  const brings = /^\s*(?:import\b|from\s+\S+\s+import\b|use\s|extern\s+crate\b|#\s*include\b|require\b|const\s+\w+\s*=\s*require\(|library\(|using\s|#!|#\s*pragma\b)/;
  const quiet = /^\s*(?:$|\/\/|#(?!\s*(?:define|include|pragma))|\/\*|\*|"""|'''|--)/;
  let after = 0;
  let inDoc = false;
  for (let line = 0; line < Math.min(doc.count, 400); line += 1) {
    const text = doc.line(line);
    const quotes = (text.match(/"""|'''/g) ?? []).length;
    if (inDoc || (quotes === 1 && line === after)) {
      if (quotes % 2 === 1) {
        inDoc = !inDoc;
      }
      after = line + 1;
      continue;
    }
    if (brings.test(text)) {
      after = line + 1;
    } else if (!quiet.test(text)) {
      break;
    }
  }
  return after;
}

// A value a constant may take: literals, and names in capitals that are constants already, joined by
// operators.
function constantValue(value) {
  const words = value.replace(/"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'/g, "").match(/[A-Za-z_]\w*/g) ?? [];
  return words.every((word) => /^[A-Z_][A-Z0-9_]*$/.test(word) || /^(?:true|false|True|False|None|null|nil|TRUE|FALSE|NULL)$/.test(word));
}

export function extractConstant(editor) {
  const forms = formsOf(editor, "Extract Constant");
  const picked = forms && selectedValue(editor, "Extract Constant");
  if (!picked) {
    return;
  }
  if (!constantValue(picked.value)) {
    say("Extract Constant takes a value made of literals and other constants. For one that names a variable, Extract Variable puts it beside its use.");
    return;
  }
  const doc = editor.s.doc;
  const name = nameFor(picked.value, forms, doc.text(), true);
  const declaration = forms.constant(name, picked.value);
  if (!declaration) {
    say("Extract Constant in Rust takes one literal: a number, a string or true or false.");
    return;
  }
  const at = headOf(doc);
  const blank = at > 0 && doc.line(at - 1).trim() !== "" ? "\n" : "";
  const below = doc.line(at).trim() !== "" ? "\n" : "";
  const inserted = `${blank}${declaration}\n${below}`;
  const use = used(forms, name);
  const nameIn = declaration.indexOf(name);
  editor.change(
    [
      { from: pos(at, 0), to: pos(at, 0), text: inserted },
      { from: picked.from, to: picked.to, text: use },
    ],
    null,
    (map) => {
      const declared = pos(at + (blank ? 1 : 0), nameIn);
      const placed = map(picked.from, false);
      const usedAt = pos(placed.line, placed.col + use.indexOf(name));
      return [
        { anchor: declared, head: pos(declared.line, declared.col + name.length), goal: null },
        { anchor: usedAt, head: pos(usedAt.line, usedAt.col + name.length), goal: null },
      ];
    },
  );
  editor.focus();
}

// A value that reads the same wherever it is put, with no parentheses about it: a name, a member, a
// call, an index, a literal or a value already in brackets.
const standsAlone = (value) =>
  /^[$\w.]+(?:\([^()]*\)|\[[^[\]]*\])*$/.test(value) || /^"(?:[^"\\]|\\.)*"$|^'(?:[^'\\]|\\.)*'$/.test(value) || /^\((?:[^()]|\([^()]*\))*\)$/.test(value) || /^-?\d[\w.]*$/.test(value);

export function inlineVariable(editor) {
  const forms = formsOf(editor, "Inline Variable");
  if (!forms) {
    return;
  }
  const s = editor.s;
  const doc = s.doc;
  const head = s.selections[s.primary].head;
  const text = doc.line(head.line);
  let start = head.col;
  let end = head.col;
  while (start > 0 && /[\w$]/.test(text[start - 1])) {
    start -= 1;
  }
  while (end < text.length && /[\w$]/.test(text[end])) {
    end += 1;
  }
  const name = text.slice(start, end).replace(/^\$/, "");
  if (!/^[A-Za-z_]\w*$/.test(name)) {
    say("Put the cursor on a variable to inline it.");
    return;
  }
  const declares = forms.declares(name);
  let declared = -1;
  let found = null;
  for (let line = head.line; line >= Math.max(0, head.line - 2000); line -= 1) {
    found = doc.line(line).match(declares);
    if (found) {
      declared = line;
      break;
    }
  }
  if (!found) {
    say(`No line above declares ${name} with a value on that line.`);
    return;
  }
  const value = found[2].trim();
  if (/[([{,]$/.test(value) || /\\$/.test(value)) {
    say(`${name} is declared over more than one line, and Inline Variable takes one.`);
    return;
  }
  const indent = found[1].length;
  const plain = (line, col) => !/t-comment|t-string/.test(s.highlight.classAt(line, col));
  const written = used(forms, name);
  const word = new RegExp(`(?<![\\w$])${written.replace(/\$/g, "\\$")}(?![\\w])`, forms === FORMS.powershell ? "gi" : "g");
  const reassigned = new RegExp(`(?<![\\w$.])${written.replace(/\$/g, "\\$")}\\s*(?:[-+*/%|&^]|<<|>>)?=(?!=)`);
  const uses = [];
  for (let line = declared + 1; line < doc.count; line += 1) {
    const here = doc.line(line);
    if (here.trim() && here.match(/^\s*/)[0].length < indent) {
      break;
    }
    if (reassigned.test(here)) {
      say(`${name} is given another value on line ${s.base + line + 1}, and inlining it would change what the code does.`);
      return;
    }
    for (const match of here.matchAll(word)) {
      if (plain(line, match.index)) {
        uses.push({ from: pos(line, match.index), to: pos(line, match.index + match[0].length) });
      }
    }
  }
  const put = standsAlone(value) ? value : `(${value})`;
  const edits = [{ from: pos(declared, 0), to: declared + 1 < doc.count ? pos(declared + 1, 0) : pos(declared, doc.line(declared).length), text: "" }, ...uses.map((use) => ({ ...use, text: put }))];
  editor.change(edits, null, (map) => {
    const at = map(uses[0]?.from ?? pos(declared, 0), false);
    return [{ anchor: at, head: at, goal: null }];
  });
  say(uses.length ? `${name} inlined in ${uses.length} ${uses.length === 1 ? "place" : "places"}.` : `${name} had no uses, and its line is taken out.`);
  editor.focus();
}
