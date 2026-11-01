// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { cmp, pos, same, wordAt } from "../document.js";
import { BREAK_STRIP, caret, empty, LINE, MAC, PAD } from "../view.js";

// Pointer.

export function unitRange(unit, p) {
  if (unit === "word") {
    const word = wordAt(this.doc, p);
    return word ? [word.from, word.to] : [p, p];
  }
  if (unit === "line") {
    const next = p.line + 1 < this.doc.count ? pos(p.line + 1, 0) : pos(p.line, this.doc.line(p.line).length);
    return [pos(p.line, 0), next];
  }
  return [p, p];
}

export function onDown(event, fromGutter = false) {
  // A press with the other button on the gutter's strip opens the breakpoint's menu.
  if (this.s && fromGutter && event.button === 2 && this.onBreakpointMenu && event.clientX - this.gutter.getBoundingClientRect().left < BREAK_STRIP) {
    event.preventDefault();
    this.onBreakpointMenu(this.s.base + this.posAt(event).line, event);
    return;
  }
  if (!this.s || event.button !== 0) {
    return;
  }
  this.inputting();
  const box = this.scroller.getBoundingClientRect();
  if (!fromGutter && (event.clientX - box.left >= this.scroller.clientWidth || event.clientY - box.top >= this.scroller.clientHeight)) {
    return;
  }
  // A press on a test's mark opens its menu.
  if (fromGutter && event.target.dataset?.test !== undefined && this.onTestMark) {
    event.preventDefault();
    this.onTestMark(Number(event.target.dataset.test), event);
    return;
  }
  // A press in the gutter's strip by its left edge sets or clears a breakpoint on the line.
  if (fromGutter && this.onBreakpoint && event.clientX - this.gutter.getBoundingClientRect().left < BREAK_STRIP) {
    event.preventDefault();
    this.onBreakpoint(this.s.base + this.posAt(event).line);
    return;
  }
  const target = event.target;
  if (target.dataset?.fold !== undefined) {
    event.preventDefault();
    this.toggleFold(Number(target.dataset.fold));
    return;
  }
  if (target.dataset?.change !== undefined) {
    event.preventDefault();
    this.onChangeMark(Number(target.dataset.change));
    return;
  }
  if (target.dataset?.history !== undefined) {
    event.preventDefault();
    this.onHistory(this.s, Number(target.dataset.history));
    return;
  }
  event.preventDefault();
  this.focus();
  this.hover.hide();
  this.suggest.close();
  const p = this.posAt(event);
  const unit = fromGutter ? "line" : ["char", "word", "line"][Math.min(event.detail || 1, 3) - 1];
  // Ctrl and a click, Cmd on a Mac, goes to where the word under it is defined, where the editor's
  // owner says how.
  if ((MAC ? event.metaKey : event.ctrlKey) && !event.altKey && !event.shiftKey && !fromGutter && unit === "char" && this.onDefinition) {
    this.select([caret(p)]);
    this.onDefinition(p);
    return;
  }
  const sels = this.s.selections;
  let index;
  let anchorFrom;
  let anchorTo;
  if (((event.altKey && event.shiftKey) || (this.columnMode && !event.altKey && !event.shiftKey && unit === "char")) && !fromGutter) {
    const goal = (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw;
    this.dragging = { column: { row: this.rows().rowOf(p.line), v: Math.max(0, goal) } };
    this.dragTo(event);
  } else {
    if (event.altKey && unit === "char" && !fromGutter) {
      const hit = sels.findIndex((sel) => empty(sel) && same(sel.head, p));
      if (hit >= 0 && sels.length > 1) {
        this.select(sels.filter((_, at) => at !== hit));
        return;
      }
      [anchorFrom, anchorTo] = this.unitRange(unit, p);
      const added = { anchor: anchorFrom, head: anchorTo, goal: null };
      this.setSelections([...sels, added], added);
    } else if (event.shiftKey) {
      const kept = this.primary();
      anchorFrom = kept.anchor;
      anchorTo = kept.anchor;
      this.setSelections([{ anchor: kept.anchor, head: p, goal: null }]);
    } else {
      [anchorFrom, anchorTo] = this.unitRange(unit, p);
      // A press past a line's end puts the cursor there where cursors may go past line ends.
      const v = Math.round((event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw);
      const past = this.pastEnds && unit === "char" && p.col === this.doc.line(p.line).length && v > this.vcolOf(this.doc.line(p.line), p.col, p.line);
      this.setSelections([{ anchor: anchorFrom, head: anchorTo, goal: past ? v : null }]);
    }
    index = this.s.primary;
    // A press with Alt pressed that then moves selects a column from where it was pressed.
    const alt = event.altKey && !event.shiftKey && unit === "char" && !fromGutter;
    const pressedAt = { row: this.rows().rowOf(p.line), v: Math.max(0, (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw) };
    this.dragging = { unit, anchorFrom, anchorTo, index, alt: alt ? pressedAt : null };
    this.doc.seal();
    this.moved();
  }
  const move = (moved) => {
    this.lastPointer = moved;
    this.dragTo(moved);
  };
  const tick = window.setInterval(() => {
    const at = this.lastPointer;
    if (!at) {
      return;
    }
    const rect = this.scroller.getBoundingClientRect();
    if (at.clientY < rect.top) {
      this.scroller.scrollTop -= Math.min(LINE * 3, rect.top - at.clientY);
    } else if (at.clientY > rect.bottom) {
      this.scroller.scrollTop += Math.min(LINE * 3, at.clientY - rect.bottom);
    } else {
      return;
    }
    this.dragTo(at);
  }, 40);
  const up = () => {
    window.removeEventListener("mousemove", move);
    window.removeEventListener("mouseup", up);
    window.clearInterval(tick);
    this.dragging = null;
    this.lastPointer = null;
  };
  window.addEventListener("mousemove", move);
  window.addEventListener("mouseup", up);
}

export function dragTo(event) {
  const drag = this.dragging;
  if (!drag || !this.s) {
    return;
  }
  const p = this.posAt(event);
  if (drag.alt && !drag.column) {
    const v = (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw;
    if (this.rows().rowOf(p.line) !== drag.alt.row || Math.abs(v - drag.alt.v) >= 1) {
      drag.column = drag.alt;
    }
  }
  if (drag.column) {
    const rows = this.rows();
    const v = Math.max(0, (event.clientX - this.space.getBoundingClientRect().left - PAD) / this.cw);
    const here = rows.rowOf(p.line);
    const list = [];
    const step = here >= drag.column.row ? 1 : -1;
    for (let row = drag.column.row; ; row += step) {
      const line = rows.lineOf(row);
      const text = this.doc.line(line);
      list.push({ anchor: pos(line, this.colAtV(text, drag.column.v)), head: pos(line, this.colAtV(text, v)), goal: v, column: true });
      if (row === here) {
        break;
      }
    }
    this.setSelections(list, list.at(-1));
    this.moved();
    return;
  }
  const [from, to] = this.unitRange(drag.unit, p);
  let anchor = drag.anchorFrom;
  let head = to;
  if (cmp(from, drag.anchorFrom) < 0) {
    anchor = drag.anchorTo;
    head = from;
  }
  const sels = [...this.s.selections];
  const index = Math.min(this.s.primary, sels.length - 1);
  sels[index] = { anchor, head, goal: null };
  this.setSelections(sels, sels[index]);
  this.moved();
}

export function onPointerMove(event) {
  window.clearTimeout(this.hoverWait);
  if (this.dragging || event.buttons || !this.s?.language?.hover) {
    return;
  }
  const { clientX, clientY } = event;
  // What the pointer rests on stays while the pointer is on its name, a step off it, or on its way
  // to it, and no other name is asked about; once the pointer has left all of them, it goes a moment
  // after, or what the pointer rests on next takes its place.
  if (this.hover.keeps(clientX, clientY)) {
    this.hover.stay();
    return;
  }
  if (this.hover.shown) {
    this.hover.hideSoon();
  }
  this.hoverWait = window.setTimeout(() => this.hoverAt(clientX, clientY), 380);
}

// A language's hover may answer at once or later, as a language server does; an answer that
// comes after the pointer has gone elsewhere is dropped. Where nothing is there, what is shown goes
// as the pointer's leaving it asked.
export async function hoverAt(clientX, clientY) {
  if (!this.s?.language?.hover && !this.s?.diagnostics?.length && !this.valueAt) {
    return;
  }
  const asked = (this.hoverAsked = (this.hoverAsked ?? 0) + 1);
  const session = this.s;
  const p = this.posAt({ clientX, clientY });
  const rect = this.space.getBoundingClientRect();
  const x = clientX - rect.left;
  const text = this.doc.line(p.line);
  if (x > PAD + this.vcolOf(text, text.length, p.line) * this.cw + this.cw || x < PAD) {
    return;
  }
  const said = this.diagnosticsAt(p);
  // While a program is stopped, the value of the name under the pointer comes first.
  const value = this.valueAt ? await this.valueAt(this.s, p) : null;
  const found = value ? null : this.s.language?.hover ? await this.s.language.hover(this.doc, p) : null;
  if (asked !== this.hoverAsked || this.s !== session) {
    return;
  }
  const parts = [...(value ? [value.node] : []), ...said, ...(found?.parts ?? [])];
  if (!parts.length) {
    return;
  }
  let from = value?.from ?? found?.from ?? p;
  if (!found && !value) {
    let col = p.col;
    while (col > 0 && /\w/.test(text[col - 1])) {
      col -= 1;
    }
    from = { line: p.line, col };
  }
  const to = value?.to ?? found?.to ?? p;
  this.hover.show({ from, to, parts });
  // What a server adds comes later; it joins the hover where the pointer still rests on it.
  const more = found?.more ? await found.more : null;
  if (more?.length && asked === this.hoverAsked && this.s === session && this.hover.shown) {
    this.hover.show({ from, to, parts: [...parts, ...more] });
  }
}

// What the diagnostics under a place say, each in its severity's color: a language server's, or a
// tool plugin's, with its code, a link to the page that tells of it where the server names one. A
// message the server writes in Markdown stands under its heading as written.
export function diagnosticsAt(p) {
  const line = p.line + this.s.base;
  const names = ["", "error", "warning", "note", "hint"];
  return (this.s.diagnostics ?? [])
    .filter(
      (diag) =>
        (line > diag.from.line || (line === diag.from.line && p.col >= diag.from.col)) &&
        (line < diag.to.line || (line === diag.to.line && p.col <= Math.max(diag.to.col, diag.from.col + 1))),
    )
    .map((diag) => {
      const code = diag.code ? (diag.href ? ` [${diag.code}](${diag.href})` : ` ${diag.code}`) : "";
      const head = `**${names[diag.severity] ?? "note"}**${diag.source ? ` ${diag.source}` : ""}${code}`;
      return { className: `diag s${diag.severity}`, text: diag.markdown ? `${head}\n\n${diag.message}` : `${head}: ${diag.message}` };
    });
}

export function bind() {
  this.keys = this.keyMap();
  this.bound = {};
  this.first = {};
  this.native = new Set();
  this.vim = null;
  this.recording = null;
  const input = this.input;
  // A key draws what it changed in its own event, and never waits on the frame after.
  const drawn = () => this.pending && this.paint();
  input.addEventListener("keydown", (event) => {
    this.inputting();
    this.onKey(event);
    drawn();
  });
  input.addEventListener("compositionstart", () => {
    this.composing = true;
    this.host.dataset.composing = "true";
  });
  input.addEventListener("compositionend", (event) => {
    this.composing = false;
    delete this.host.dataset.composing;
    const text = event.data || input.value;
    input.value = "";
    if (text && this.s) {
      this.note({ text });
      this.type(text);
    }
  });
  input.addEventListener("input", () => {
    this.inputting();
    if (this.composing) {
      return;
    }
    const text = input.value.replace(/\r\n?/g, "\n");
    input.value = "";
    if (text && this.s) {
      this.note({ text });
      this.type(text);
    }
    drawn();
  });
  input.addEventListener("copy", (event) => this.s && this.copy(event, false));
  input.addEventListener("cut", (event) => this.s && this.copy(event, true));
  input.addEventListener("paste", (event) => this.s && this.paste(event));
  input.addEventListener("focus", () => {
    this.host.dataset.focus = "true";
    this.schedule();
  });
  input.addEventListener("blur", () => {
    this.host.dataset.focus = "false";
    this.schedule();
  });
  this.scroller.addEventListener("mousedown", (event) => this.onDown(event));
  this.gutter.addEventListener("mousedown", (event) => this.onDown(event, true));
  this.scroller.addEventListener("mousemove", (event) => this.onPointerMove(event));
  this.scroller.addEventListener("mouseleave", (event) => {
    window.clearTimeout(this.hoverWait);
    this.hoverAsked = (this.hoverAsked ?? 0) + 1;
    if (!this.hover.holds(event.relatedTarget)) {
      this.hover.hideSoon();
    }
  });
  this.scroller.addEventListener("scroll", () => this.onScroll());
  this.scroller.addEventListener("contextmenu", (event) => event.preventDefault());
  this.scroller.addEventListener("wheel", (event) => this.onWheel(event), { passive: false });
  this.gutter.addEventListener("wheel", (event) => this.onWheel(event), { passive: false });
}
