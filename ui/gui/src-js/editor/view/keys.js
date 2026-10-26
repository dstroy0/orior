// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { pos } from "../document.js";
import { Vim } from "../vim.js";
import { LIST_KEYS } from "../widgets.js";
import { caret, div, empty, endOfSel, JUMP_LABELS, keyName, keysName, LINE, MAC, startOf } from "../view.js";

// Keys.

export function keyMap() {
  const move = (step) => (extend) => () => this.moveBy(step, extend);
  const left = move((sel, extend) => {
    if (!extend && this.pastEnd(sel) > 0) {
      return { p: sel.head, goal: this.vcol(sel.head) + this.pastEnd(sel) - 1 };
    }
    return !extend && !empty(sel) ? startOf(sel) : this.charLeft(sel.head);
  });
  const right = move((sel, extend) => {
    if (!extend && empty(sel) && this.pastEnds && sel.head.col === this.doc.line(sel.head.line).length) {
      return { p: sel.head, goal: this.vcol(sel.head) + this.pastEnd(sel) + 1 };
    }
    return !extend && !empty(sel) ? endOfSel(sel) : this.charRight(sel.head);
  });
  const up = move((sel) => this.vertical(sel, -1));
  const down = move((sel) => this.vertical(sel, 1));
  const wordLeft = move((sel) => this.wordLeft(sel.head));
  const wordRight = move((sel) => this.wordRight(sel.head));
  const lineStart = move((sel) => this.home(sel));
  const lineEnd = move((sel) => pos(sel.head.line, this.doc.line(sel.head.line).length));
  const docStart = move(() => pos(0, 0));
  const docEnd = move(() => this.doc.end());
  const both = (name, make) => ({ [name]: make(false), [`Shift+${name}`]: make(true) });
  const keys = {
    ...both("Left", left),
    ...both("Right", right),
    ...both("Up", up),
    ...both("Down", down),
    ...both("Home", lineStart),
    ...both("End", lineEnd),
    PageUp: () => this.page(-1, false),
    PageDown: () => this.page(1, false),
    "Shift+PageUp": () => this.page(-1, true),
    "Shift+PageDown": () => this.page(1, true),
    Backspace: () => this.backspace(),
    "Shift+Backspace": () => this.backspace(),
    Delete: () => this.deleteForward(),
    Enter: () => (this.suggest.open ? this.suggest.accept() : this.newline()),
    "Shift+Enter": () => this.newline(),
    "Mod+Enter": () => this.insertLine(true),
    "Mod+Shift+Enter": () => this.insertLine(false),
    Tab: () => this.tab(false),
    "Shift+Tab": () => this.tab(true),
    Escape: () => this.escape(),
    "Mod+A": () => this.selectAll(),
    "Mod+Z": () => this.undo(true),
    "Mod+Shift+Z": () => this.undo(false),
    "Mod+Y": () => this.undo(false),
    "Mod+D": () => this.addMatch(false),
    "Mod+Shift+L": () => this.addMatch(true),
    "Mod+L": () => this.selectLine(),
    "Mod+J": () => this.joinLines(),
    "Mod+/": () => this.toggleComment(),
    "Alt+Up": () => this.moveLines(-1),
    "Alt+Down": () => this.moveLines(1),
    "Shift+Alt+Up": () => this.copyLines(-1),
    "Alt+Shift+Right": () => this.expandSelection(),
    "Alt+Shift+Left": () => this.shrinkSelection(),
    "Shift+Alt+Down": () => this.copyLines(1),
    "Alt+Shift+Up": () => this.copyLines(-1),
    "Alt+Shift+Down": () => this.copyLines(1),
    "Mod+Shift+K": () => this.deleteLines(),
    "Mod+Alt+Up": () => this.addCursor(-1),
    "Mod+Alt+Down": () => this.addCursor(1),
    "Mod+]": () => this.indentLines(1),
    "Mod+[": () => this.indentLines(-1),
    "Mod+Shift+[": () => this.fold(this.primary().head.line, true),
    "Mod+Shift+]": () => this.fold(this.primary().head.line, false),
    "Mod+Shift+\\": () => this.jumpBracket(),
    "Mod+F": () => this.find.open(false),
    "Mod+H": () => this.find.open(true),
    F3: () => this.find.step(1),
    "Shift+F3": () => this.find.step(-1),
    "Mod+G": () => this.goto.open(),
    "Mod+Space": () => this.suggest.update(true, true),
    "Mod+K": () => (this.chord = "Mod+K"),
    "Mod+K Mod+0": () => this.foldAll(true),
    "Mod+K Mod+J": () => this.foldAll(false),
  };
  if (MAC) {
    Object.assign(keys, {
      ...both("Mod+Left", lineStart),
      ...both("Mod+Right", lineEnd),
      ...both("Alt+Left", wordLeft),
      ...both("Alt+Right", wordRight),
      ...both("Mod+Up", docStart),
      ...both("Mod+Down", docEnd),
      "Alt+Backspace": () => this.backspace(true),
      "Alt+Delete": () => this.deleteForward(true),
      "Mod+Backspace": () => this.deleteToStart(),
    });
    for (const name of ["Mod+Left", "Mod+Right", "Mod+Up", "Mod+Down"]) {
      keys[name.replace("Mod+", "Mod+Shift+")] = keys[`Shift+${name}`];
    }
    for (const name of ["Alt+Left", "Alt+Right"]) {
      keys[name.replace("Alt+", "Alt+Shift+")] = keys[`Shift+${name}`];
    }
  } else {
    Object.assign(keys, {
      ...both("Mod+Left", wordLeft),
      ...both("Mod+Right", wordRight),
      ...both("Mod+Home", docStart),
      ...both("Mod+End", docEnd),
      "Mod+Backspace": () => this.backspace(true),
      "Mod+Delete": () => this.deleteForward(true),
      "Mod+Up": () => (this.scroller.scrollTop -= LINE),
      "Mod+Down": () => (this.scroller.scrollTop += LINE),
    });
    for (const name of ["Mod+Left", "Mod+Right", "Mod+Home", "Mod+End"]) {
      keys[name.replace("Mod+", "Mod+Shift+")] = keys[`Shift+${name}`];
    }
  }
  return keys;
}

// Binds keys written as a menu lists them, such as "Shift+Alt+F" or "Ctrl+K Ctrl+I", each to its
// `run`, in place of those the menus bound before. A key the editor already binds keeps its own.
// The keys of the menus' commands for the editor. A command marked `native` leaves its key to the
// browser, which does the work itself: a paste's text then comes in the paste event, straight from
// the system's clipboard, and never crosses to the app's side and back as text.
export function addKeys(list) {
  this.keys = this.keyMap();
  this.first = {};
  this.native = new Set();
  for (const { keys, run, first, native } of list) {
    if (native) {
      this.native.add(keysName(keys));
    } else if (first) {
      this.first[keysName(keys)] = run;
    } else {
      this.keys[keysName(keys)] ??= run;
    }
  }
}

// The keys the reader binds, in place of any the reader bound before, each over the editor's own
// for the same keys.
export function bindKeys(list) {
  this.bound = Object.fromEntries(list.map(({ keys, run }) => [keysName(keys), run]));
}

export function onKey(event) {
  if (!this.s || this.composing || event.isComposing) {
    return;
  }
  if (this.jump) {
    this.jumpKey(event);
    return;
  }
  if (this.vim?.key(event)) {
    this.hover.hide();
    return;
  }
  this.hover.hide();
  const name = keyName(event);
  if (LIST_KEYS.has(name)) {
    this.suggest.flush();
  }
  if (this.suggest.open && this.suggest.key(name)) {
    event.preventDefault();
    return;
  }
  const full = this.chord ? `${this.chord} ${name}` : name;
  // A key offered first to a command takes it where the command takes it, as Run Statement takes
  // Ctrl+Enter in an SQL file, and leaves it to the editor's own use where the command declines.
  if (this.first[full]?.() === true) {
    event.preventDefault();
    this.chord = null;
    this.note({ key: full });
    return;
  }
  // A key the reader bound is told which editor it was pressed in.
  const own = this.bound[full];
  if (!own && this.native?.has(full)) {
    this.chord = null;
    this.note({ key: full });
    return;
  }
  const found = own ? () => own(this) : this.keys[full];
  if (this.chord) {
    if (!/^(?:Mod|Alt|Shift|Ctrl)\+(?:Control|Meta|Alt|Shift)$|^(?:Control|Meta|Alt|Shift)$/.test(name)) {
      this.chord = null;
    }
    if (!found) {
      return;
    }
  }
  if (found) {
    event.preventDefault();
    this.note({ key: full });
    found();
  }
}

// Jump: the letter or two typed after it are looked for in the lines in sight, case aside, and each
// place they stand gets a mark of a letter of its own, the nearest the cursor first; typing a mark's
// letter puts the cursor there. After one letter typed, a mark's letter is never one that follows a
// place found, which leaves that letter free to look for two. Escape, a press, a scroll, or any key
// that is none of these ends it.
export function startJump() {
  this.endJump();
  this.jump = { query: "", marks: [], layer: div("ed-jump") };
  document.body.append(this.jump.layer);
  this.host.dataset.jump = "true";
  const end = () => this.endJump();
  this.jump.unbind = [
    ["scroll", this.scroller],
    ["mousedown", document],
    ["blur", this.input],
  ].map(([name, target]) => (target.addEventListener(name, end, { once: true }), () => target.removeEventListener(name, end)));
}

export function endJump() {
  if (!this.jump) {
    return;
  }
  this.jump.layer.remove();
  this.jump.unbind.forEach((unbind) => unbind());
  this.jump = null;
  delete this.host.dataset.jump;
}

export function jumpKey(event) {
  const jump = this.jump;
  const letter = event.key.length === 1 && !event.ctrlKey && !event.altKey && !event.metaKey ? event.key : null;
  if (["Shift", "Control", "Alt", "Meta"].includes(event.key)) {
    return;
  }
  event.preventDefault();
  const mark = letter && jump.marks.find((one) => one.label === letter.toLowerCase());
  if (mark) {
    this.endJump();
    this.setSelections([caret(mark.at)], caret(mark.at));
    this.moved();
    this.reveal();
    return;
  }
  if (!letter || jump.query.length >= 2) {
    this.endJump();
    return;
  }
  jump.query += letter;
  this.markJumps();
}

// Marks every place in sight the query stands, the nearest the cursor first, while labels last.
export function markJumps() {
  const jump = this.jump;
  const wanted = jump.query.toLowerCase();
  const rows = this.rows();
  const box = this.scroller.getBoundingClientRect();
  const first = Math.max(0, Math.floor((this.scroller.scrollTop - this.pad) / LINE));
  const last = Math.min(rows.size - 1, Math.ceil((this.scroller.scrollTop + this.scroller.clientHeight - this.pad) / LINE));
  const head = this.primary().head;
  const found = [];
  for (let row = first; row <= last; row += 1) {
    const line = rows.lineOf(row);
    const text = this.doc.line(line).toLowerCase();
    for (let col = text.indexOf(wanted); col >= 0; col = text.indexOf(wanted, col + 1)) {
      const at = pos(line, col);
      const rect = this.rectOf(at);
      if (rect.left >= box.left && rect.left < box.right) {
        found.push({ at, rect, after: text[col + wanted.length] ?? "", far: Math.abs(line - head.line) * 1000 + Math.abs(col - head.col) });
      }
    }
  }
  found.sort((a, b) => a.far - b.far);
  const taken = jump.query.length === 1 ? new Set(found.map((one) => one.after)) : new Set();
  const labels = [...JUMP_LABELS].filter((label) => !taken.has(label));
  jump.marks = found.slice(0, labels.length).map((one, index) => ({ ...one, label: labels[index] }));
  jump.layer.replaceChildren(
    ...jump.marks.map(({ rect, label }) => {
      const node = div("ed-jump-mark");
      node.textContent = label;
      node.style.left = `${rect.left}px`;
      node.style.top = `${rect.top}px`;
      return node;
    })
  );
  if (!found.length) {
    this.endJump();
  }
}

// Each key the editor acts on, each text typed, and each cut, copy and paste, told to a recording
// and to whatever watches the steps, as Vim's `.` does while it takes the text typed after a change.
export function note(step) {
  this.recording?.push(step);
  this.stepWatch?.(step);
}

// Vim's keys, on or off.
// Shows the line at the width the file's formatter keeps lines to, or takes it away.
export function setMargin(on) {
  this.marginShown = on;
  this.follow();
}

export function setVim(on) {
  this.vim?.end();
  this.vim = on ? new Vim(this) : null;
  this.statusSaid = null;
  this.schedule();
}

// Recording: each key the editor acts on, each text typed, each cut, copy and paste, until it stops
// and gives the steps.
export function record() {
  this.recording = [];
}

export function stopRecording() {
  const steps = this.recording ?? [];
  this.recording = null;
  return steps;
}

// Plays steps back as the keys and the typing did, a paste after a cut or a copy among them taking
// what that cut or copy took, and any other the text it pasted as recorded.
export async function play(steps) {
  let clip = null;
  for (const step of steps) {
    if (step.key) {
      const own = this.bound[step.key];
      await (own ? own(this) : this.keys[step.key]?.());
      this.chord = null;
    } else if (step.text) {
      this.type(step.text);
    } else if (step.cut !== undefined) {
      clip = this.takeOut(step.cut);
    } else if (step.paste !== undefined) {
      this.pasteText(clip ?? step.paste);
    }
  }
  this.schedule();
}
