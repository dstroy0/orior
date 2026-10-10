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

// A grammar's definition as the app's Rust side reads it: JSON, each pattern its source and flags.
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
  const push = (col, token) => {
    const name = classOf(token);
    if (runs.length && runs.at(-1)[1] === name) {
      return;
    }
    if (runs.length && runs.at(-1)[0] === col) {
      runs.pop();
      if (runs.length && runs.at(-1)[1] === name) {
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

// How far back a line looks for a line whose state is known, and where it finds none, how far
// back it starts from root instead. A state carried further than that is guessed. A jump into
// the middle of a large text costs a few hundred lines and not every line above it.
const LOOK_BACK = 3000;
const GUESS_FROM = 300;

const PLAIN = [[0, ""]];

// How many lines past the last one asked for each request to the Rust side colors besides, which
// a scroll then finds done, and how many lines below an edit are asked for as it is made.
const AHEAD = 120;
const SCREEN = 80;

// The app's Rust side, where the page has handed it in: `keep(json)` keeps a grammar and answers its
// key, and `color(key, state, lines)` answers each line's runs and the state each ends in.
let colorer = null;
const keys = new WeakMap();

export function colorWith(given) {
  colorer = given;
}

// The key a grammar is kept under on the Rust side, or null where it refuses the grammar.
function keyOf(grammar) {
  if (!keys.has(grammar)) {
    keys.set(grammar, colorer.keep(grammarJson(grammar)).catch(() => null));
  }
  return keys.get(grammar);
}

// The tokens of the lines of a text, worked out as they are asked for, from the lines on screen
// outward, and kept until an edit reaches them. Where the app's Rust side keeps the grammar, it
// works them out: the lines asked for in a frame go in one request, and until the answer comes, a
// line an edit reached shows the tokens it had before, carried with its line, and `colored` is
// called once they come. Where it does not, the page works them out as they are asked for. A
// line's tokens wanted at once, as bracket pairs and printing want them, are worked out in the
// page for a line not yet answered.
// A line's runs with a parse's spans laid over them, each span's class from its first col to its
// last. A parse that colors every col stands in for the runs whole.
function overlay(runs, spans, names, whole) {
  if (whole) {
    return spans.length ? spans.map(([from, , cls]) => [from, names[cls] ?? ""]) : [[0, ""]];
  }
  if (!spans.length) {
    return runs;
  }
  const classAt = (col) => {
    let name = "";
    for (const [start, cls] of runs) {
      if (start > col) {
        break;
      }
      name = cls;
    }
    return name;
  };
  const out = [];
  let col = 0;
  for (const [from, to, cls] of spans) {
    if (from > col) {
      out.push([col, classAt(col)]);
      for (const [start, name] of runs) {
        if (start > col && start < from) {
          out.push([start, name]);
        }
      }
    }
    out.push([from, names[cls] ?? ""]);
    col = to;
  }
  out.push([col, classAt(col)]);
  for (const [start, name] of runs) {
    if (start > col) {
      out.push([start, name]);
    }
  }
  return out;
}

export class Highlight {
  constructor(doc, grammar, colored = null) {
    this.doc = doc;
    this.grammar = grammar;
    // The file's parse, where one reads it: each line's spans by the line, the names of its classes,
    // whether it colors every col, the lines asked for, its folds, and whether the text changed since.
    this.parse = null;
    this.starts = ["root"];
    this.runs = [];
    this.stale = [];
    this.count = doc.count;
    this.colored = colored;
    this.remote = null;
    this.wanted = null;
    this.flying = false;
    this.cut = Infinity;
    if (grammar && colorer) {
      keyOf(grammar).then((key) => {
        if (key !== null && this.grammar === grammar) {
          this.remote = key;
          this.colored?.();
        }
      });
    }
  }

  // Forgets every line from this one on. The lines after it keep, as stale, the tokens they had,
  // moved by as many lines as the text gained or lost, to show until theirs come. The lines from it
  // to a screen's worth below are asked for at once, so that the answer comes before the frame that
  // draws the edit.
  forget(line) {
    const moved = this.doc.count - this.count;
    this.count = this.doc.count;
    this.cut = Math.min(this.cut, line);
    const kept = this.runs.length > line;
    if (this.remote) {
      for (let at = line; at < this.runs.length; at += 1) {
        if (this.runs[at]) {
          this.stale[at > line ? at + moved : at] = this.runs[at];
        }
      }
    }
    if (this.starts.length > line + 1) {
      this.starts.length = line + 1;
    }
    if (this.runs.length > line) {
      this.runs.length = line;
    }
    if (this.remote && kept && line < this.doc.count) {
      this.ask(line);
      this.ask(Math.min(this.doc.count - 1, line + SCREEN));
    }
  }

  // Lines put in above the first: what was worked out moves down with its lines, and the new first
  // line starts at root. The lines below keep the state they were guessed to start in until the
  // lines above reach them and say otherwise.
  shift(added) {
    if (!added) {
      return;
    }
    this.starts = new Array(added).concat(this.starts);
    this.starts[0] = "root";
    this.runs = new Array(added).concat(this.runs);
    this.stale = new Array(added).concat(this.stale);
    this.count = this.doc.count;
  }

  // Writes the state a line starts in. Where it differs from the one kept, everything worked out
  // from that line on was worked out from a wrong state, and is forgotten.
  startAt(line, state) {
    if (this.starts[line] !== undefined && this.starts[line] !== state) {
      this.forget(line);
    }
    this.starts[line] = state;
  }

  // The line the tokens of `line` are worked out from: the nearest line above with its state known,
  // or one GUESS_FROM above, started at root, where none is within LOOK_BACK.
  knownFrom(line) {
    let from = line;
    const floor = Math.max(0, line - LOOK_BACK);
    while (from > floor && this.starts[from] === undefined) {
      from -= 1;
    }
    if (this.starts[from] === undefined) {
      from = Math.max(0, line - GUESS_FROM);
      this.starts[from] = this.starts[from] ?? "root";
    }
    return from;
  }

  stateAt(line) {
    if (!this.grammar || this.grammar.perLine) {
      return "root";
    }
    if (this.starts[line] !== undefined) {
      return this.starts[line];
    }
    const from = this.knownFrom(line);
    for (let at = from; at < line; at += 1) {
      const { runs, state } = tokenize(this.grammar, this.doc.line(at), this.starts[at]);
      if (!this.remote) {
        this.runs[at] = runs;
      }
      this.startAt(at + 1, state);
    }
    return this.starts[line];
  }

  // A line's runs with its parse laid over them, where the parse holds the line.
  laid(line, runs) {
    const spans = this.parse?.byLine.get(line);
    return spans ? overlay(runs ?? PLAIN, spans, this.parse.names, this.parse.whole) : runs;
  }

  // A line's tokens where they are already worked out, or null.
  cached(line) {
    if (this.parse?.whole && this.parse.byLine.has(line)) {
      return this.laid(line, PLAIN);
    }
    return this.grammar ? this.laid(line, this.runs[line]) ?? null : this.laid(line, PLAIN);
  }

  runsOf(line) {
    return this.laid(line, this.ownRunsOf(line));
  }

  ownRunsOf(line) {
    if (!this.grammar) {
      return PLAIN;
    }
    const found = this.runs[line];
    if (found) {
      return found;
    }
    if (!this.remote) {
      return this.runsNow(line);
    }
    this.ask(line);
    return this.stale[line] ?? PLAIN;
  }

  // A line's tokens now: kept, or worked out in the page where they are not.
  runsNow(line) {
    return this.laid(line, this.ownRunsNow(line));
  }

  ownRunsNow(line) {
    if (!this.grammar) {
      return PLAIN;
    }
    const found = this.runs[line];
    if (found) {
      return found;
    }
    const { runs, state } = tokenize(this.grammar, this.doc.line(line), this.stateAt(line));
    if (!this.remote) {
      this.runs[line] = runs;
    }
    if (!this.grammar.perLine) {
      this.startAt(line + 1, state);
    }
    return runs;
  }

  // Adds a line to the next request to the Rust side, which goes once the page's work in hand is done.
  ask(line) {
    const first = this.wanted === null;
    this.wanted = first ? [line, line] : [Math.min(this.wanted[0], line), Math.max(this.wanted[1], line)];
    if (first && !this.flying) {
      queueMicrotask(() => this.send());
    }
  }

  send() {
    if (!this.wanted || this.flying || !this.remote) {
      return;
    }
    const [low, high] = this.wanted;
    this.wanted = null;
    const from = this.grammar.perLine ? low : this.knownFrom(low);
    const to = Math.min(this.doc.count - 1, high + AHEAD);
    const lines = [];
    for (let at = from; at <= to; at += 1) {
      lines.push(this.doc.line(at));
    }
    const grammar = this.grammar;
    const key = this.remote;
    this.flying = true;
    this.cut = Infinity;
    colorer
      .color(key, this.grammar.perLine ? "root" : this.starts[from], lines)
      .then(({ names, runs, states }) => {
        this.flying = false;
        if (this.grammar !== grammar || this.remote !== key) {
          return;
        }
        const classes = names.map(classOf);
        // Only the lines above the first an edit reached since the request went are as they were.
        const until = Math.min(from + runs.length, this.cut);
        for (let at = from; at < until; at += 1) {
          const own = runs[at - from].map(([col, name]) => [col, classes[name]]);
          this.runs[at] = own;
          this.stale[at] = undefined;
          if (!grammar.perLine && at + 1 <= this.cut) {
            this.startAt(at + 1, states[at - from]);
          }
        }
        this.colored?.();
        if (this.wanted) {
          this.send();
        }
      })
      .catch(() => {
        this.flying = false;
        this.remote = null;
        this.colored?.();
      });
  }

  // The class of the token at a place.
  classAt(line, col) {
    const runs = this.runsNow(line);
    let name = "";
    for (const [start, cls] of runs) {
      if (start > col) {
        break;
      }
      name = cls;
    }
    return name;
  }
}
