// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A page of HTML, CSS or SVG drawn in the live preview as it is typed, in a frame that runs none of
// its scripts and submits none of its forms. The page is drawn whole once; after that each change is
// patched into it where it changed, and its scroll, its form's fields and what else it holds stay as
// they were. A stylesheet the page links that a tab holds is taken from the tab as it is typed and
// swapped in place; one no tab holds is read from the tree. SVG is drawn on a page of its own, and a
// CSS file over a page made from its selectors: for each selector, the elements it applies to, the
// last holding the selector's text. Each element is tied to the line its tag starts on, and the one
// the cursor stands in is outlined over the frame.

export const LINE = "data-orior-line";

const RAW = /^(script|style|textarea|title)$/i;
const VOID = new Set(["area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"]);
const UNSEEN = new Set(["head", "meta", "link", "script", "style", "title", "template", "base", "noscript"]);
const PAGE = new Set(["html", "body", ":root"]);

// `text` with each element's start tag told the line it starts on, counted from 0, as LINE; a
// comment, a CDATA section and the text of a script, a style, a textarea or a title are left as they
// are.
export function annotate(text) {
  const tag = /<!--[\s\S]*?(?:-->|$)|<!\[CDATA\[[\s\S]*?(?:\]\]>|$)|<([a-zA-Z][\w:.-]*)/g;
  let out = "";
  let from = 0;
  let line = 0;
  let counted = 0;
  let match;
  while ((match = tag.exec(text))) {
    if (!match[1]) {
      continue;
    }
    for (; counted < match.index; counted += 1) {
      line += text.charCodeAt(counted) === 10 ? 1 : 0;
    }
    const end = match.index + match[0].length;
    out += `${text.slice(from, end)} ${LINE}="${line}"`;
    from = end;
    if (RAW.test(match[1])) {
      const close = text.slice(end).search(new RegExp(`</${match[1]}\\s*>`, "i"));
      tag.lastIndex = close < 0 ? text.length : end + close;
    }
  }
  return out + text.slice(from);
}

// Where the string that opens at `at` in `css` ends: at its closing quote, or before the end of its
// line where it has none.
function stringEnd(css, at) {
  let end = at + 1;
  while (end < css.length && css[end] !== css[at] && css[end] !== "\n") {
    end += css[end] === "\\" ? 2 : 1;
  }
  return css[end] === "\n" ? end - 1 : Math.min(end, css.length - 1);
}

// The rules of a stylesheet, each its selector and the line it starts on, counted from 0; the rules
// inside @media, @supports, @layer, @container and @scope are read as rules, and the blocks of other
// at-rules are passed over.
export function cssRules(css) {
  const rules = [];
  let line = 0;
  let prelude = "";
  let preludeLine = 0;
  for (let at = 0; at < css.length; at += 1) {
    const c = css[at];
    if (c === "/" && css[at + 1] === "*") {
      const end = css.indexOf("*/", at + 2);
      const stop = end < 0 ? css.length : end + 2;
      line += (css.slice(at, stop).match(/\n/g) ?? []).length;
      at = stop - 1;
      continue;
    }
    if (c === "\n") {
      line += 1;
    }
    if (c === '"' || c === "'") {
      const end = stringEnd(css, at);
      prelude += css.slice(at, end + 1);
      at = end;
      continue;
    }
    if (c === "{") {
      const head = prelude.trim();
      const group = /^@(media|supports|layer|container|scope|document)\b/i.test(head);
      if (!group) {
        if (head && !head.startsWith("@")) {
          rules.push({ selector: head.replace(/\s+/g, " "), line: preludeLine });
        }
        let depth = 1;
        while (depth && at + 1 < css.length) {
          at += 1;
          if (css[at] === "/" && css[at + 1] === "*") {
            const end = css.indexOf("*/", at + 2);
            const stop = end < 0 ? css.length : end + 2;
            line += (css.slice(at, stop).match(/\n/g) ?? []).length;
            at = stop - 1;
            continue;
          }
          if (css[at] === '"' || css[at] === "'") {
            at = stringEnd(css, at);
            continue;
          }
          depth += css[at] === "{" ? 1 : css[at] === "}" ? -1 : 0;
          line += css[at] === "\n" ? 1 : 0;
        }
      }
      prelude = "";
      continue;
    }
    if (c === "}" || c === ";") {
      prelude = "";
      continue;
    }
    if (!prelude.trim() && /\S/.test(c)) {
      preludeLine = line;
    }
    prelude += c;
  }
  return rules;
}

// `text` split at each `at` character outside brackets, parentheses and quotes.
function splitTop(text, at) {
  const parts = [];
  let depth = 0;
  let quote = "";
  let from = 0;
  for (let i = 0; i < text.length; i += 1) {
    const c = text[i];
    if (quote) {
      quote = c === quote ? "" : quote;
    } else if (c === '"' || c === "'") {
      quote = c;
    } else if (c === "(" || c === "[") {
      depth += 1;
    } else if (c === ")" || c === "]") {
      depth -= 1;
    } else if (!depth && at.includes(c)) {
      parts.push(text.slice(from, i), c);
      from = i + 1;
    }
  }
  parts.push(text.slice(from));
  return parts;
}

// A selector as its compounds and the combinators between them: [compound, " " | ">" | "+" | "~", ...].
function steps(selector) {
  const out = [];
  let joiner = null;
  for (const part of splitTop(selector, " >+~\t\n")) {
    if (part === "" || /^\s$/.test(part)) {
      joiner ??= part ? " " : null;
      continue;
    }
    if (part === ">" || part === "+" || part === "~") {
      joiner = part;
      continue;
    }
    if (out.length) {
      out.push(joiner ?? " ");
    }
    out.push(part);
    joiner = null;
  }
  return out;
}

const SIMPLE = /^(\*|[a-zA-Z][\w-]*)|#([\w-]+)|\.([\w-]+)|\[\s*([\w:-]+)\s*(?:[~|^$*]?=\s*("[^"]*"|'[^']*'|[^\]\s]+))?\s*[is]?\s*\]|::?[\w-]+(?:\((?:[^()]|\([^()]*\))*\))?/g;

// A compound selector as the element it applies to: its tag, id, classes and attributes, its pseudo-
// classes and pseudo-elements left out.
function compoundOf(text) {
  const out = { tag: "", id: "", classes: [], attrs: [] };
  for (const match of text.matchAll(SIMPLE)) {
    if (match[1] && match.index === 0) {
      out.tag = match[1] === "*" ? "" : match[1].toLowerCase();
    } else if (match[2]) {
      out.id = match[2];
    } else if (match[3]) {
      out.classes.push(match[3]);
    } else if (match[4] && !/^on/i.test(match[4])) {
      out.attrs.push([match[4], (match[5] ?? "").replace(/^["']|["']$/g, "")]);
    } else if (/^:root$/i.test(match[0]) && match.index === 0) {
      out.tag = ":root";
    }
  }
  return out;
}

function build(doc, compound) {
  let element;
  try {
    element = doc.createElement(compound.tag || "div");
  } catch {
    element = doc.createElement("div");
  }
  if (compound.id) {
    element.id = compound.id;
  }
  if (compound.classes.length) {
    element.className = compound.classes.join(" ");
  }
  for (const [name, value] of compound.attrs) {
    try {
      element.setAttribute(name, value);
    } catch {
      // A name the page cannot hold is left off.
    }
  }
  return element;
}

// The elements the selector `selector` of the rule at line `line` applies to, built in `doc`: each
// compound an element, a descendant or a child inside the one before it, a sibling after it, the
// page's own html and body standing for themselves; the last is tied to the rule's line and holds
// the selector's text. None for a selector of an element that is not seen.
export function selectorElements(doc, selector, line) {
  const parts = steps(selector);
  const holder = doc.createDocumentFragment();
  let parent = holder;
  let last = null;
  for (let at = 0; at < parts.length; at += 2) {
    const compound = compoundOf(parts[at]);
    if (UNSEEN.has(compound.tag)) {
      return null;
    }
    if (PAGE.has(compound.tag)) {
      continue;
    }
    const element = build(doc, compound);
    const joiner = parts[at - 1];
    if (last && (joiner === "+" || joiner === "~")) {
      last.after(element);
    } else {
      (last ?? parent).append(element);
      parent = last ?? parent;
    }
    last = element;
  }
  if (!last) {
    return null;
  }
  last.setAttribute(LINE, String(line));
  if (last.localName === "img") {
    last.alt = selector;
  } else if (!VOID.has(last.localName)) {
    last.append(selector);
  }
  return holder;
}

// The page a stylesheet is drawn over, made from its selectors, in rule order, into `doc`'s body.
export function cssPage(doc, css) {
  for (const rule of cssRules(css)) {
    for (const selector of splitTop(rule.selector, ",").filter((part) => part !== ",")) {
      const built = selectorElements(doc, selector.trim(), rule.line);
      if (built) {
        doc.body.append(built);
      }
    }
  }
}

// `css` with each address in it that is not absolute made so from `base`, the address of its folder.
export function rebase(css, base) {
  const absolute = (url) => {
    try {
      return /^([a-z][a-z0-9+.-]*:|#|\/\/)/i.test(url) ? url : new URL(url, base).href;
    } catch {
      return url;
    }
  };
  return css
    .replace(/url\(\s*(["']?)([^"')]+)\1\s*\)/g, (_, quote, url) => `url(${quote}${absolute(url)}${quote})`)
    .replace(/@import\s+(["'])([^"']+)\1/g, (_, quote, url) => `@import ${quote}${absolute(url)}${quote}`);
}

// The page the text `text` of a file of kind "html", "svg" or "css" makes, as a document of its own
// whose addresses are read from `base`, the address of the file's folder. `sheet(href)` gives the
// text and folder's address of a stylesheet the page links where a tab holds it, which the page
// takes in place of the link. Null for SVG that does not read as XML.
export function pageOf(kind, text, { base, sheet = () => null }) {
  let doc;
  if (kind === "html") {
    doc = new DOMParser().parseFromString(annotate(text), "text/html");
  } else {
    doc = document.implementation.createHTMLDocument("");
    if (kind === "svg") {
      if (new DOMParser().parseFromString(text, "image/svg+xml").querySelector("parsererror")) {
        return null;
      }
      doc.body.innerHTML = annotate(text.replace(/^\s*<\?xml[^>]*\?>/, "").replace(/<!DOCTYPE[^>[]*(\[[^\]]*\])?\s*>/i, ""));
    } else {
      const own = doc.createElement("style");
      own.setAttribute("data-orior-sheet", "");
      own.textContent = rebase(text, base);
      doc.head.append(own);
      cssPage(doc, text);
    }
  }
  for (const link of doc.querySelectorAll('link[rel~="stylesheet" i][href]')) {
    const held = sheet(link.getAttribute("href"));
    if (held) {
      const style = doc.createElement("style");
      style.setAttribute("data-orior-sheet", link.getAttribute("href"));
      style.textContent = rebase(held.text, held.base);
      link.replaceWith(style);
    }
  }
  doc.querySelectorAll("base").forEach((one) => one.remove());
  const own = doc.createElement("base");
  own.href = base;
  doc.head.prepend(own);
  return doc;
}

function attributesOf(live, fresh) {
  let changed = false;
  for (const { name } of [...live.attributes]) {
    if (!fresh.hasAttribute(name)) {
      live.removeAttribute(name);
      changed = true;
    }
  }
  for (const { name, value } of [...fresh.attributes]) {
    if (live.getAttribute(name) !== value) {
      live.setAttribute(name, value);
      changed = true;
    }
  }
  return changed;
}

// Patches the element `live` to stand as `fresh` does, keeping each node of the same kind in the
// same place and changing only what differs; counts how many nodes it kept, changed, added and took
// out into `counts`.
export function morph(live, fresh, counts = { kept: 0, changed: 0, added: 0, removed: 0 }) {
  counts[attributesOf(live, fresh) ? "changed" : "kept"] += 1;
  const was = [...live.childNodes];
  const now = [...fresh.childNodes];
  const doc = live.ownerDocument;
  for (let at = 0; at < Math.max(was.length, now.length); at += 1) {
    const one = was[at];
    const next = now[at];
    if (!next) {
      one.remove();
      counts.removed += 1;
    } else if (!one) {
      live.append(doc.importNode(next, true));
      counts.added += 1;
    } else if (one.nodeType !== next.nodeType || one.nodeName !== next.nodeName || one.namespaceURI !== next.namespaceURI) {
      one.replaceWith(doc.importNode(next, true));
      counts.added += 1;
      counts.removed += 1;
    } else if (one.nodeType === Node.ELEMENT_NODE) {
      morph(one, next, counts);
    } else if (one.nodeValue !== next.nodeValue) {
      one.nodeValue = next.nodeValue;
      counts.changed += 1;
    } else {
      counts.kept += 1;
    }
  }
  return counts;
}

const serialize = (doc) => `${doc.doctype ? "<!doctype html>" : ""}${doc.documentElement.outerHTML}`;

// The element drawn in `doc` tied to line `line` of the text, or to the nearest line before it that
// one is tied to: of those tied to the same line, the innermost. One not drawn is passed over.
export function elementAt(doc, line) {
  let best = null;
  let bestLine = -1;
  for (const element of doc.querySelectorAll(`[${LINE}]`)) {
    const at = Number(element.getAttribute(LINE));
    if (at <= line && at >= bestLine && element.getClientRects().length) {
      best = element;
      bestLine = at;
    }
  }
  return best;
}

// The frame a page is drawn in, in `holder`, with the outline over the element the cursor stands
// in. `follow(href)` is told of each link pressed in the page.
export class PageFrame {
  constructor(holder, { follow }) {
    this.holder = holder;
    this.follow = follow;
    this.frame = null;
    this.ready = false;
    this.line = null;
    this.here = document.createElement("div");
    this.here.className = "preview-here";
    this.here.hidden = true;
  }

  // Draws the page `doc`: whole, into a new frame, where none is ready, else patched into
  // the one there. Says what it did: { whole: true } or morph's counts.
  draw(doc) {
    if (!this.frame || !this.frame.isConnected || !this.ready) {
      this.frame = document.createElement("iframe");
      this.frame.className = "preview-frame";
      this.frame.setAttribute("sandbox", "allow-same-origin");
      this.ready = false;
      this.frame.addEventListener("load", () => this.loaded(), { once: true });
      this.frame.srcdoc = serialize(doc);
      this.holder.replaceChildren(this.frame, this.here);
      return { whole: true };
    }
    const counts = morph(this.frame.contentDocument.documentElement, doc.documentElement);
    this.mark(this.line, false);
    return counts;
  }

  loaded() {
    this.ready = true;
    const doc = this.frame.contentDocument;
    doc.addEventListener("click", (event) => {
      const link = event.target.closest?.("a[href]");
      if (link) {
        event.preventDefault();
        this.follow(link.getAttribute("href"));
      }
    });
    this.frame.contentWindow.addEventListener("scroll", () => this.place(), { passive: true });
    this.frame.contentWindow.addEventListener("resize", () => this.place(), { passive: true });
    this.mark(this.line, false);
  }

  // The page's document, once its frame has drawn it.
  page() {
    return this.ready ? this.frame.contentDocument : null;
  }

  // Outlines the element of line `line`, and scrolls the page to it where `reveal` and it is out of
  // sight.
  mark(line, reveal) {
    this.line = line;
    const doc = this.page();
    this.target = doc && line !== null ? elementAt(doc, line) : null;
    if (this.target && reveal) {
      const box = this.target.getBoundingClientRect();
      const view = this.frame.contentWindow;
      if (box.bottom < 0 || box.top > view.innerHeight) {
        this.target.scrollIntoView({ block: "center" });
      }
    }
    this.place();
  }

  place() {
    const target = this.target?.isConnected ? this.target : null;
    this.here.hidden = !target;
    if (!target) {
      return;
    }
    const box = target.getBoundingClientRect();
    this.here.style.left = `${this.frame.offsetLeft + box.left}px`;
    this.here.style.top = `${this.frame.offsetTop + box.top}px`;
    this.here.style.width = `${box.width}px`;
    this.here.style.height = `${box.height}px`;
  }
}
