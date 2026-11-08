// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Vim's motions and text objects: where each motion goes from a place, and the span each text object takes. Each is a
// method of Vim (editor/vim.js), `this` the Vim it runs for.

import { cmp, pos } from "../document.js";

const KEYWORD = /[\p{L}\p{N}_]/u;
const PAIRS = { "(": "()", ")": "()", b: "()", "[": "[]", "]": "[]", "{": "{}", "}": "{}", B: "{}", "<": "<>", ">": "<>" };
const QUOTES = new Set(['"', "'", "`"]);

// Motions.

export function charAt(p) {
  const text = this.line(p.line);
  return p.col < text.length ? text[p.col] : "\n";
}

export function forward(p) {
  if (p.col < this.line(p.line).length) {
    return pos(p.line, p.col + 1);
  }
  return p.line + 1 < this.doc.count ? pos(p.line + 1, 0) : null;
}

export function backward(p) {
  if (p.col > 0) {
    return pos(p.line, p.col - 1);
  }
  return p.line > 0 ? pos(p.line - 1, this.line(p.line - 1).length) : null;
}

// A letter's type: 0 for space and line ends, 1 for a word's letters, 2 for any other; with `big`,
// 1 for every letter but space.
export function type(letter, big) {
  if (letter === "\n" || /\s/.test(letter)) {
    return 0;
  }
  return big || KEYWORD.test(letter) ? 1 : 2;
}

export function wordStart(p, big) {
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

export function wordEnd(p, big) {
  let at = this.forward(p);
  while (at && this.kind(this.charAt(at), big) === 0) {
    at = this.forward(at);
  }
  if (!at) {
    return p;
  }
  const type = this.kind(this.charAt(at), big);
  for (let next = this.forward(at); next && this.kind(this.charAt(next), big) === type; next = this.forward(next)) {
    at = next;
  }
  return at;
}

export function wordBack(p, big) {
  let at = this.backward(p);
  while (at && this.kind(this.charAt(at), big) === 0 && !(at.col === 0 && this.line(at.line).length === 0)) {
    at = this.backward(at);
  }
  if (!at) {
    return pos(0, 0);
  }
  const type = this.kind(this.charAt(at), big);
  for (let back = this.backward(at); type && back && back.line === at.line && this.kind(this.charAt(back), big) === type; back = this.backward(back)) {
    at = back;
  }
  return at;
}

// The next or the last letter on the line that is `letter`, `times` over: on it for f and F, and
// short of it for t and T.
export function findOnLine(p, how, letter, times) {
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
export function pairOf(p) {
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

export function paragraph(p, dir, times) {
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
export function searchFrom(p, searched, back) {
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
export function motion(motion, count, from) {
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

export function toLetter(from, how, letter, times) {
  const to = this.findOnLine(from, how, letter, times);
  return to && { to, inclusive: how === "f" || how === "t" };
}

export function moveBy(motion, count) {
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
export function object({ kind: type, of }, from, count) {
  const inner = type === "i";
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
        if (inner) {
          return { start: pos(from.line, open + 1), end: pos(from.line, close) };
        }
        // `a` takes the white space after the closing quote, or where none follows, the white space before the
        // opening one.
        let start = open;
        let end = close + 1;
        while (end < text.length && /\s/.test(text[end])) {
          end += 1;
        }
        if (end === close + 1) {
          while (start > 0 && /\s/.test(text[start - 1])) {
            start -= 1;
          }
        }
        return { start: pos(from.line, start), end: pos(from.line, end) };
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

