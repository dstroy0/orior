// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's keys for the editor, where Preferences sets them on. The editor asks this of each key first,
// and what this does not take goes to the editor's own keys, which in insert mode is every key but
// Escape, and in the other modes every key pressed with Ctrl that Vim gives nothing to.
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

import { pos } from "./document.js";
import * as commands from "./vim/commands.js";
import { ARROWS, CHANGES, COMMANDS, MOTIONS, NAMES, OPERATORS, WAIT } from "./vim/keys.js";
import * as motions from "./vim/motions.js";

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
}

// Vim's motions and text objects, and its operators, commands and visual mode, kept in editor/vim/ and put on the
// class as its own methods are: not counted among an object's keys.
for (const part of [motions, commands]) {
  for (const [name, method] of Object.entries(part)) {
    Object.defineProperty(Vim.prototype, name, { value: method, writable: true, configurable: true });
  }
}
