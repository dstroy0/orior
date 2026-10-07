// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panels that open at the text: what a word means, from the language's hover, and the words
// that can go where the cursor is, from its completion.
//
// A language's hover(doc, at) answers { from, to, parts } or null, each part a short text in which
// **bold**, `code` and _slanted_ are marked. Its complete(doc, at) answers items { label, kind,
// detail, doc, insert, snippet }, where a snippet's insert has stops written ${1:name}.

import { pos } from "./document.js";
import { empty, endOfSel, escapeHtml, parseSnippet, startOf } from "./view.js";

// A part as markup: code first, and nothing inside it is read as bold or slanted.
export function markup(text) {
  return text
    .split(/\n{2,}/)
    .map((paragraph) =>
      paragraph
        .split(/(`[^`]*`)/)
        .map((piece, index) => {
          if (index % 2) {
            return `<code>${escapeHtml(piece.slice(1, -1))}</code>`;
          }
          return escapeHtml(piece)
            .replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
            .replace(/(^|[^\p{L}\p{N}])_([^_]+)_(?![\p{L}\p{N}])/gu, "$1<i>$2</i>")
            .replace(/\n/g, "<br>");
        })
        .join("")
    )
    .map((paragraph) => `<p>${paragraph}</p>`)
    .join("");
}

export class Hover {
  constructor(editor) {
    this.ed = editor;
    this.shown = false;
    this.el = document.createElement("div");
    this.el.className = "ed-hover";
    this.el.hidden = true;
    this.el.addEventListener("mouseenter", () => window.clearTimeout(this.wait));
    this.el.addEventListener("mouseleave", () => this.hideSoon());
    this.el.addEventListener("mousedown", (event) => event.stopPropagation());
    editor.host.append(this.el);
  }

  holds(node) {
    return Boolean(node) && this.el.contains(node);
  }

  show(found) {
    window.clearTimeout(this.wait);
    this.el.innerHTML = found.parts.map(markup).join("<hr>");
    this.el.hidden = false;
    this.shown = true;
    const host = this.ed.host.getBoundingClientRect();
    const at = this.ed.rectOf(found.from);
    const height = this.el.offsetHeight;
    const width = this.el.offsetWidth;
    const above = at.top - host.top - height - 4;
    const top = above >= 0 ? above : at.bottom - host.top + 4;
    const left = Math.max(4, Math.min(at.left - host.left, host.width - width - 12));
    this.el.style.left = `${left}px`;
    this.el.style.top = `${top}px`;
  }

  hide() {
    window.clearTimeout(this.wait);
    if (this.shown) {
      this.el.hidden = true;
      this.shown = false;
    }
  }

  hideSoon() {
    window.clearTimeout(this.wait);
    this.wait = window.setTimeout(() => this.hide(), 300);
  }
}

const SHOWN_ROWS = 10;

// How well a label answers what is typed: a start beats a part, a part beats letters in order,
// and nothing is 0.
function score(label, typed) {
  if (!typed) {
    return 1;
  }
  const lower = label.toLowerCase();
  const want = typed.toLowerCase();
  if (label.startsWith(typed)) {
    return 5;
  }
  if (lower.startsWith(want)) {
    return 4;
  }
  if (lower.includes(want)) {
    return 3;
  }
  let at = 0;
  for (const char of lower) {
    if (char === want[at]) {
      at += 1;
      if (at === want.length) {
        return 2;
      }
    }
  }
  return 0;
}

export class Suggest {
  constructor(editor) {
    this.ed = editor;
    this.open = false;
    this.items = null;
    this.shown = [];
    this.index = 0;
    this.list = document.createElement("div");
    this.list.className = "ed-suggest-list";
    this.detail = document.createElement("div");
    this.detail.className = "ed-suggest-detail";
    this.el = document.createElement("div");
    this.el.className = "ed-suggest";
    this.el.hidden = true;
    this.el.append(this.list, this.detail);
    this.el.addEventListener("mousedown", (event) => {
      event.preventDefault();
      event.stopPropagation();
      const row = event.target.closest(".ed-item");
      if (row) {
        this.index = Number(row.dataset.index);
        this.accept();
      }
    });
    editor.host.append(this.el);
  }

  // Opens or filters the list for the word before the cursor. `start` opens it where it is shut,
  // and `forced` opens it with nothing typed yet.
  update(start, forced = false) {
    const ed = this.ed;
    const language = ed.s?.language;
    if (!language?.complete || ed.s.selections.length === 0) {
      this.close();
      return;
    }
    if (!this.open && !start) {
      return;
    }
    const prefix = ed.prefix();
    if (!prefix.text && !forced && !this.open) {
      return;
    }
    const moved = !this.at || this.at.line !== prefix.from.line || this.at.col !== prefix.from.col;
    if (!this.items || moved || forced) {
      this.items = language.complete(ed.doc, ed.primary().head) ?? [];
      this.at = prefix.from;
    }
    const kept = this.shown[this.index]?.label;
    this.shown = this.items
      .map((item) => ({ item, score: score(item.label, prefix.text) }))
      .filter((one) => one.score > 0)
      .sort((a, b) => b.score - a.score || a.item.label.length - b.item.label.length || a.item.label.localeCompare(b.item.label))
      .slice(0, 400)
      .map((one) => one.item);
    if (!this.shown.length || (this.shown.length === 1 && this.shown[0].label === prefix.text && !forced)) {
      this.close();
      return;
    }
    this.index = Math.max(0, this.shown.findIndex((item) => item.label === kept));
    if (!this.open) {
      this.first = 0;
    }
    this.open = true;
    this.el.hidden = false;
    this.typed = prefix.text;
    this.draw();
    this.place();
  }

  draw() {
    const typed = this.typed ?? "";
    // The window of rows moves only as far as it must to keep the chosen row in it.
    let first = this.first ?? 0;
    if (this.index < first) {
      first = this.index;
    } else if (this.index >= first + SHOWN_ROWS) {
      first = this.index - SHOWN_ROWS + 1;
    }
    this.first = Math.max(0, Math.min(first, this.shown.length - SHOWN_ROWS));
    const rows = this.shown.slice(this.first, this.first + SHOWN_ROWS).map((item, offset) => {
      const index = this.first + offset;
      const label = item.label.toLowerCase().startsWith(typed.toLowerCase()) && typed
        ? `<b>${escapeHtml(item.label.slice(0, typed.length))}</b>${escapeHtml(item.label.slice(typed.length))}`
        : escapeHtml(item.label);
      const kind = escapeHtml(item.kind ?? "word");
      const detail = item.detail ? `<span class="ed-item-detail">${escapeHtml(item.detail)}</span>` : "";
      return `<div class="ed-item" data-index="${index}" aria-selected="${index === this.index}"><span class="ed-kind k-${kind}"></span><span class="ed-item-label">${label}</span>${detail}</div>`;
    });
    this.list.innerHTML = rows.join("");
    const chosen = this.shown[this.index];
    const said = [chosen?.detail ? `\`${chosen.detail}\`` : "", chosen?.doc ?? ""].filter(Boolean).join("\n\n");
    this.detail.hidden = !said;
    this.detail.innerHTML = said ? markup(said) : "";
  }

  place() {
    if (!this.open || !this.at) {
      return;
    }
    const ed = this.ed;
    const host = ed.host.getBoundingClientRect();
    const at = ed.rectOf(this.at);
    const height = this.el.offsetHeight;
    const below = at.bottom - host.top + 2;
    const top = below + height > host.height - 24 && at.top - host.top - height - 2 >= 0 ? at.top - host.top - height - 2 : below;
    this.el.style.top = `${top}px`;
    this.el.style.left = `${Math.max(4, Math.min(at.left - host.left - 22, host.width - this.el.offsetWidth - 12))}px`;
  }

  key(name) {
    const count = this.shown.length;
    const steps = { Up: -1, Down: 1, PageUp: -SHOWN_ROWS, PageDown: SHOWN_ROWS };
    if (name in steps) {
      const next = this.index + steps[name];
      this.index = name === "Up" || name === "Down" ? (next + count) % count : Math.max(0, Math.min(count - 1, next));
      this.draw();
      return true;
    }
    if (name === "Enter" || name === "Tab") {
      this.accept();
      return true;
    }
    if (name === "Escape") {
      this.close();
      return true;
    }
    return false;
  }

  accept() {
    const ed = this.ed;
    const item = this.shown[this.index];
    if (!item) {
      this.close();
      return;
    }
    const prefix = ed.prefix();
    const length = prefix.text.length;
    this.close();
    if (item.snippet && ed.s.selections.length === 1) {
      ed.insertSnippet(prefix.from, ed.primary().head, item.insert);
      return;
    }
    const text = item.snippet ? parseSnippet(item.insert).text : item.insert;
    ed.edit((sel) => {
      if (!empty(sel)) {
        return { from: startOf(sel), to: endOfSel(sel), text };
      }
      const head = sel.head;
      const before = ed.doc.line(head.line).slice(Math.max(0, head.col - length), head.col);
      const from = before === prefix.text ? pos(head.line, head.col - length) : head;
      return { from, to: head, text };
    }, null);
    ed.doc.seal();
  }

  close() {
    this.open = false;
    this.items = null;
    this.at = null;
    this.el.hidden = true;
  }
}
