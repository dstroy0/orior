// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Change Signature: a function's parameters changed in its declaration and at every call of it in
// the tree, one step that undo takes back in every file it changed, where no language server offers
// it. The function is what the cursor's line declares or what the word under the cursor names,
// in JavaScript, Python, Rust or the C family. A sheet lists its parameters as written: each can be
// written anew, moved up or down, or taken out, and a parameter added is given the value each call
// passes for it.
//
// A call is the function's name and a bracket after it, in any file of the tree the search reads, a
// declaration's own name and a C prototype's being declarations. Each call's arguments are moved as
// their parameters moved, an argument whose parameter is taken out goes, and an added parameter's
// value goes where it stands. Where a call passed fewer arguments than a moved parameter needs, the
// gap takes the parameter's default, and where it has none the gap ends the arguments. A method's
// first parameter, `self`, `cls` or `this`, is passed by the object a dotted call is made on and stays
// first. Python's keyword arguments keep their places and take a parameter's new name.

import { symbolsOf } from "./outline.js";
import { say } from "./statusbar.js";

const QUOTES = new Set(['"', "'", "`"]);

// The languages Change Signature reads, and how each writes a gap it cannot fill.
const LANGUAGES = {
  javascript: { empty: "undefined", declarer: /\b(?:function\s*\*?|get|set|static|async)\s*$/ },
  typescript: { empty: "undefined", declarer: /\b(?:function\s*\*?|get|set|static|async)\s*$/ },
  python: { empty: "None", declarer: /\bdef\s+$/, keywords: true },
  rust: { empty: null, declarer: /\bfn\s+$/, generics: true },
  c: { empty: null, declarer: null, generics: true, prototypes: true },
  cpp: { empty: null, declarer: null, generics: true, prototypes: true },
  cuda: { empty: null, declarer: null, generics: true, prototypes: true },
};

// A method's first parameter, which the object a dotted call is made on passes.
const RECEIVER = /^(?:&\s*(?:'\w+\s+)?(?:mut\s+)?)?(?:mut\s+)?(?:self|cls|this)\b/;

// The offset each line of a text starts at.
function startsOf(text) {
  const starts = [0];
  for (let at = 0; at < text.length; at += 1) {
    if (text[at] === "\n") {
      starts.push(at + 1);
    }
  }
  return starts;
}

function placeOf(starts, offset) {
  let low = 0;
  let high = starts.length - 1;
  while (low < high) {
    const middle = (low + high + 1) >> 1;
    if (starts[middle] <= offset) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return { line: low, col: offset - starts[low] };
}

// The list a bracket at `open` holds in `text`, read to its closing bracket past strings and nested
// brackets: where it opens and closes, and each of its parts at the top as [start, end] offsets with
// the white space at their ends left out. `angles` reads < and > after a name as brackets, for
// generics.
export function listAt(text, open, angles = false) {
  const parts = [];
  let depth = 0;
  let start = open + 1;
  let quote = null;
  for (let at = open; at < text.length; at += 1) {
    const char = text[at];
    if (quote) {
      if (char === "\\") {
        at += 1;
      } else if (char === quote) {
        quote = null;
      }
      continue;
    }
    if (QUOTES.has(char) && at > open) {
      quote = char;
    } else if ("([{".includes(char) || (angles && char === "<" && /[\w>]/.test(text[at - 1] ?? ""))) {
      depth += 1;
    } else if (")]}".includes(char) || (angles && char === ">" && depth > 1 && text[at - 1] !== "-")) {
      depth -= 1;
      if (depth === 0) {
        parts.push([start, at]);
        const trimmed = parts
          .map(([from, to]) => {
            while (from < to && /\s/.test(text[from])) from += 1;
            while (to > from && /\s/.test(text[to - 1])) to -= 1;
            return [from, to];
          })
          .filter(([from, to], index, all) => from < to || all.length > 1);
        return { open, close: at, parts: trimmed };
      }
    } else if (char === "," && depth === 1) {
      parts.push([start, at]);
      start = at + 1;
    }
  }
  return null;
}

// The name a parameter as written declares: Python's and JavaScript's before a type or a default,
// Rust's before its type, the C family's last word before its default; null for a pattern taken
// apart, which has none.
export function paramName(text, language) {
  const plain = text.replace(/\s*=.*$/s, "").trim();
  if (/^[[{(]/.test(plain)) {
    return null;
  }
  if (language === "rust") {
    return plain.match(/^(?:&\s*(?:'\w+\s+)?)?(?:mut\s+)?([A-Za-z_]\w*)/)?.[1] ?? null;
  }
  if (["c", "cpp", "cuda"].includes(language)) {
    return plain.replace(/\[[^\]]*\]\s*$/, "").match(/([A-Za-z_]\w*)\s*$/)?.[1] ?? null;
  }
  return plain.replace(/^\*{1,2}|^\.\.\./, "").match(/^([A-Za-z_$][\w$]*)/)?.[1] ?? null;
}

// A parameter's default as written after its =, or null.
function defaultOf(text) {
  const cut = text.search(/=(?!=)/);
  return cut >= 0 && !/[<>!]$/.test(text.slice(0, cut)) ? text.slice(cut + 1).trim() || null : null;
}

// The function at the cursor: its name, and where its parameters' list opens in the file's text.
export function functionAt(doc, language, head) {
  const word = (() => {
    const line = doc.line(head.line);
    let from = head.col;
    let to = head.col;
    while (from > 0 && /[\w$]/.test(line[from - 1])) from -= 1;
    while (to < line.length && /[\w$]/.test(line[to])) to += 1;
    return line.slice(from, to);
  })();
  const symbols = symbolsOf(language, doc).filter((one) => /function|method|fn/i.test(one.kind ?? "function"));
  const symbol = symbols.find((one) => one.line === head.line && (!word || one.name === word || !symbols.some((two) => two.name === word))) ?? symbols.find((one) => one.name === word);
  if (!symbol) {
    return null;
  }
  const text = doc.text();
  const starts = startsOf(text);
  const lineAt = starts[symbol.line];
  const own = new RegExp(`(?<![\\w$])${symbol.name.replace(/[$]/g, "\\$")}(?![\\w$])`, "g");
  own.lastIndex = lineAt;
  const found = own.exec(text);
  if (!found) {
    return null;
  }
  // The list opens after the name, past generics or an assignment to an arrow function.
  const after = text.slice(found.index + symbol.name.length).match(/^\s*(?:<[^()]*?>\s*)?(?:=\s*(?:async\s*)?(?:function\s*)?)?\(/);
  if (!after) {
    return null;
  }
  return { name: symbol.name, at: found.index, open: found.index + symbol.name.length + after[0].length - 1, text, starts };
}

// The new list of a call's arguments, from its old ones and the parameters as changed: each a
// parameter's old place or null where it is added, its text, and an added one's value.
export function argumentsFor(old, params, { receiver, language, keywords }) {
  const forms = LANGUAGES[language] ?? {};
  const named = keywords ? old.filter((arg) => /^[A-Za-z_]\w*\s*=(?!=)/.test(arg)) : [];
  const placed = old.filter((arg) => !named.includes(arg));
  const shift = receiver ? 1 : 0;
  const out = [];
  let gap = false;
  for (const param of params.slice(shift)) {
    let arg;
    if (param.from === null) {
      arg = param.value?.trim() || null;
    } else {
      arg = placed[param.from - shift];
      if (arg === undefined && param.oldName && named.some((one) => one.startsWith(`${param.oldName}=`) || new RegExp(`^${param.oldName}\\s*=`).test(one))) {
        continue;
      }
      arg = arg ?? null;
    }
    if (arg === null) {
      const fill = defaultOf(param.text) ?? forms.empty;
      if (fill === null || fill === undefined) {
        gap = true;
        continue;
      }
      out.push({ text: fill, filler: true });
      continue;
    }
    if (gap) {
      break;
    }
    out.push({ text: arg, filler: false });
  }
  // Fillers at the end stand for nothing a call passed, and go.
  while (out.length && out.at(-1).filler) {
    out.pop();
  }
  const renamed = new Map(params.filter((param) => param.oldName && param.oldName !== paramName(param.text, language)).map((param) => [param.oldName, paramName(param.text, language)]));
  const gone = new Set(params.removed ?? []);
  const keyword = named
    .filter((arg) => !gone.has(arg.match(/^([A-Za-z_]\w*)/)[1]))
    .map((arg) => {
      const name = arg.match(/^([A-Za-z_]\w*)/)[1];
      return renamed.has(name) ? arg.replace(/^[A-Za-z_]\w*/, renamed.get(name)) : arg;
    });
  return [...out.map((one) => one.text), ...keyword];
}

// Every call of `name` in a text, and every other declaration of it a C prototype makes: each where
// its list opens and whether it is a declaration or a dotted call.
export function callsIn(text, name, language, skip = -1) {
  const forms = LANGUAGES[language] ?? {};
  const found = [];
  const pattern = new RegExp(`(?<![\\w$])${name.replace(/[$]/g, "\\$")}(\\s*(?:::<[^()]*?>)?\\s*)\\(`, "g");
  for (const match of text.matchAll(pattern)) {
    if (match.index === skip) {
      continue;
    }
    const before = text.slice(Math.max(0, match.index - 40), match.index);
    if (forms.declarer?.test(before)) {
      continue;
    }
    const declares = Boolean(forms.prototypes && /[\w*&>]\s+[*&]*$/.test(before) && !/\b(?:return|else|case|new|delete|throw)\s+$/.test(before));
    found.push({ open: match.index + match[0].length - 1, declares, dotted: /\.\s*$/.test(before) });
  }
  return found;
}

// The parameters as changed, from the sheet, or null where it was closed.
function askParams(sheet, name, params, language) {
  return new Promise((done) => {
    const body = document.createElement("form");
    body.className = "sheet-ask signature-sheet";
    const title = Object.assign(document.createElement("h3"), { textContent: `Change Signature of ${name}` });
    const list = Object.assign(document.createElement("div"), { className: "signature-list" });
    const rows = params.map((text, from) => ({ from, text, value: "" }));
    const draw = () => {
      list.replaceChildren(
        ...rows.map((row, at) => {
          const line = Object.assign(document.createElement("div"), { className: "signature-row" });
          const field = Object.assign(document.createElement("input"), { className: "report-field", value: row.text, spellcheck: false, ariaLabel: `Parameter ${at + 1}` });
          field.addEventListener("input", () => (row.text = field.value));
          line.append(field);
          if (row.from === null) {
            const value = Object.assign(document.createElement("input"), { className: "report-field signature-value", value: row.value, spellcheck: false, placeholder: "Value at each call", ariaLabel: `Value at each call for parameter ${at + 1}` });
            value.addEventListener("input", () => (row.value = value.value));
            line.append(value);
          }
          const button = (label, glyph, act, off = false) => {
            const made = Object.assign(document.createElement("button"), { type: "button", className: "signature-act", textContent: glyph, title: label, disabled: off });
            made.setAttribute("aria-label", label);
            made.addEventListener("click", () => {
              act();
              draw();
            });
            return made;
          };
          line.append(
            button("Move Up", "↑", () => rows.splice(at - 1, 0, ...rows.splice(at, 1)), at === 0),
            button("Move Down", "↓", () => rows.splice(at + 1, 0, ...rows.splice(at, 1)), at === rows.length - 1),
            button("Take Out", "×", () => rows.splice(at, 1)),
          );
          return line;
        }),
      );
    };
    draw();
    const add = Object.assign(document.createElement("button"), { type: "button", className: "signature-add", textContent: "Add Parameter" });
    add.addEventListener("click", () => {
      rows.push({ from: null, text: "", value: "" });
      draw();
      list.lastElementChild?.querySelector("input")?.focus();
    });
    const go = Object.assign(document.createElement("button"), { type: "submit", className: "primary", textContent: "Change" });
    body.append(title, list, add, go);
    let answer = null;
    body.addEventListener("submit", (event) => {
      event.preventDefault();
      const kept = rows.filter((row) => row.text.trim());
      const taken = new Set(kept.map((row) => row.from).filter((from) => from !== null));
      answer = kept.map((row) => ({ from: row.from, text: row.text.trim(), value: row.value, oldName: row.from === null ? null : paramName(params[row.from], language) }));
      answer.removed = params.map((text, from) => (taken.has(from) ? null : paramName(text, language))).filter(Boolean);
      dialog.close();
    });
    const dialog = sheet(body);
    dialog.addEventListener("close", () => done(answer));
    list.querySelector("input")?.focus();
  });
}

// Changes the signature of the function at the cursor of `editor`'s file `path`. `sheet` shows the
// sheet, `search` finds the files that name the function, `textOf` reads a file of the tree as the
// tab that holds it or the disk has it, and `apply` writes every file's edits as one step.
export async function changeSignature({ editor, path, sheet, search, textOf, apply }) {
  const s = editor.s;
  const language = s.language?.id;
  const forms = LANGUAGES[language];
  if (!forms) {
    say(`Change Signature has no form for ${s.language?.name ?? "this file"}.`);
    return;
  }
  const at = functionAt(s.doc, language, editor.primary().head);
  if (!at) {
    say("Change Signature changes a function declared on the cursor's line or named by the word under the cursor.");
    return;
  }
  const own = listAt(at.text, at.open, forms.generics);
  if (!own) {
    say(`The parameters of ${at.name} do not close.`);
    return;
  }
  const params = own.parts.map(([from, to]) => at.text.slice(from, to));
  const changed = await askParams(sheet, at.name, params, language);
  if (!changed) {
    return;
  }
  const receiver = params.length > 0 && RECEIVER.test(params[0]);
  if (receiver && changed[0]?.from !== 0) {
    say(`${params[0]} is passed by the object a call is made on, and stays first.`, { failed: true });
    return;
  }
  const declared = (list, text) => ({ from: placeOf(text.starts, list.open + 1), to: placeOf(text.starts, list.close), text: changed.map((param) => param.text).join(", ") });
  const files = new Map([[path, [declared(own, at)]]]);
  // The function's declaration is skipped as a call in its own file; every other file that names it
  // is read for its calls.
  const paths = new Set([path, ...(await search(at.name))]);
  let calls = 0;
  for (const file of paths) {
    const text = file === path ? at.text : await textOf(file);
    if (text === null) {
      continue;
    }
    const read = { text, starts: file === path ? at.starts : startsOf(text) };
    for (const call of callsIn(text, at.name, language, file === path ? at.at : -1)) {
      const list = listAt(text, call.open, call.declares && forms.generics);
      if (!list) {
        continue;
      }
      let replacement;
      if (call.declares) {
        replacement = declared(list, read).text;
      } else {
        const old = list.parts.map(([from, to]) => text.slice(from, to));
        replacement = argumentsFor(old, changed, { receiver: receiver && call.dotted, language, keywords: forms.keywords }).join(", ");
        if (receiver && !call.dotted && old.length) {
          replacement = [old[0], ...argumentsFor(old.slice(1), changed, { receiver: true, language, keywords: forms.keywords })].join(", ");
        }
        calls += 1;
      }
      const edit = { from: placeOf(read.starts, list.open + 1), to: placeOf(read.starts, list.close), text: replacement };
      files.set(file, [...(files.get(file) ?? []), edit]);
    }
  }
  const written = [...files].map(([file, edits]) => ({ path: file, edits }));
  await apply(written, `Change Signature of ${at.name}`);
  say(`${at.name} changed, with ${calls} call${calls === 1 ? "" : "s"} in ${written.length} file${written.length === 1 ? "" : "s"}.`);
}
