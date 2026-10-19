// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's keys for the editor, where Preferences sets them on. The editor asks this of each key first,
// and what this does not take goes to the editor's own keys, which in insert mode is every key but
// Escape, and in the other modes every key held with Ctrl that Vim gives nothing to.
//
// Normal mode takes a count before a command, and "x before it names register x; the register `"`
// takes every yank and delete, and `0` the last yank. The motions: h j k l and the arrows, w b e and
// W B E, 0 ^ $, gg and G, f F t T to a letter on the line and ; , to repeat it, % to the bracket that
// pairs with the next on the line, { } to the empty line before or after, n N to the next or the last
// place searched for, and Ctrl+D Ctrl+U by half a screen. The operators d c y > < act on a motion, on
// a text object (iw aw iW aW, and i or a before one of " ' ` ( ) b [ ] { } B < >), or, doubled, on
// whole lines. Then x X D C Y s S J ~ r, p P, u and Ctrl+R, i a I A o O, v V, / ? * #, and `.`, which
// does the last change again, the text typed in insert mode after it among it.
//
// Visual mode, from v, and visual line mode, from V, move the selection's end by the motions, take a
// text object, swap the ends with o, and act on what is chosen with d c y > < ~ J p, x and s.

import { cmp, endOf, pos } from "./document.js";

const KEYWORD = /[\p{L}\p{N}_]/u;
const OPERATORS = new Set(["d", "c", "y", ">", "<"]);
const MOTIONS = new Set(["h", "j", "k", "l", "w", "W", "b", "B", "e", "E", "0", "^", "$", "G", ";", ",", "%", "{", "}", "n", "N", "C-d", "C-u"]);
const COMMANDS = new Set(["x", "X", "D", "C", "Y", "s", "S", "J", "~", "p", "P", "u", "C-r", ".", "i", "a", "I", "A", "o", "O", "v", "V", "/", "?", "*", "#", "Esc"]);
// The commands that change the text, which `.` does again.
const CHANGES = new Set(["d", "c", ">", "<", "x", "X", "D", "C", "s", "S", "J", "~", "r", "p", "P", "i", "a", "I", "A", "o", "O"]);
const PAIRS = { "(": "()", ")": "()", b: "()", "[": "[]", "]": "[]", "{": "{}", "}": "{}", B: "{}", "<": "<>", ">": "<>" };
const QUOTES = new Set(['"', "'", "`"]);
const ARROWS = { ArrowLeft: "h", ArrowRight: "l", ArrowUp: "k", ArrowDown: "j", Home: "0", End: "$", Delete: "x", Enter: "j" };
const NAMES = { normal: "NORMAL", insert: "INSERT", visual: "VISUAL", line: "VISUAL LINE" };

// What a command still waits for more keys to be whole.
const WAIT = Symbol("wait");

export class Vim {
  constructor(editor) {
    this.editor = editor;
    this.mode = "normal";
    this.typed = [];
    this.registers = new Map();
    this.found = null;
    this.searched = null;
    this.last = null;
    this.taking = null;
    this.goal = null;
    this.visual = null;
    this.prompt = null;
    this.setMode("normal");
  }

  get doc() {
    return this.editor.doc;
  }

  line(index) {
    return this.doc.line(index);
  }

  // What the status line shows: the mode, and the keys of a command typed so far.
  label() {
    return [NAMES[this.mode], this.typed.join("")].filter(Boolean).join(" ");
  }

  // Whether the caret is drawn as a block over its letter.
  block() {
    return this.mode !== "insert";
  }

  end() {
    this.editor.host.removeAttribute("data-vim");
    this.editor.stepWatch = null;
    const prompt = this.prompt;
    this.prompt = null;
    prompt?.remove();
  }

  setMode(mode) {
    this.mode = mode;
    this.editor.host.dataset.vim = mode;
    this.editor.stepWatch = mode === "insert" && this.taking ? (step) => this.taking.steps.push(step) : null;
    this.editor.schedule();
  }

  head() {
    return this.visual ? this.visual.head : this.editor.primary().head;
  }

  lastCol(line) {
    return Math.max(0, this.line(line).length - 1);
  }

  firstNonBlank(line) {
    const text = this.line(line);
    const col = text.search(/\S/);
    return col < 0 ? Math.max(0, text.length - 1) : col;
  }

  // Puts the cursor at a place, on a letter outside insert mode.
  put(p, keepGoal = false) {
    const line = Math.max(0, Math.min(this.doc.count - 1, p.line));
    const col = Math.max(0, Math.min(p.col, this.mode === "insert" ? this.line(line).length : this.lastCol(line)));
    const at = pos(line, col);
    if (!keepGoal) {
      this.goal = null;
    }
    this.editor.select([{ anchor: at, head: at, goal: null }]);
  }

  // A key as this names it, or null where it is the editor's.
  token(event) {
    if (["Shift", "Control", "Alt", "Meta"].includes(event.key)) {
      return null;
    }
    if (event.ctrlKey && !event.altKey && !event.metaKey && /^[a-z[]$/i.test(event.key)) {
      return event.key === "[" ? "Esc" : `C-${event.key.toLowerCase()}`;
    }
    if (event.ctrlKey || event.altKey || event.metaKey) {
      return null;
    }
    if (event.key === "Escape") {
      return "Esc";
    }
    if (event.key.length === 1) {
      return event.key;
    }
    return ARROWS[event.key] ?? (event.key === "Tab" || event.key === "Backspace" ? "" : null);
  }

  // Takes a key, and answers whether it was this one's.
  key(event) {
    if (this.mode === "insert") {
      if (event.key === "Escape" || (event.ctrlKey && event.key === "[")) {
        event.preventDefault();
        this.leaveInsert();
        return true;
      }
      return false;
    }
    const token = this.token(event);
    if (token === null || (token.startsWith("C-") && !["C-r", "C-d", "C-u"].includes(token))) {
      return false;
    }
    event.preventDefault();
    if (token === "") {
      return true;
    }
    this.take(token);
    return true;
  }

  // Escape drops the keys typed so far, and ends visual mode.
  take(token) {
    if (token === "Esc") {
      this.typed = [];
      if (this.visual) {
        this.leaveVisual(this.visual.head);
      }
      this.editor.statusSaid = null;
      this.editor.schedule();
      return;
    }
    this.typed.push(token);
    const command = this.parse(this.typed);
    if (command === WAIT) {
      this.editor.statusSaid = null;
      this.editor.schedule();
      return;
    }
    const keys = this.typed;
    this.typed = [];
    if (command) {
      this.run(command, keys);
    }
    this.editor.statusSaid = null;
    this.editor.schedule();
  }

  // A command from its keys: WAIT where it is not whole yet, and null where it is no command.
  parse(keys) {
    let at = 0;
    const more = () => at < keys.length;
    const next = () => keys[at++];
    const count = () => {
      let digits = "";
      while (more() && /^[0-9]$/.test(keys[at]) && (digits || keys[at] !== "0")) {
        digits += next();
      }
      return digits ? Number(digits) : null;
    };
    let register = null;
    if (keys[at] === '"') {
      at += 1;
      if (!more()) {
        return WAIT;
      }
      register = next();
    }
    const first = count();
    if (!more()) {
      return WAIT;
    }
    const name = next();
    const visual = this.mode === "visual" || this.mode === "line";
    if (visual && (OPERATORS.has(name) || ["~", "J", "p", "P", "x", "s", "o"].includes(name))) {
      return { register, count: first, name, chosen: true };
    }
    if ((name === "i" || name === "a") && visual) {
      if (!more()) {
        return WAIT;
      }
      return { count: first, object: { kind: name, of: next() } };
    }
    if (OPERATORS.has(name)) {
      const inner = count();
      if (!more()) {
        return WAIT;
      }
      const target = next();
      const counted = first !== null || inner !== null ? (first ?? 1) * (inner ?? 1) : null;
      if (target === name) {
        return { register, name, count: counted, lines: true };
      }
      if (target === "i" || target === "a") {
        if (!more()) {
          return WAIT;
        }
        return { register, name, count: counted, object: { kind: target, of: next() } };
      }
      const motion = this.parseMotion(target, more, next);
      return motion === WAIT || motion === null ? motion : { register, name, count: counted, motion };
    }
    if (name === "r") {
      if (!more()) {
        return WAIT;
      }
      return { count: first, name, letter: next() };
    }
    const motion = this.parseMotion(name, more, next);
    if (motion === WAIT || motion) {
      return motion === WAIT ? WAIT : { count: first, motion };
    }
    return COMMANDS.has(name) ? { register, count: first, name } : null;
  }

  parseMotion(name, more, next) {
    if (name === "g") {
      if (!more()) {
        return WAIT;
      }
      return next() === "g" ? { name: "gg" } : null;
    }
    if ("fFtT".includes(name) && name.length === 1) {
      if (!more()) {
        return WAIT;
      }
      return { name, letter: next() };
    }
    return MOTIONS.has(name) ? { name } : null;
  }

  run(command, keys) {
    const changes = !this.replaying && !command.chosen && CHANGES.has(command.name ?? "") && command.name !== "y";
    if (changes) {
      this.taking = { keys: [...keys], steps: [] };
    }
    if (command.chosen || (command.object && !command.name)) {
      this.runChosen(command);
    } else if (command.motion && !command.name) {
      this.moveBy(command.motion, command.count);
    } else if (OPERATORS.has(command.name)) {
      this.operate(command);
    } else {
      this.runCommand(command);
    }
    if (changes && this.mode !== "insert") {
      this.last = this.taking;
      this.taking = null;
    }
  }

  // Motions.

  charAt(p) {
    const text = this.line(p.line);
    return p.col < text.length ? text[p.col] : "\n";
  }

  forward(p) {
    if (p.col < this.line(p.line).length) {
      return pos(p.line, p.col + 1);
    }
    return p.line + 1 < this.doc.count ? pos(p.line + 1, 0) : null;
  }

  backward(p) {
    if (p.col > 0) {
      return pos(p.line, p.col - 1);
    }
    return p.line > 0 ? pos(p.line - 1, this.line(p.line - 1).length) : null;
  }

  // A letter's kind: 0 for space and line ends, 1 for a word's letters, 2 for any other; with `big`,
  // 1 for every letter but space.
  kind(letter, big) {
    if (letter === "\n" || /\s/.test(letter)) {
      return 0;
    }
    return big || KEYWORD.test(letter) ? 1 : 2;
  }

  wordStart(p, big) {
    let at = p;
    const start = this.kind(this.charAt(at), big);
    while (start && this.kind(this.charAt(at), big) === start) {
      at = this.forward(at);
      if (!at) {
        return p;
      }
    }
    while (this.kind(this.charAt(at), big) === 0) {
      const next = this.forward(at);
      if (!next) {
        return at;
      }
      at = next;
      if (at.col === 0 && this.line(at.line).length === 0) {
        return at;
      }
    }
    return at;
  }

  wordEnd(p, big) {
    let at = this.forward(p);
    while (at && this.kind(this.charAt(at), big) === 0) {
      at = this.forward(at);
    }
    if (!at) {
      return p;
    }
    const kind = this.kind(this.charAt(at), big);
    for (let next = this.forward(at); next && this.kind(this.charAt(next), big) === kind; next = this.forward(next)) {
      at = next;
    }
    return at;
  }

  wordBack(p, big) {
    let at = this.backward(p);
    while (at && this.kind(this.charAt(at), big) === 0 && !(at.col === 0 && this.line(at.line).length === 0)) {
      at = this.backward(at);
    }
    if (!at) {
      return pos(0, 0);
    }
    const kind = this.kind(this.charAt(at), big);
    for (let back = this.backward(at); kind && back && back.line === at.line && this.kind(this.charAt(back), big) === kind; back = this.backward(back)) {
      at = back;
    }
    return at;
  }

  // The next or the last letter on the line that is `letter`, `times` over: on it for f and F, and
  // short of it for t and T.
  findOnLine(p, how, letter, times) {
    const text = this.line(p.line);
    let col = p.col;
    for (let time = 0; time < times; time += 1) {
      if (how === "f" || how === "t") {
        const found = text.indexOf(letter, col + (how === "t" ? 2 : 1));
        if (found < 0) {
          return null;
        }
        col = how === "t" ? found - 1 : found;
      } else {
        const from = col - (how === "T" ? 2 : 1);
        const found = from < 0 ? -1 : text.lastIndexOf(letter, from);
        if (found < 0) {
          return null;
        }
        col = how === "T" ? found + 1 : found;
      }
    }
    return pos(p.line, col);
  }

  // The bracket that pairs with the first one at or after the cursor on its line.
  pairOf(p) {
    const text = this.line(p.line);
    let col = p.col;
    while (col < text.length && !"()[]{}".includes(text[col])) {
      col += 1;
    }
    if (col >= text.length) {
      return null;
    }
    const letter = text[col];
    const open = "([{".includes(letter);
    const pair = { "(": ")", ")": "(", "[": "]", "]": "[", "{": "}", "}": "{" }[letter];
    let depth = 0;
    for (let at = pos(p.line, col); at; at = open ? this.forward(at) : this.backward(at)) {
      const here = this.charAt(at);
      if (here === letter) {
        depth += 1;
      } else if (here === pair && --depth === 0) {
        return at;
      }
    }
    return null;
  }

  paragraph(p, dir, times) {
    let line = p.line;
    for (let time = 0; time < times; time += 1) {
      while (line + dir >= 0 && line + dir < this.doc.count && this.line(line).length === 0) {
        line += dir;
      }
      while (line + dir >= 0 && line + dir < this.doc.count && this.line(line + dir).length !== 0) {
        line += dir;
      }
      line += dir;
    }
    if (line < 0) {
      return pos(0, 0);
    }
    if (line >= this.doc.count) {
      return pos(this.doc.count - 1, this.line(this.doc.count - 1).length);
    }
    return pos(line, 0);
  }

  // The next place a search finds, or the last for `back`, from a place, going round the text.
  searchFrom(p, searched, back) {
    const { pattern } = searched;
    const lines = this.doc.count;
    for (let step = 0; step <= lines; step += 1) {
      const line = (((back ? p.line - step : p.line + step) % lines) + lines) % lines;
      const text = this.line(line);
      const cols = [];
      pattern.lastIndex = 0;
      for (let found = pattern.exec(text); found; found = pattern.exec(text)) {
        cols.push(found.index);
        if (found[0].length === 0) {
          pattern.lastIndex += 1;
        }
      }
      const fits = (col) => step > 0 && step < lines ? true : step === 0 ? (back ? col < p.col : col > p.col) : back ? col > p.col : col < p.col;
      const col = back ? cols.reverse().find(fits) : cols.find(fits);
      if (col !== undefined) {
        return pos(line, col);
      }
    }
    return null;
  }

  // Where a motion goes from a place, and whether it takes whole lines or takes the letter it ends on.
  motion(motion, count, from) {
    const times = count ?? 1;
    const lineEnd = (line) => pos(line, this.line(line).length);
    switch (motion.name) {
      case "h":
        return { to: pos(from.line, Math.max(0, from.col - times)) };
      case "l":
        return { to: pos(from.line, Math.min(this.line(from.line).length, from.col + times)) };
      case "j":
      case "k":
      case "C-d":
      case "C-u": {
        const rows = motion.name.startsWith("C-") ? Math.max(1, Math.floor(this.editor.scroller.clientHeight / 2 / this.editor.lineHeight)) * times : times;
        const line = Math.max(0, Math.min(this.doc.count - 1, from.line + (motion.name === "j" || motion.name === "C-d" ? rows : -rows)));
        this.goal ??= from.col;
        return { to: pos(line, this.goal), linewise: true, keepGoal: true };
      }
      case "w":
      case "W": {
        let at = from;
        for (let time = 0; time < times; time += 1) {
          at = this.wordStart(at, motion.name === "W");
        }
        return { to: at };
      }
      case "b":
      case "B": {
        let at = from;
        for (let time = 0; time < times; time += 1) {
          at = this.wordBack(at, motion.name === "B");
        }
        return { to: at };
      }
      case "e":
      case "E": {
        let at = from;
        for (let time = 0; time < times; time += 1) {
          at = this.wordEnd(at, motion.name === "E");
        }
        return { to: at, inclusive: true };
      }
      case "0":
        return { to: pos(from.line, 0) };
      case "^":
        return { to: pos(from.line, this.firstNonBlank(from.line)) };
      case "$": {
        const line = Math.min(this.doc.count - 1, from.line + times - 1);
        return { to: pos(line, Math.max(0, this.line(line).length - 1)), inclusive: this.line(line).length > 0, end: lineEnd(line) };
      }
      case "gg":
      case "G": {
        const line = count !== null ? Math.max(0, Math.min(this.doc.count - 1, count - 1)) : motion.name === "gg" ? 0 : this.doc.count - 1;
        return { to: pos(line, this.firstNonBlank(line)), linewise: true };
      }
      case "f":
      case "F":
      case "t":
      case "T":
        this.found = { how: motion.name, letter: motion.letter };
        return this.toLetter(from, motion.name, motion.letter, times);
      case ";":
      case ",": {
        if (!this.found) {
          return null;
        }
        const turned = { f: "F", F: "f", t: "T", T: "t" };
        const how = motion.name === ";" ? this.found.how : turned[this.found.how];
        return this.toLetter(from, how, this.found.letter, times);
      }
      case "%": {
        const to = this.pairOf(from);
        return to && { to, inclusive: true };
      }
      case "{":
      case "}":
        return { to: this.paragraph(from, motion.name === "}" ? 1 : -1, times) };
      case "n":
      case "N": {
        if (!this.searched) {
          return null;
        }
        let at = from;
        for (let time = 0; time < times && at; time += 1) {
          at = this.searchFrom(at, this.searched, this.searched.back !== (motion.name === "N"));
        }
        return at && { to: at };
      }
      default:
        return null;
    }
  }

  toLetter(from, how, letter, times) {
    const to = this.findOnLine(from, how, letter, times);
    return to && { to, inclusive: how === "f" || how === "t" };
  }

  moveBy(motion, count) {
    const found = this.motion(motion, count, this.head());
    if (!found) {
      return;
    }
    if (this.visual) {
      this.visual.head = pos(found.to.line, Math.min(found.to.col, this.lastCol(found.to.line)));
      this.drawChosen();
      if (!found.keepGoal) {
        this.goal = null;
      }
      return;
    }
    this.put(found.to, found.keepGoal);
  }

  // Text objects: the span of a word, of a quoted text on the line, or of a bracketed one around the
  // cursor, inside its marks for i and with them for a.
  object({ kind, of }, from, count) {
    const inner = kind === "i";
    const text = this.line(from.line);
    if (of === "w" || of === "W") {
      if (!text.length) {
        return null;
      }
      const big = of === "W";
      const at = Math.min(from.col, text.length - 1);
      const sort = this.kind(text[at], big);
      let start = at;
      let end = at + 1;
      while (start > 0 && this.kind(text[start - 1], big) === sort) {
        start -= 1;
      }
      while (end < text.length && this.kind(text[end], big) === sort) {
        end += 1;
      }
      if (!inner) {
        let after = end;
        while (after < text.length && /\s/.test(text[after])) {
          after += 1;
        }
        if (after > end) {
          end = after;
        } else {
          while (start > 0 && /\s/.test(text[start - 1])) {
            start -= 1;
          }
        }
      }
      return { start: pos(from.line, start), end: pos(from.line, end) };
    }
    if (QUOTES.has(of)) {
      const marks = [...text].map((letter, at) => (letter === of && text[at - 1] !== "\\" ? at : -1)).filter((at) => at >= 0);
      for (let at = 0; at + 1 < marks.length; at += 2) {
        if (from.col <= marks[at + 1]) {
          const [open, close] = [marks[at], marks[at + 1]];
          return inner ? { start: pos(from.line, open + 1), end: pos(from.line, close) } : { start: pos(from.line, open), end: pos(from.line, close + 1) };
        }
      }
      return null;
    }
    const pair = PAIRS[of];
    if (!pair) {
      return null;
    }
    let open = null;
    let at = from;
    for (let level = 0; level < (count ?? 1); level += 1) {
      let depth = 0;
      for (let back = level === 0 && this.charAt(at) === pair[0] ? at : this.backward(at); back; back = this.backward(back)) {
        const letter = this.charAt(back);
        if (letter === pair[1] && !(level === 0 && cmp(back, from) === 0)) {
          depth += 1;
        } else if (letter === pair[0]) {
          if (depth === 0) {
            open = back;
            break;
          }
          depth -= 1;
        }
      }
      if (!open) {
        return null;
      }
      at = open;
      if (level + 1 < (count ?? 1)) {
        at = this.backward(open) ?? open;
        open = null;
      }
    }
    let depth = 0;
    let close = null;
    for (let ahead = this.forward(open); ahead; ahead = this.forward(ahead)) {
      const letter = this.charAt(ahead);
      if (letter === pair[0]) {
        depth += 1;
      } else if (letter === pair[1]) {
        if (depth === 0) {
          close = ahead;
          break;
        }
        depth -= 1;
      }
    }
    if (!close) {
      return null;
    }
    return inner ? { start: this.forward(open), end: close } : { start: open, end: this.forward(close) ?? close };
  }

  // Operators.

  operate(command) {
    const from = this.head();
    let start;
    let end;
    let lines = false;
    if (command.lines) {
      const last = Math.min(this.doc.count - 1, from.line + (command.count ?? 1) - 1);
      [start, end, lines] = [pos(from.line, 0), pos(last, this.line(last).length), true];
    } else if (command.object) {
      const span = this.object(command.object, from, command.count);
      if (!span) {
        return;
      }
      ({ start, end } = span);
    } else {
      let motion = command.motion;
      if (command.name === "c" && (motion.name === "w" || motion.name === "W") && this.kind(this.charAt(from), motion.name === "W")) {
        motion = { name: motion.name === "w" ? "e" : "E" };
      }
      const found = this.motion(motion, command.count, from);
      if (!found) {
        return;
      }
      let to = found.to;
      if ((motion.name === "w" || motion.name === "W") && to.line > from.line) {
        to = pos(from.line, this.line(from.line).length);
      }
      [start, end] = cmp(from, to) <= 0 ? [from, to] : [to, from];
      if (found.inclusive) {
        end = found.end ?? pos(end.line, Math.min(this.line(end.line).length, end.col + 1));
      }
      if (found.linewise) {
        [start, end, lines] = [pos(start.line, 0), pos(end.line, this.line(end.line).length), true];
      }
    }
    this.goal = null;
    this.apply(command.name, start, end, lines, command.register);
  }

  store(register, text, lines, yanked) {
    const kept = { text, lines };
    if (register && /^[a-z]$/.test(register)) {
      this.registers.set(register, kept);
    } else if (register && /^[A-Z]$/.test(register)) {
      const before = this.registers.get(register.toLowerCase());
      this.registers.set(register.toLowerCase(), before ? { text: `${before.text}${lines || before.lines ? "\n" : ""}${text}`, lines: lines || before.lines } : kept);
    }
    this.registers.set('"', kept);
    if (yanked) {
      this.registers.set("0", kept);
    }
  }

  apply(name, start, end, lines, register) {
    const text = this.doc.slice(start, end);
    if (name === "y") {
      this.store(register, text, lines, true);
      this.put(lines ? pos(start.line, this.head().col) : start);
      return;
    }
    if (name === ">" || name === "<") {
      this.editor.setSelections([{ anchor: pos(start.line, 0), head: pos(end.line, Math.max(0, this.line(end.line).length)), goal: null }]);
      this.editor.indentLines(name === ">" ? 1 : -1);
      this.put(pos(start.line, this.firstNonBlank(start.line)));
      return;
    }
    this.store(register, text, lines, false);
    if (name === "d") {
      if (lines) {
        let [from, to] = [start, end];
        if (end.line + 1 < this.doc.count) {
          to = pos(end.line + 1, 0);
        } else if (start.line > 0) {
          from = pos(start.line - 1, this.line(start.line - 1).length);
        }
        this.editor.change([{ from, to, text: "" }], null);
        const line = Math.min(start.line, this.doc.count - 1);
        this.put(pos(line, this.firstNonBlank(line)));
      } else {
        this.editor.change([{ from: start, to: end, text: "" }], null);
        this.put(start);
      }
      return;
    }
    if (name === "c") {
      const indent = lines ? this.line(start.line).match(/^\s*/)[0] : "";
      this.editor.change([{ from: start, to: end, text: indent }], null);
      this.insertAt(pos(start.line, start.col + indent.length));
    }
  }

  // Commands.

  insertAt(p) {
    this.setMode("insert");
    this.put(p);
  }

  leaveInsert() {
    const head = this.editor.primary().head;
    this.setMode("normal");
    this.put(pos(head.line, Math.max(0, head.col - 1)));
    if (this.taking) {
      this.last = this.taking;
      this.taking = null;
    }
  }

  paste(command, before) {
    const kept = this.registers.get(command.register ?? '"');
    if (!kept) {
      return;
    }
    const times = command.count ?? 1;
    const text = Array.from({ length: times }, () => kept.text).join(kept.lines ? "\n" : "");
    const at = this.head();
    if (kept.lines) {
      const line = before ? at.line : at.line + 1;
      const where = before ? pos(at.line, 0) : pos(at.line, this.line(at.line).length);
      this.editor.change([{ from: where, to: where, text: before ? `${text}\n` : `\n${text}` }], null);
      this.put(pos(line, this.firstNonBlank(line)));
      return;
    }
    const where = pos(at.line, before || !this.line(at.line).length ? at.col : at.col + 1);
    this.editor.change([{ from: where, to: where, text }], null);
    const after = endOf(where, text);
    this.put(pos(after.line, Math.max(0, after.col - 1)));
  }

  join(first, count) {
    const last = Math.min(this.doc.count - 1, first + Math.max(1, count - 1));
    if (last === first) {
      return;
    }
    const edits = [];
    for (let line = first; line < last; line += 1) {
      const next = this.line(line + 1);
      const lead = next.match(/^\s*/)[0].length;
      const space = next.length === lead || next[lead] === ")" || /\s$/.test(this.line(line)) ? "" : " ";
      edits.push({ from: pos(line, this.line(line).length), to: pos(line + 1, lead), text: space });
    }
    const col = this.line(first).length;
    this.editor.change(edits, null);
    this.put(pos(first, col));
  }

  toggleCase(start, end) {
    const text = this.doc.slice(start, end);
    const turned = [...text].map((letter) => (letter === letter.toLowerCase() ? letter.toUpperCase() : letter.toLowerCase())).join("");
    this.editor.change([{ from: start, to: end, text: turned }], null);
  }

  openPrompt(back) {
    const prompt = this.prompt;
    this.prompt = null;
    prompt?.remove();
    const field = document.createElement("input");
    field.className = "ed-vim-prompt";
    field.spellcheck = false;
    field.setAttribute("aria-label", back ? "Search back" : "Search");
    field.placeholder = back ? "?" : "/";
    // Closes the prompt once, the blur its removal sets off finding it closed.
    const done = (searching) => {
      if (this.prompt !== field) {
        return;
      }
      this.prompt = null;
      field.remove();
      this.editor.focus();
      if (searching && field.value) {
        this.searchFor(field.value, back, false);
      }
    };
    field.addEventListener("keydown", (event) => {
      event.stopPropagation();
      if (event.key === "Enter" || event.key === "Escape") {
        event.preventDefault();
        done(event.key === "Enter");
      }
    });
    field.addEventListener("blur", () => done(false));
    this.editor.host.append(field);
    this.prompt = field;
    field.focus();
  }

  // Looks for the text, as a regular expression where it reads as one, and goes to the next place it
  // stands, or the last for `back`; `whole` looks for it as a whole word.
  searchFor(text, back, whole) {
    const plain = text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    let pattern;
    try {
      pattern = new RegExp(whole ? `\\b${plain}\\b` : text, "g");
    } catch {
      pattern = new RegExp(plain, "g");
    }
    this.searched = { pattern, back };
    const to = this.searchFrom(this.head(), this.searched, back);
    if (to) {
      this.put(to);
    }
  }

  runCommand(command) {
    const head = this.head();
    const times = command.count ?? 1;
    const text = this.line(head.line);
    switch (command.name) {
      case "x":
        if (text.length) {
          this.apply("d", head, pos(head.line, Math.min(text.length, head.col + times)), false, command.register);
        }
        break;
      case "X":
        if (head.col > 0) {
          this.apply("d", pos(head.line, Math.max(0, head.col - times)), head, false, command.register);
        }
        break;
      case "D":
      case "C": {
        const last = Math.min(this.doc.count - 1, head.line + times - 1);
        this.apply(command.name === "D" ? "d" : "c", head, pos(last, this.line(last).length), false, command.register);
        break;
      }
      case "Y":
        this.operate({ name: "y", lines: true, count: times, register: command.register });
        break;
      case "s":
        this.apply("c", head, pos(head.line, Math.min(text.length, head.col + times)), false, command.register);
        break;
      case "S":
        this.operate({ name: "c", lines: true, count: times, register: command.register });
        break;
      case "J":
        this.join(head.line, Math.max(2, times));
        break;
      case "~":
        if (text.length) {
          const end = pos(head.line, Math.min(text.length, head.col + times));
          this.toggleCase(head, end);
          this.put(end);
        }
        break;
      case "r":
        if (head.col + times <= text.length) {
          this.editor.change([{ from: head, to: pos(head.line, head.col + times), text: command.letter.repeat(times) }], null);
          this.put(pos(head.line, head.col + times - 1));
        }
        break;
      case "p":
      case "P":
        this.paste(command, command.name === "P");
        break;
      case "u":
      case "C-r":
        for (let time = 0; time < times; time += 1) {
          this.editor.undo(command.name === "u");
        }
        this.put(this.editor.primary().head);
        break;
      case ".":
        this.repeat();
        break;
      case "i":
        this.insertAt(head);
        break;
      case "a":
        this.insertAt(pos(head.line, Math.min(text.length, head.col + 1)));
        break;
      case "I":
        this.insertAt(pos(head.line, text.search(/\S/) < 0 ? text.length : text.search(/\S/)));
        break;
      case "A":
        this.insertAt(pos(head.line, text.length));
        break;
      case "o":
      case "O": {
        const indent = text.match(/^\s*/)[0];
        const where = command.name === "o" ? pos(head.line, text.length) : pos(head.line, 0);
        this.editor.change([{ from: where, to: where, text: command.name === "o" ? `\n${indent}` : `${indent}\n` }], null);
        this.insertAt(pos(command.name === "o" ? head.line + 1 : head.line, indent.length));
        break;
      }
      case "v":
      case "V":
        this.visual = { anchor: head, head };
        this.setMode(command.name === "v" ? "visual" : "line");
        this.drawChosen();
        break;
      case "/":
      case "?":
        this.openPrompt(command.name === "?");
        break;
      case "*":
      case "#": {
        const word = text.slice(0, head.col + 1).match(/[\p{L}\p{N}_]*$/u)[0] + text.slice(head.col + 1).match(/^[\p{L}\p{N}_]*/u)[0];
        if (word) {
          this.searchFor(word, command.name === "#", true);
        }
        break;
      }
      default:
        break;
    }
  }

  // Does the last change again, the text typed after it in insert mode among it.
  async repeat() {
    const last = this.last;
    if (!last) {
      return;
    }
    this.replaying = true;
    try {
      const command = this.parse(last.keys);
      if (command && command !== WAIT) {
        this.run(command, last.keys);
      }
      if (this.mode === "insert") {
        await this.editor.play(last.steps);
        this.leaveInsert();
      }
    } finally {
      this.replaying = false;
    }
  }

  // Visual mode.

  // The selection visual mode has made: whole lines in visual line mode, and from one end's letter
  // to the other's, both taken, in visual mode.
  span() {
    const { anchor, head } = this.visual;
    const [first, second] = cmp(anchor, head) <= 0 ? [anchor, head] : [head, anchor];
    if (this.mode === "line") {
      return { start: pos(first.line, 0), end: pos(second.line, this.line(second.line).length), lines: true };
    }
    return { start: first, end: pos(second.line, Math.min(this.line(second.line).length, second.col + 1)), lines: false };
  }

  drawChosen() {
    const { start, end } = this.span();
    const forward = cmp(this.visual.anchor, this.visual.head) <= 0;
    this.editor.select([{ anchor: forward ? start : end, head: forward ? end : start, goal: null }]);
  }

  leaveVisual(at) {
    this.visual = null;
    this.setMode("normal");
    this.put(at);
  }

  runChosen(command) {
    if (command.object) {
      const span = this.object(command.object, this.visual.head, command.count);
      if (span) {
        this.visual = { anchor: span.start, head: this.backward(span.end) ?? span.start };
        this.drawChosen();
      }
      return;
    }
    if (command.name === "o") {
      this.visual = { anchor: this.visual.head, head: this.visual.anchor };
      this.drawChosen();
      return;
    }
    const { start, end, lines } = this.span();
    const name = { x: "d", s: "c" }[command.name] ?? command.name;
    this.visual = null;
    this.setMode("normal");
    if (OPERATORS.has(name)) {
      this.apply(name, start, end, lines, command.register);
    } else if (name === "~") {
      this.toggleCase(start, end);
      this.put(start);
    } else if (name === "J") {
      this.join(start.line, Math.max(2, end.line - start.line + 1));
    } else if (name === "p" || name === "P") {
      const kept = this.registers.get(command.register ?? '"');
      if (kept) {
        this.store(null, this.doc.slice(start, end), lines, false);
        this.editor.change([{ from: start, to: end, text: kept.text }], null);
        this.put(start);
      }
    }
  }

}
