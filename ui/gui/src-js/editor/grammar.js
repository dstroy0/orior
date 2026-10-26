// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Colors a text a line at a time from a grammar.
//
// A grammar is { tokenizer: { root: rules, <state>: rules, ... }, <name>: [words], ... }. Each rule
// is tried in order at each place in a line, and the first whose pattern matches there takes the
// match:
//
//   [pattern, token]              the match is one token
//   [pattern, token, next]        and then the state changes
//   [pattern, { token, next }]    the same, written as one
//   [pattern, [token, ...]]       each group of the match is a token of its own, in order
//   { include: "@state" }         the rules of another state, here
//
// A token is a class name such as "keyword" or "number.cost", or { cases: { "@<name>": token,
// "<word>": token, "@default": token } }, which picks by the matched text: a list of words the
// grammar names, a word itself, or else the default. "@brackets" is the token of a bracket. A next
// of "@<state>" enters a state, "@pop" leaves the one entered last, and "@popall" returns to root.
//
// A pattern's ^ matches only at the start of a line and $ only at its end. A grammar whose
// `perLine` is set starts every line in root; any other carries the state from the end of one line
// to the start of the next.

const LONGEST = 20000;

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

function sticky(pattern) {
  const flags = pattern.flags.replace(/[gy]/g, "");
  return new RegExp(pattern.source, `${flags}y`);
}

export function compile(def) {
  const words = {};
  for (const [name, value] of Object.entries(def)) {
    if (Array.isArray(value)) {
      words[name] = new Set(def.ignoreCase ? value.map((word) => word.toLowerCase()) : value);
    }
  }
  const states = {};
  const expand = (name, seen) => {
    const out = [];
    for (const rule of def.tokenizer[name] ?? []) {
      if (!Array.isArray(rule)) {
        const included = rule.include.replace(/^@/, "");
        if (!seen.has(included)) {
          out.push(...expand(included, new Set([...seen, included])));
        }
        continue;
      }
      const [pattern, action, next] = rule;
      noteKinds(action);
      const own = action && typeof action === "object" && !Array.isArray(action) && !action.cases;
      out.push({ re: sticky(pattern), token: own ? action.token : action, next: own ? action.next : next });
    }
    return out;
  };
  for (const name of Object.keys(def.tokenizer)) {
    states[name] = expand(name, new Set([name]));
  }
  return { states, words, perLine: Boolean(def.perLine), ignoreCase: Boolean(def.ignoreCase), def };
}

// A grammar's definition as the app's window's side reads it: JSON, each pattern its source and flags.
export function grammarJson(grammar) {
  return JSON.stringify(grammar.def, (key, value) => (value instanceof RegExp ? { pattern: value.source, flags: value.flags } : value));
}

function resolve(grammar, token, text) {
  if (typeof token === "string") {
    return token === "@brackets" ? "delimiter.bracket" : token;
  }
  if (!token?.cases) {
    return "";
  }
  const key = grammar.ignoreCase ? text.toLowerCase() : text;
  for (const [name, value] of Object.entries(token.cases)) {
    if (name === "@default") {
      continue;
    }
    const hit = name.startsWith("@") ? grammar.words[name.slice(1)]?.has(key) : name === text;
    if (hit) {
      return resolve(grammar, value, text);
    }
  }
  return resolve(grammar, token.cases["@default"] ?? "", text);
}

function move(stack, next) {
  if (!next) {
    return;
  }
  if (next === "@pop") {
    if (stack.length > 1) {
      stack.pop();
    }
  } else if (next === "@popall") {
    stack.length = 1;
  } else {
    stack.push(next.replace(/^@/, ""));
  }
}

// One line's tokens as runs [col, classes], each running to the next run's col or the line's end,
// and the state the line ends in.
export function tokenize(grammar, text, state) {
  const stack = state.split("/");
  const runs = [];
  // Each token is a run of its own; plain text joins the plain run before it.
  const push = (col, token) => {
    const name = classOf(token);
    if (runs.length && !name && !runs.at(-1)[1]) {
      return;
    }
    if (runs.length && runs.at(-1)[0] === col) {
      runs.pop();
      if (runs.length && !name && !runs.at(-1)[1]) {
        return;
      }
    }
    runs.push([col, name]);
  };
  const length = Math.min(text.length, LONGEST);
  let p = 0;
  let stalled = 0;
  while (p < length) {
    const rules = grammar.states[stack.at(-1)] ?? grammar.states.root;
    let taken = false;
    for (const rule of rules) {
      rule.re.lastIndex = p;
      const found = rule.re.exec(text);
      if (!found || (!found[0].length && (!rule.next || stalled > 2))) {
        continue;
      }
      if (Array.isArray(rule.token)) {
        let col = p;
        rule.token.forEach((token, index) => {
          const part = found[index + 1] ?? "";
          if (part.length) {
            push(col, resolve(grammar, token, part));
          }
          col += part.length;
        });
      } else if (found[0].length) {
        push(p, resolve(grammar, rule.token, found[0]));
      }
      stalled = found[0].length ? 0 : stalled + 1;
      p += found[0].length;
      move(stack, rule.next);
      taken = true;
      break;
    }
    if (!taken) {
      push(p, "");
      p += 1;
      stalled = 0;
    }
  }
  if (length < text.length) {
    push(length, "");
  }
  // A rule that matches nothing at the line's end and changes the state, such as [/$/, "", "@pop"],
  // still applies there, once. It is how a state that lasts to the end of its line ends, and a line
  // that carries on to the next enters the state twice so that one is left.
  const ending = (grammar.states[stack.at(-1)] ?? []).find((one) => {
    one.re.lastIndex = text.length;
    return one.next && one.re.exec(text)?.[0] === "";
  });
  if (ending) {
    move(stack, ending.next);
  }
  return { runs: runs.length ? runs : [[0, ""]], state: grammar.perLine ? "root" : stack.join("/") };
}
