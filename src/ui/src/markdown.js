// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Markdown as HTML, as GitHub reads it: headings, paragraphs, lists and task lists, quotes, code
// fenced or indented, tables, rules, links, images, emphasis, strikethrough and links written bare.
// Each block carries the line of the text it starts on, as data-line, which a preview scrolls by.
//
// HTML in the text keeps the tags and the attributes ALLOWED names, and no other: a script, a
// handler, a style or a link to javascript: never reaches the page. A link's or an image's address
// goes through `resolve`, which turns one relative to the file into one the page can reach.

const ALLOWED = {
  a: ["href", "title"], abbr: ["title"], b: [], blockquote: [], br: [], code: [], dd: [], del: [], details: ["open"], div: ["align"], dl: [], dt: [], em: [],
  h1: ["align"], h2: ["align"], h3: ["align"], h4: ["align"], h5: ["align"], h6: ["align"], hr: [], i: [], img: ["src", "alt", "title", "width", "height", "align"],
  ins: [], kbd: [], li: [], mark: [], ol: ["start"], p: ["align"], picture: [], pre: [], q: [], s: [], samp: [], source: ["srcset", "media"], span: [], strike: [],
  strong: [], sub: [], summary: [], sup: [], table: [], tbody: [], td: ["align", "colspan", "rowspan"], tfoot: [], th: ["align", "colspan", "rowspan"], thead: [], tr: [], tt: [], u: [], ul: [], var: [],
};

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
export const escapeHtml = (text) => text.replace(/[&<>"]/g, (letter) => ESCAPES[letter]);

// An address that is safe to follow: none that runs script.
function safeUrl(url) {
  const bare = url.trim().replace(/[\u0000-\u001f\s]/g, "").toLowerCase();
  return /^(javascript|vbscript|data:text\/html)/.test(bare) ? "#" : url;
}

// HTML written in the text, with only the tags and attributes ALLOWED names.
function sanitized(html, resolve) {
  return html.replace(/<(\/?)([A-Za-z][A-Za-z0-9-]*)([^>]*)>|<!--[\s\S]*?-->/g, (whole, closing, tag, attrs) => {
    if (!tag) {
      return "";
    }
    const name = tag.toLowerCase();
    const allowed = ALLOWED[name];
    if (!allowed) {
      return escapeHtml(whole);
    }
    if (closing) {
      return `</${name}>`;
    }
    const kept = [];
    for (const match of (attrs ?? "").matchAll(/([A-Za-z_:][\w:.-]*)(?:\s*=\s*("[^"]*"|'[^']*'|[^\s"'>]+))?/g)) {
      const key = match[1].toLowerCase();
      if (!allowed.includes(key)) {
        continue;
      }
      let value = (match[2] ?? "").replace(/^["']|["']$/g, "");
      if (key === "href" || key === "src" || key === "srcset") {
        value = safeUrl(key === "href" ? value : resolve(value));
      }
      kept.push(`${key}="${escapeHtml(value)}"`);
    }
    const self = /\/\s*$/.test(attrs ?? "") || name === "br" || name === "hr" || name === "img" || name === "source";
    return `<${name}${kept.length ? ` ${kept.join(" ")}` : ""}${self ? " /" : ""}>`;
  });
}

// The inline marks of a run of text as HTML.
function inline(text, resolve) {
  const held = [];
  const hold = (html) => {
    held.push(html);
    return `\u0000${held.length - 1}\u0000`;
  };
  let out = text;
  // code spans first, whose contents take no other mark
  out = out.replace(/(`+)([\s\S]*?[^`])\1(?!`)/g, (_, ticks, code) => hold(`<code>${escapeHtml(code.replace(/^ (.*) $/, "$1"))}</code>`));
  out = out.replace(/\\([\\`*_{}[\]()#+\-.!|~<>$])/g, (_, letter) => hold(escapeHtml(letter)));
  out = out.replace(/\$([^\s$](?:[^$]*[^\s$])?)\$/g, (_, math) => hold(`<code class="math">${escapeHtml(math)}</code>`));
  out = out.replace(/<(https?:\/\/[^>\s]+|mailto:[^>\s]+)>/g, (_, url) => hold(`<a href="${escapeHtml(safeUrl(url))}">${escapeHtml(url)}</a>`));
  out = out.replace(/<\/?[A-Za-z][^>]*>/g, (tag) => hold(sanitized(tag, resolve)));
  out = out.replace(/!\[([^\]]*)\]\(\s*<?([^)\s>]*)>?(?:\s+"([^"]*)")?\s*\)/g, (_, alt, url, title) =>
    hold(`<img src="${escapeHtml(resolve(url))}" alt="${escapeHtml(alt)}"${title ? ` title="${escapeHtml(title)}"` : ""} />`),
  );
  out = out.replace(/\[([^\]]+)\]\(\s*<?([^)\s>]*)>?(?:\s+"([^"]*)")?\s*\)/g, (_, label, url, title) =>
    hold(`<a href="${escapeHtml(safeUrl(url))}"${title ? ` title="${escapeHtml(title)}"` : ""}>${inline(label, resolve)}</a>`),
  );
  out = out.replace(/(^|[\s(])((?:https?:\/\/|www\.)[^\s<]*[^\s<.,:;"')\]])/g, (_, before, url) => `${before}${hold(`<a href="${escapeHtml(safeUrl(url.startsWith("www.") ? `http://${url}` : url))}">${escapeHtml(url)}</a>`)}`);
  out = escapeHtml(out);
  out = out.replace(/\*\*(?=\S)([\s\S]*?\S)\*\*|__(?=\S)([\s\S]*?\S)__/g, (_, one, two) => `<strong>${one ?? two}</strong>`);
  out = out.replace(/\*(?=\S)([\s\S]*?\S)\*|(^|[^\w])_(?=\S)([\s\S]*?\S)_(?!\w)/g, (_, one, before, two) => (one !== undefined ? `<em>${one}</em>` : `${before}<em>${two}</em>`));
  out = out.replace(/~~(?=\S)([\s\S]*?\S)~~/g, "<del>$1</del>");
  out = out.replace(/(?: {2,}|\\)\n/g, "<br />\n");
  return out.replace(/\u0000(\d+)\u0000/g, (_, index) => held[Number(index)]);
}

// A table's row as its cells, the pipes at its ends taken off and an escaped pipe kept.
function cellsOf(line) {
  const trimmed = line.trim().replace(/^\|/, "").replace(/\|$/, "");
  return trimmed.split(/(?<!\\)\|/).map((cell) => cell.trim().replace(/\\\|/g, "|"));
}

const RULE = /^ {0,3}([-*_])(?:[ \t]*\1){2,}[ \t]*$/;
const FENCE = /^ {0,3}(`{3,}|~{3,})(.*)$/;
const HEADING = /^ {0,3}(#{1,6})(?:[ \t]+(.*?))?(?:[ \t]+#+)?[ \t]*$/;
const ITEM = /^( {0,3})([-*+]|\d{1,9}[.)])([ \t]+|$)(.*)$/;
const ALIGN = /^\s*\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$/;

// The blocks of `lines`, the first at line `base` of the text, as HTML.
function blocks(lines, base, resolve) {
  const out = [];
  let at = 0;
  const tagged = (tag, line, inner, extra = "") => `<${tag} data-line="${base + line}"${extra}>${inner}</${tag}>`;
  while (at < lines.length) {
    const line = lines[at];
    if (!line.trim()) {
      at += 1;
      continue;
    }
    const start = at;
    const fence = line.match(FENCE);
    if (fence) {
      const [, mark, info] = fence;
      const body = [];
      at += 1;
      while (at < lines.length && !new RegExp(`^ {0,3}${mark[0]}{${mark.length},}\\s*$`).test(lines[at])) {
        body.push(lines[at]);
        at += 1;
      }
      at += 1;
      const language = info.trim().split(/\s+/)[0];
      out.push(tagged("pre", start, `<code${language ? ` class="language-${escapeHtml(language)}"` : ""}>${escapeHtml(body.join("\n"))}</code>`));
      continue;
    }
    if (/^ {0,3}\$\$/.test(line)) {
      const body = [line.replace(/^\s*\$\$/, "")];
      at += 1;
      while (at < lines.length && !/\$\$\s*$/.test(body[body.length - 1])) {
        body.push(lines[at]);
        at += 1;
      }
      out.push(tagged("pre", start, `<code class="math">${escapeHtml(body.join("\n").replace(/\$\$\s*$/, "").trim())}</code>`));
      continue;
    }
    const heading = line.match(HEADING);
    if (heading) {
      out.push(tagged(`h${heading[1].length}`, start, inline(heading[2] ?? "", resolve)));
      at += 1;
      continue;
    }
    if (RULE.test(line)) {
      out.push(`<hr data-line="${base + start}" />`);
      at += 1;
      continue;
    }
    if (/^ {4,}|^\t/.test(line)) {
      const body = [];
      while (at < lines.length && (/^ {4,}|^\t/.test(lines[at]) || !lines[at].trim())) {
        body.push(lines[at].replace(/^( {4}|\t)/, ""));
        at += 1;
      }
      while (body.length && !body[body.length - 1].trim()) {
        body.pop();
      }
      out.push(tagged("pre", start, `<code>${escapeHtml(body.join("\n"))}</code>`));
      continue;
    }
    if (/^ {0,3}>/.test(line)) {
      const body = [];
      while (at < lines.length && lines[at].trim() && (/^ {0,3}>/.test(lines[at]) || body.length)) {
        if (!/^ {0,3}>/.test(lines[at]) && (ITEM.test(lines[at]) || FENCE.test(lines[at]) || HEADING.test(lines[at]))) {
          break;
        }
        body.push(lines[at].replace(/^ {0,3}> ?/, ""));
        at += 1;
      }
      out.push(tagged("blockquote", start, blocks(body, base + start, resolve)));
      continue;
    }
    const item = line.match(ITEM);
    if (item) {
      const ordered = /\d/.test(item[2]);
      const items = [];
      while (at < lines.length) {
        const one = lines[at].match(ITEM);
        if (!one || /\d/.test(one[2]) !== ordered || one[1].length > item[1].length + 1) {
          break;
        }
        const indent = one[1].length + one[2].length + Math.max(1, Math.min(4, one[3].length));
        const body = [one[4]];
        const from = at;
        at += 1;
        while (at < lines.length) {
          const next = lines[at];
          if (!next.trim()) {
            if (at + 1 < lines.length && lines[at + 1].startsWith(" ".repeat(indent))) {
              body.push("");
              at += 1;
              continue;
            }
            break;
          }
          if (next.startsWith(" ".repeat(indent)) || (!ITEM.test(next) && !FENCE.test(next) && !HEADING.test(next) && !/^ {0,3}>/.test(next) && !RULE.test(next) && !/^\s/.test(next))) {
            body.push(next.startsWith(" ".repeat(indent)) ? next.slice(indent) : next);
            at += 1;
            continue;
          }
          break;
        }
        let inner;
        const task = body[0].match(/^\[([ xX])\][ \t]+(.*)$/);
        if (task) {
          body[0] = task[2];
        }
        if (body.length === 1 || !body.slice(1).some((part) => part.trim())) {
          inner = inline(body[0], resolve);
        } else {
          inner = blocks(body, base + from, resolve).replace(/^<p data-line="\d+">([\s\S]*?)<\/p>/, "$1");
        }
        if (task) {
          inner = `<input type="checkbox" disabled${/[xX]/.test(task[1]) ? " checked" : ""} /> ${inner}`;
        }
        items.push(`<li data-line="${base + from}"${task ? ' class="task"' : ""}>${inner}</li>`);
      }
      const first = Number.parseInt(item[2], 10);
      out.push(tagged(ordered ? "ol" : "ul", start, items.join(""), ordered && first !== 1 ? ` start="${first}"` : ""));
      continue;
    }
    if (line.includes("|") && at + 1 < lines.length && ALIGN.test(lines[at + 1]) && lines[at + 1].includes("-")) {
      const heads = cellsOf(line);
      const aligns = cellsOf(lines[at + 1]).map((cell) => (cell.startsWith(":") && cell.endsWith(":") ? "center" : cell.endsWith(":") ? "right" : cell.startsWith(":") ? "left" : ""));
      at += 2;
      const rows = [];
      while (at < lines.length && lines[at].trim() && lines[at].includes("|")) {
        rows.push(cellsOf(lines[at]));
        at += 1;
      }
      const cell = (tag, text, index) => `<${tag}${aligns[index] ? ` align="${aligns[index]}"` : ""}>${inline(text ?? "", resolve)}</${tag}>`;
      const head = `<thead><tr>${heads.map((text, index) => cell("th", text, index)).join("")}</tr></thead>`;
      const body = rows.length ? `<tbody>${rows.map((row) => `<tr>${heads.map((_, index) => cell("td", row[index], index)).join("")}</tr>`).join("")}</tbody>` : "";
      out.push(tagged("table", start, head + body));
      continue;
    }
    if (/^ {0,3}<([A-Za-z][\w-]*)[\s>/]|^ {0,3}<!--/.test(line)) {
      const body = [];
      while (at < lines.length && lines[at].trim()) {
        body.push(lines[at]);
        at += 1;
      }
      out.push(`<div data-line="${base + start}">${sanitized(body.join("\n"), resolve)}</div>`);
      continue;
    }
    const body = [];
    while (at < lines.length && lines[at].trim()) {
      const next = lines[at];
      if (body.length && /^ {0,3}(=+|-+)\s*$/.test(next)) {
        out.push(tagged(next.trim()[0] === "=" ? "h1" : "h2", start, inline(body.join("\n"), resolve)));
        body.length = 0;
        at += 1;
        break;
      }
      if (body.length && (FENCE.test(next) || HEADING.test(next) || RULE.test(next) || /^ {0,3}>/.test(next) || ITEM.test(next))) {
        break;
      }
      body.push(next);
      at += 1;
    }
    if (body.length) {
      out.push(tagged("p", start, inline(body.join("\n").trim(), resolve)));
    }
  }
  return out.join("\n");
}

// The Markdown `text` as HTML; `resolve` turns an address in it into one the page can reach.
export function markdownHtml(text, resolve = (url) => url) {
  return blocks(text.replace(/\r\n?/g, "\n").split("\n"), 0, resolve);
}
