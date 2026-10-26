// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's operators, commands and visual mode: what each does to the text, the registers, the search prompt, and `.`.
// Each is a method of Vim (editor/vim.js), `this` the Vim it runs for.

import { cmp, endOf, pos } from "../document.js";
import { OPERATORS, WAIT } from "./keys.js";

// Operators.

export function operate(command) {
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

export function store(register, text, lines, yanked) {
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

export function apply(name, start, end, lines, register) {
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

export function insertAt(p) {
  this.setMode("insert");
  this.put(p);
}

export function leaveInsert() {
  const head = this.editor.primary().head;
  this.setMode("normal");
  this.put(pos(head.line, Math.max(0, head.col - 1)));
  if (this.taking) {
    this.last = this.taking;
    this.taking = null;
  }
}

export function paste(command, before) {
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

export function join(first, count) {
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

export function toggleCase(start, end) {
  const text = this.doc.slice(start, end);
  const turned = [...text].map((letter) => (letter === letter.toLowerCase() ? letter.toUpperCase() : letter.toLowerCase())).join("");
  this.editor.change([{ from: start, to: end, text: turned }], null);
}

export function openPrompt(back) {
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
export function searchFor(text, back, whole) {
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

export function runCommand(command) {
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
export async function repeat() {
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
export function span() {
  const { anchor, head } = this.visual;
  const [first, second] = cmp(anchor, head) <= 0 ? [anchor, head] : [head, anchor];
  if (this.mode === "line") {
    return { start: pos(first.line, 0), end: pos(second.line, this.line(second.line).length), lines: true };
  }
  return { start: first, end: pos(second.line, Math.min(this.line(second.line).length, second.col + 1)), lines: false };
}

export function drawChosen() {
  const { start, end } = this.span();
  const forward = cmp(this.visual.anchor, this.visual.head) <= 0;
  this.editor.select([{ anchor: forward ? start : end, head: forward ? end : start, goal: null }]);
}

export function leaveVisual(at) {
  this.visual = null;
  this.setMode("normal");
  this.put(at);
}

export function runChosen(command) {
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
