// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A grammar as the page holds it, and the classes the page draws a token's name in. orior's side colors every text by
// its grammar; the page colors none.
//
// A grammar is { tokenizer: { root: rules, <state>: rules, ... }, <name>: [words], ... }, as the plugins write it.
// A token's name is a class name such as "keyword" or "number.cost".

const classes = new Map();

// Every kind of token the grammars compiled name, as "keyword" or "number.cost", for the themes that
// color each.
const kinds = new Set();

export function tokenKinds() {
  return [...kinds].sort();
}

// Notes each kind of token a rule's action names: a token, a list of them, cases, or a rule's own.
function noteKinds(action) {
  if (typeof action === "string") {
    const kind = action === "@brackets" ? "delimiter.bracket" : action;
    if (kind && !kind.startsWith("@")) {
      kinds.add(kind);
    }
  } else if (Array.isArray(action)) {
    action.forEach(noteKinds);
  } else if (action?.cases) {
    Object.values(action.cases).forEach(noteKinds);
  } else if (action && typeof action === "object") {
    noteKinds(action.token);
  }
}

// The class names of a token: "number.cost" is t-number and t-number-cost. A color given to
// number reaches every kind of number.
export function classOf(token) {
  if (!token) {
    return "";
  }
  let found = classes.get(token);
  if (found === undefined) {
    const parts = token.split(".");
    found = parts.map((_, index) => `t-${parts.slice(0, index + 1).join("-")}`).join(" ");
    classes.set(token, found);
  }
  return found;
}

// A grammar as the page holds it: its definition, every kind of token its rules name noted for the themes.
export function compile(def) {
  for (const rules of Object.values(def.tokenizer ?? {})) {
    for (const rule of rules) {
      if (Array.isArray(rule)) {
        noteKinds(rule[1]);
      }
    }
  }
  return { perLine: Boolean(def.perLine), def };
}
