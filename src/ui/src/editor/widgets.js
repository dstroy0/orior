// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panels that open at the text: what a word means, from the language's hover, and the words
// that can go where the cursor is, from its completion.
//
// A language's hover(doc, at) answers { from, to, parts } or null, each part a short text in which
// **bold**, `code` and _slanted_ are marked, or { className, text } for a text drawn in a class. Its complete(doc, at) answers items { label, kind,
// detail, doc, insert, snippet }, where a snippet's insert has stops written ${1:name}, or a promise of
// them from a language server, and its completeNow(doc, at), where it has one, the items it answers
// at once while the server's are awaited. The list stands at once on those and on the file's own
// words, and the server's join them as they come.

import { pos } from "./document.js";
import { empty, endOfSel, escapeHtml, parseSnippet, startOf } from "./view.js";

// A part as markup: code first, and nothing inside it is read as bold or slanted.
// A hover's text as HTML: a fenced block of code as it is, a line of --- as a rule, a heading as bold,
// and in each paragraph `code`, **bold** and _italic_.
export function markup(text) {
  return text
    .split(/^```[^\n]*\n([\s\S]*?)^```[ \t]*$/m)
    .map((piece, index) => (index % 2 ? `<pre><code>${escapeHtml(piece.replace(/\n$/, ""))}</code></pre>` : prose(piece)))
    .join("");
}

const RULE = "\u0000";

// A link written as [text](address) or as a bare address, to an http or https page; a press on it
// opens it in the browser, as edit.js has it.
const LINK = /\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)|(https?:\/\/[^\s<]*[^\s<.,;:!?)\]])/g;

function inline(text) {
  return text
    .split(/(`[^`]*`)/)
    .map((piece, index) => {
      if (index % 2) {
        return `<code>${escapeHtml(piece.slice(1, -1))}</code>`;
      }
      return escapeHtml(piece)
        .replace(LINK, (_, label, address, bare) => `<a href="${address ?? bare}" data-link>${label ?? bare}</a>`)
        .replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
        .replace(/(^|[^\p{L}\p{N}])_([^_]+)_(?![\p{L}\p{N}])/gu, "$1<i>$2</i>")
        .replace(/\n/g, "<br>");
    })
    .join("");
}

function prose(text) {
  return text
    .replace(/^[ \t]*(?:-{3,}|\*{3,})[ \t]*$/gm, `\n\n${RULE}\n\n`)
    .replace(/^(#{1,6}[ \t]+.+)$/gm, "\n\n$1\n\n")
    .split(/\n{2,}/)
    .map((paragraph) => paragraph.trim())
    .filter(Boolean)
    .map((paragraph) => {
      if (paragraph === RULE) {
        return "<hr>";
      }
      const heading = paragraph.match(/^#{1,6}[ \t]+(.+)$/);
      return heading ? `<p><b>${inline(heading[1])}</b></p>` : block(paragraph);
    })
    .join("");
}

// A paragraph's lines: its lists, each item a line that starts with a dash, a star, a plus or a
// number and a stop, a line indented under an item going on with it; and its text between them.
function block(paragraph) {
  const out = [];
  let text = [];
  let list = null;
  const textDone = () => {
    if (text.length) {
      out.push(`<p>${inline(text.join("\n"))}</p>`);
      text = [];
    }
  };
  const listDone = () => {
    if (list) {
      const tag = list.ordered ? "ol" : "ul";
      out.push(`<${tag}${list.ordered && list.start !== 1 ? ` start="${list.start}"` : ""}>${list.items.map((item) => `<li>${inline(item)}</li>`).join("")}</${tag}>`);
      list = null;
    }
  };
  for (const line of paragraph.split("\n")) {
    const item = line.match(/^\s*(?:([-*+])|(\d+)[.)])\s+(.*)$/);
    if (item) {
      textDone();
      const ordered = !item[1];
      if (list && list.ordered !== ordered) {
        listDone();
      }
      list ??= { ordered, start: ordered ? Number(item[2]) : 1, items: [] };
      list.items.push(item[3]);
    } else if (list && /^\s+\S/.test(line)) {
      list.items[list.items.length - 1] += `\n${line.trim()}`;
    } else {
      listDone();
      text.push(line);
    }
  }
  textDone();
  listDone();
  return out.join("");
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
    // A part is Markdown, Markdown with a class, or a node of its own, as a value's tree is.
    this.el.replaceChildren();
    found.parts.forEach((part, at) => {
      if (at) {
        this.el.append(document.createElement("hr"));
      }
      if (part instanceof Node) {
        this.el.append(part);
        return;
      }
      const holder = document.createElement("div");
      holder.innerHTML = typeof part === "string" ? markup(part) : `<div class="${escapeHtml(part.className ?? "")}">${markup(part.text)}</div>`;
      this.el.append(...holder.childNodes);
    });
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
    // What it is kept open by, from the editor's corner, which it moves with: the name and a character's
    // step either side of it, the box, and the gap between the two the pointer crosses on its way to
    // the box.
    const end = this.ed.rectOf(found.to ?? found.from);
    const step = this.ed.cw;
    const near = { left: at.left - host.left - step, right: Math.max(at.left, end.left) - host.left + step, top: at.top - host.top - 2, bottom: at.bottom - host.top + 2 };
    this.keptBy = [near, { left: left - 4, right: left + width + 4, top: Math.min(top, near.bottom), bottom: Math.max(top + height, near.top) }];
  }

  // Whether the pointer at the page's x, y keeps it open: on the name or a step off it, in the box, or
  // between the two.
  keeps(x, y) {
    if (!this.shown || !this.keptBy) {
      return false;
    }
    const host = this.ed.host.getBoundingClientRect();
    const [at, down] = [x - host.left, y - host.top];
    return this.keptBy.some((one) => at >= one.left && at <= one.right && down >= one.top && down <= one.bottom);
  }

  // The pointer is back where it keeps it open: it goes no longer.
  stay() {
    window.clearTimeout(this.wait);
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

// The keys that act on an open list, before which any work put off on it is done.
export const LIST_KEYS = new Set(["Up", "Down", "PageUp", "PageDown", "Enter", "Tab", "Escape"]);

// How many lines either side of the cursor the file's own words are taken from, and the most taken.
const NEAR_LINES = 3000;
const MOST_WORDS = 4000;

// The words of the file within NEAR_LINES of `at`, each of two letters or more once, as items, the
// word being typed, `typed`, left out.
export function fileWords(doc, at, typed) {
  const seen = new Set([typed]);
  const items = [];
  const last = Math.min(doc.count - 1, at.line + NEAR_LINES);
  for (let line = Math.max(0, at.line - NEAR_LINES); line <= last && items.length < MOST_WORDS; line += 1) {
    for (const [word] of doc.line(line).matchAll(/[\p{L}_$][\p{L}\p{N}_$]+/gu)) {
      if (!seen.has(word)) {
        seen.add(word);
        items.push({ label: word, kind: "word", insert: word });
      }
    }
  }
  return items;
}

// `first` and then each item of `rest` whose label `first` has not given.
function joined(first, rest) {
  const labels = new Set(first.map((item) => item.label));
  return [...first, ...rest.filter((item) => !labels.has(item.label))];
}

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

  // Opens or filters the list for the word before the cursor once the frame that draws the key typed
  // has been drawn: the letter shows before any of the list's work begins. Keys typed before then
  // are taken together.
  later(start) {
    this.pending = { start: Boolean(this.pending?.start) || start };
    if (this.pendingFrame) {
      return;
    }
    this.pendingFrame = requestAnimationFrame(() => {
      this.pendingFrame = 0;
      setTimeout(() => this.flush(), 0);
    });
  }

  // Does the work `later` put off now, as a key that acts on the list comes.
  flush() {
    const pending = this.pending;
    this.pending = null;
    if (pending) {
      this.update(pending.start);
    }
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
    // A question already asked for the word begun here is answered for every letter typed since:
    // its answer is filtered by the word as it stands when it comes, and no letter asks again. Each
    // letter meanwhile filters the items the list stands on.
    if (this.waiting && !moved && !forced) {
      this.filter(forced);
      return;
    }
    if (!this.items || moved || forced) {
      const asked = (this.asked = (this.asked ?? 0) + 1);
      const head = ed.primary().head;
      const items = language.complete(ed.doc, head) ?? [];
      const words = fileWords(ed.doc, head, prefix.text);
      this.at = prefix.from;
      // A language server answers later: the list stands at once on the language's own items and the
      // file's words, and the server's answer joins them, ahead of them, if the word asked about is
      // still the one before the cursor.
      if (typeof items.then === "function") {
        const now = joined(language.completeNow?.(ed.doc, head) ?? [], words);
        this.items = now;
        this.waiting = true;
        items.then(
          (answer) => {
            if (asked !== this.asked) {
              return;
            }
            this.waiting = false;
            const here = ed.prefix().from;
            if (ed.s?.language === language && here.line === prefix.from.line && here.col === prefix.from.col) {
              this.items = joined(answer ?? [], now);
              this.at = prefix.from;
              this.filter(forced, this.open);
            }
          },
          () => asked === this.asked && (this.waiting = false),
        );
      } else {
        this.items = joined(items, words);
      }
    }
    this.filter(forced);
  }

  // Shows the items that answer the word before the cursor, best first. With `again`, the list
  // stands open with the server's items joined, the row chosen kept where it is still there.
  filter(forced, again = false) {
    const prefix = this.ed.prefix();
    const kept = this.shown[this.index]?.label;
    this.shown = this.items
      .map((item) => ({ item, score: score(item.label, prefix.text) }))
      .filter((one) => one.score > 0)
      .sort((a, b) => b.score - a.score || a.item.label.length - b.item.label.length || a.item.label.localeCompare(b.item.label))
      .slice(0, 400)
      .map((one) => one.item);
    if (!this.shown.length || (this.shown.length === 1 && this.shown[0].label === prefix.text && !forced)) {
      if (!again) {
        this.close();
      }
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
    this.asked = (this.asked ?? 0) + 1;
    this.pending = null;
    this.waiting = false;
    this.open = false;
    this.items = null;
    this.at = null;
    this.el.hidden = true;
  }
}
