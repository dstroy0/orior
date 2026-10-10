// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Print: the file open, or the lines the selection takes, as the editor colors them, in the
// light scheme's colors on white paper, each line with its number, a long line going on under itself.
// The page goes to the system's print dialog from a frame of its own, which goes once it is printed.

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escape = (text) => text.replace(/[&<>"]/g, (letter) => ESCAPES[letter]);

// How the page reaches the dialog, which a test can stand in for.
export const printing = { print: (frame) => frame.contentWindow.print() };

// The lines printed: those the primary selection takes, or every line where it takes none.
function linesOf(s) {
  const sel = s.selections[s.primary];
  const [from, to] = [sel.anchor, sel.head].sort((a, b) => a.line - b.line || a.col - b.col);
  if (from.line === to.line && from.col === to.col) {
    return [0, s.doc.count - 1];
  }
  return [from.line, to.col === 0 && to.line > from.line ? to.line - 1 : to.line];
}

// The page to print, as HTML.
export function printed(s, title) {
  const [first, last] = linesOf(s);
  const probe = document.createElement("div");
  probe.dataset.scheme = "light";
  probe.style.cssText = "position:absolute;visibility:hidden;pointer-events:none;color:var(--ed-fg)";
  document.body.append(probe);
  const styles = new Map();
  const styleOf = (name) => {
    if (!styles.has(name)) {
      const span = document.createElement("span");
      span.className = name;
      probe.append(span);
      const seen = getComputedStyle(span);
      styles.set(name, `color:${seen.color};font-weight:${seen.fontWeight};font-style:${seen.fontStyle}`);
    }
    return styles.get(name);
  };
  const width = String(s.base + last + 1).length;
  const rows = [];
  for (let line = first; line <= last; line += 1) {
    const text = s.doc.line(line);
    const runs = s.highlight.runsOf(line);
    let code = "";
    runs.forEach(([start, name], index) => {
      const end = runs[index + 1]?.[0] ?? text.length;
      if (end > start) {
        const part = escape(text.slice(start, end));
        code += name ? `<span style="${styleOf(name)}">${part}</span>` : part;
      }
    });
    rows.push(`<div class="row"><span class="n">${String(s.base + line + 1).padStart(width)}</span><span class="c">${code || " "}</span></div>`);
  }
  const ink = getComputedStyle(probe).getPropertyValue("--ed-fg").trim() || "#222";
  const number = getComputedStyle(probe).getPropertyValue("--fg-3").trim() || "#888";
  const font = getComputedStyle(document.documentElement).getPropertyValue("--code").trim() || "monospace";
  probe.remove();
  return `<!doctype html><html><head><meta charset="utf-8"><title>${escape(title)}</title><style>
@page { margin: 14mm; }
body { margin: 0; background: #fff; color: ${ink}; font: 9.5pt/1.45 ${font}; tab-size: ${s.indent.size}; }
.row { display: flex; break-inside: avoid; }
.n { flex: none; padding-right: 1.2em; color: ${number}; white-space: pre; user-select: none; }
.c { flex: 1; white-space: pre-wrap; overflow-wrap: anywhere; }
</style></head><body>${rows.join("")}</body></html>`;
}

// Prints the session's text, `title` the name the dialog and the page's head give it.
export function printText(s, title) {
  const frame = document.createElement("iframe");
  frame.style.cssText = "position:fixed;width:0;height:0;border:0;visibility:hidden";
  document.body.append(frame);
  const page = frame.contentDocument;
  page.open();
  page.write(printed(s, title));
  page.close();
  frame.contentWindow.addEventListener("afterprint", () => frame.remove());
  printing.print(frame);
}
