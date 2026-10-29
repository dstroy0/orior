// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { wordBefore } from "../document.js";
import { enter, leave } from "../../trace.js";
import { icon } from "../../icons.js";
import { empty, endOfSel, LINE, PAD, startOf, STICKY_KEY, STICKY_MOST } from "../view.js";

// Moves the cursor to the next of the language server's diagnostics after it, or the one before
// it where `dir` is -1, round from the end to the start, and says what it is.
export function stepProblem(dir) {
  const s = this.s;
  const all = [...(s?.diagnostics ?? [])].sort((a, b) => a.from.line - b.from.line || a.from.col - b.from.col);
  if (!all.length) {
    return null;
  }
  const head = this.primary().head;
  const at = { line: head.line + s.base, col: head.col };
  const after = (diag) => diag.from.line > at.line || (diag.from.line === at.line && diag.from.col > at.col);
  const before = (diag) => diag.from.line < at.line || (diag.from.line === at.line && diag.from.col < at.col);
  const found = dir > 0 ? all.find(after) ?? all[0] : [...all].reverse().find(before) ?? all.at(-1);
  this.goTo(found.from.line - s.base, found.from.col);
  return found;
}

// The status line, built again only when what it says changes.
export function drawStatus() {
  const s = this.s;
  const primary = this.primary();
  const head = primary.head;
  let picked = "";
  const spans = s.selections.filter((sel) => !empty(sel));
  if (spans.length) {
    const lines = spans.reduce((sum, sel) => sum + endOfSel(sel).line - startOf(sel).line, 0);
    const chars = spans.reduce((sum, sel) => sum + (startOf(sel).line === endOfSel(sel).line ? endOfSel(sel).col - startOf(sel).col : 0), 0);
    picked = lines ? `${lines + spans.length} lines selected` : `${chars} selected`;
  }
  const errors = (s.diagnostics ?? []).filter((diag) => diag.severity === 1).length;
  const warnings = (s.diagnostics ?? []).filter((diag) => diag.severity === 2).length;
  const mode = this.vim?.label() ?? "";
  const said = [s.base + head.line, this.vcol(head) + this.pastEnd(primary), picked, s.selections.length, s.indent.tabs, s.indent.size, s.doc.eol, s.language?.id, errors, warnings, s.readOnly, mode].join("|");
  if (said === this.statusSaid) {
    return;
  }
  this.statusSaid = said;
  const parts = [];
  const where = document.createElement("button");
  where.type = "button";
  where.textContent = `${s.base + head.line + 1}:${this.vcol(head) + this.pastEnd(primary) + 1}`;
  where.title = `Line ${s.base + head.line + 1}, column ${this.vcol(head) + 1}: Go to Line (Ctrl+G)`;
  where.addEventListener("click", () => this.goto.open());
  where.dataset.item = "cursor";
  // Each part names itself, for the reader to move it or hide it.
  const named = (item, props) => {
    const made = Object.assign(document.createElement("span"), props);
    made.dataset.item = item;
    return made;
  };
  if (mode) {
    parts.push(named("vim", { className: "status-vim", textContent: mode }));
  }
  parts.push(where);
  if (picked) {
    parts.push(named("selection", { textContent: picked }));
  }
  if (s.selections.length > 1) {
    parts.push(named("cursors", { textContent: `${s.selections.length} cursors` }));
  }
  // Then the line ends, the encoding every file is read and written in, the indent, which a press
  // turns between tabs and spaces, and the lock, which a press turns where the editor's owner says
  // the text can be written.
  const eol = named("eol", { textContent: s.doc.eol === "\r\n" ? "CRLF" : "LF", title: "Line ends" });
  const encoding = named("encoding", { textContent: "UTF-8", title: "Encoding" });
  const indent = document.createElement("button");
  indent.dataset.item = "indent";
  indent.type = "button";
  indent.textContent = s.indent.tabs ? `Tab ${s.indent.size}` : `${s.indent.size} spaces`;
  indent.title = s.indent.tabs ? "Indent with tabs: a press indents with spaces" : "Indent with spaces: a press indents with tabs";
  indent.addEventListener("click", () => {
    s.indent.tabs = !s.indent.tabs;
    this.paint();
  });
  const lock = document.createElement("button");
  lock.type = "button";
  lock.className = "status-lock";
  lock.dataset.item = "lock";
  lock.title = s.readOnly ? "Read-only: a press makes it writable" : "Writable: a press makes it read-only";
  lock.setAttribute("aria-label", s.readOnly ? "Read-only" : "Writable");
  lock.append(icon(s.readOnly ? "lock" : "unlock"));
  lock.addEventListener("click", () => this.onLock?.(s));
  parts.push(eol, encoding, indent, lock);
  this.status.replaceChildren(...parts);
}

// Sticky scroll: the line that opens each region the editor's top row is inside, held along the
// top, the outermost first. A click on one goes to it, and the wheel over them scrolls the text.
export function setSticky(on) {
  this.stickyOn = on;
  localStorage.setItem(STICKY_KEY, String(on));
  this.stickyKey = "";
  this.schedule();
}

// The regions the top row stands inside are the session's folds that hold it, found again each time the top row, the
// text or the folds change.
export function drawSticky(rows, top, left = this.scroller.scrollLeft) {
  const s = this.s;
  if (!this.stickyOn) {
    this.sticky.hidden = true;
    this.stickyKey = "";
    return;
  }
  const firstRow = Math.floor(top / LINE);
  const heldFor = `${s.doc.id}|${firstRow}|${rows.size}|${s.highlight.foldsCame}`;
  let held = this.stickyHeldFor === heldFor ? this.stickyHeld : [];
  // The held lines cover rows of their own, and they hold the regions of the row under them.
  for (let pass = 0; pass < 3 && this.stickyHeldFor !== heldFor; pass += 1) {
    const line = rows.lineOf(Math.min(rows.size - 1, firstRow + held.length));
    const openersCall = enter("openers");
    const next = s.openersOf(line, STICKY_MOST);
    leave(openersCall);
    if (next.join() === held.join()) {
      break;
    }
    held = next;
  }
  this.stickyHeldFor = heldFor;
  this.stickyHeld = held;
  if (top <= 0) {
    held = [];
  }
  const gutter = this.gutterWidth ?? this.gutter.offsetWidth;
  const key = `${held.join()}|${s.doc.id}|${left}|${gutter}|${this.viewSize().width}|${s.base}`;
  if (key === this.stickyKey) {
    return;
  }
  this.stickyKey = key;
  this.sticky.hidden = !held.length;
  if (!held.length) {
    return;
  }
  this.sticky.style.width = `${gutter + this.viewSize().width}px`;
  const heldCall = enter("held");
  this.sticky.innerHTML = held
    .map(
      (line) =>
        `<div class="ed-sticky-row" data-line="${line}" style="height:${LINE}px"><span class="ed-sticky-num" style="width:${gutter}px">${s.base + line + 1}</span><span class="ed-sticky-text"><span style="display:inline-block;padding-left:${PAD}px;transform:translateX(${-left}px)">${this.rowHtml(line)}</span></span></div>`
    )
    .join("");
  leave(heldCall);
}

// Colors the minimap reads from the stylesheet, read again once the scheme changes.
export function refreshColors() {
  this.minimap.colors = null;
  this.schedule();
}

// The rectangle, in the page, of a place's row.
export function rectOf(p) {
  const rect = this.space.getBoundingClientRect();
  const row = this.rows().rowAt(p.line, p.col);
  const top = rect.top + this.pad;
  return { left: rect.left + this.xOf(p), top: top + row * LINE, bottom: top + (row + 1) * LINE };
}

// The word before the primary cursor, which completion replaces.
export function prefix() {
  return wordBefore(this.doc, this.primary().head);
}
