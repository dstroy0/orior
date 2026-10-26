// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Extract Function in Rust and the C family: each parameter and what the new function gives back take their types
// from the language server's hover.

import { pos } from "../editor/document.js";
import { freshName, indentOf, widthOf, write } from "../extract.js";
import { inferred, statementStart } from "../refactor.js";
import { hoverAt } from "../servers.js";
import { say } from "../statusbar.js";

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

export async function extractTyped(editor, tab, id, lang, read) {
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

  // Each name's type, from the server's hover where the selection first reads or sets it: null where
  // the hover names no type of the name's own, and undefined where the server answered nothing.
  const kinds = new Map();
  const ask = async (name, line, col) => {
    if (!kinds.has(name)) {
      const said = await hoverAt(tab, pos(line, col), null, true);
      kinds.set(name, said ? lang.typeIn(said, name) : undefined);
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
    if (kind === undefined) {
      say(`The language server gives no type for ${name}, which the selection reads. Extract Function again once it has read the file.`);
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
    const said = await hoverAt(tab, pos(top, indent.length + binding.indexOf(name)), lines.join("\n"), true);
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
