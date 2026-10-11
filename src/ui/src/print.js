// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Print: the file open, or the lines the selection takes, as the editor colors them, in the
// light scheme's colors on white paper, each line with its number, a long line going on under itself.
//
// The page is printed from orior's own print sheet: the page as it prints beside the printer, the
// copies, the pages, the way the paper lies and color or gray, or a PDF written to a file of the
// tree, with no dialog of the system's. Where the system prints only through its own dialog, the
// sheet says so, and the page goes to that dialog from a frame of its own, which goes once it is
// printed.

import { invoke } from "./bridge.js";
import { say } from "./statusbar.js";

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escape = (text) => text.replace(/[&<>"]/g, (letter) => ESCAPES[letter]);


const KEPT = "orior.print";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

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
    const runs = s.highlight.runsNow(line);
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

function keptSettings() {
  try {
    return JSON.parse(localStorage.getItem(KEPT) ?? "{}") ?? {};
  } catch {
    return {};
  }
}

// Prints the session's text, `title` the name the sheet and the page's head give it, from orior's
// print sheet over the window, `sheet` making it.
export async function printText(s, title, sheet) {
  const html = printed(s, title);
  const found = await invoke("printers_list").catch((error) => ({ printers: [], silent: false, error: String(error) }));
  if (!found.silent || !sheet) {
    say(`Printing did not start: ${found.error ?? "the window gave no printers"}. File, Print opens again once it can.`, { failed: true, act: () => printText(s, title, sheet) });
    return;
  }
  const kept = keptSettings();
  const pdfChoice = "\u0000pdf";
  const printer = element(
    "select",
    { className: "report-field" },
    ...found.printers.map((one) => element("option", { value: one.name, textContent: one.default ? `${one.name} (the system's own)` : one.name, selected: kept.printer ? kept.printer === one.name : one.default })),
    element("option", { value: pdfChoice, textContent: "Save as PDF", selected: kept.printer === pdfChoice }),
  );
  const copies = element("input", { className: "report-field", type: "number", min: 1, max: 99, value: kept.copies ?? 1 });
  const pages = element("input", { className: "report-field", type: "text", placeholder: "every page, or as 1-3, 5", value: "" });
  const landscape = element("input", { type: "checkbox", checked: Boolean(kept.landscape) });
  const gray = element("input", { type: "checkbox", checked: Boolean(kept.gray) });
  const tree = (document.getElementById("tree-path")?.textContent ?? "").replace(/[\\/]+$/, "");
  const base = (title || "printed").replace(/^.*[\\/]/, "").replace(/\.[^.]+$/, "");
  const pdf = element("input", { className: "report-field", type: "text", value: `${base}.pdf`, spellcheck: false });
  const row = (label, input) => element("label", { className: "report-row" }, element("span", { textContent: label }), input);
  const check = (input, label) => element("label", { className: "report-check" }, input, element("span", { textContent: label }));
  const pdfRow = row("The PDF, in the tree", pdf);
  const copiesRow = row("Copies", copies);
  const said = element("p", { className: "report-said print-said" });
  const preview = element("iframe", { className: "print-preview", sandbox: "", title: "The page as it prints", srcdoc: html });
  const paper = element("div", { className: "print-paper" }, preview);
  const go = element("button", { className: "primary", type: "submit", textContent: "Print" });
  const shown = () => {
    const toFile = printer.value === pdfChoice;
    pdfRow.hidden = !toFile;
    copiesRow.hidden = toFile;
    go.textContent = toFile ? "Save" : "Print";
    paper.classList.toggle("landscape", landscape.checked);
    paper.classList.toggle("gray", gray.checked);
  };
  [printer, landscape, gray].forEach((one) => one.addEventListener("change", shown));
  const form = element(
    "form",
    { className: "sheet-ask print-sheet" },
    element("h2", { textContent: `Print ${title || "the file"}` }),
    element(
      "div",
      { className: "print-body" },
      paper,
      element("div", { className: "print-settings" }, row("Printer", printer), copiesRow, pdfRow, row("Pages", pages), check(landscape, "Landscape"), check(gray, "Gray"), said, element("div", { className: "commit-buttons" }, go)),
    ),
  );
  shown();
  const dialog = sheet(form);
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const toFile = printer.value === pdfChoice;
    const settings = {
      printer: toFile ? null : printer.value,
      copies: Math.max(1, Number(copies.value) || 1),
      pages: pages.value.trim() || null,
      landscape: landscape.checked,
      gray: gray.checked,
      pdf: toFile ? (/^([A-Za-z]:)?[\\/]/.test(pdf.value) ? pdf.value : `${tree}/${pdf.value}`) : null,
    };
    localStorage.setItem(KEPT, JSON.stringify({ printer: printer.value, copies: settings.copies, landscape: settings.landscape, gray: settings.gray }));
    // The page goes to the printer behind the sheet, and the window is the reader's again at once.
    dialog.close();
    say(toFile ? `Writing ${pdf.value}…` : `Sending ${title || "the page"} to ${settings.printer}…`);
    try {
      say(`${title || "The page"} ${await invoke("print_page", { html, settings })}`);
    } catch (error) {
      say(String(error), { failed: true, act: () => printText(s, title, sheet) });
    }
  });
  printer.focus();
}
