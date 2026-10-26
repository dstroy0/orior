// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells drawn: a cell's code in the editor's colors, its form fields, its outputs and its source, the
// marks of the cells run, and the map beside them.

import { breakpointsOf } from "../debug.js";
import { escapeHtml, markdownHtml } from "../markdown.js";
import { coloredHtml } from "../screen.js";
import { breaksKey, cellFile, cellName, element, ENDS, LONG, staleBy, state } from "../notebook.js";

// The code of a cell in the editor's colors, as lines of spans of its classes, each line in `breaks`
// marked as holding a breakpoint.
function codeHtml(session, breaks = null) {
  const doc = session.doc;
  const rows = [];
  for (let line = 0; line < doc.count; line += 1) {
    const text = doc.line(line);
    const runs = session.highlight?.runsNow?.(line) ?? [[0, ""]];
    let html = "";
    runs.forEach(([start, name], index) => {
      const end = runs[index + 1]?.[0] ?? text.length;
      if (end > start) {
        const part = escapeHtml(text.slice(start, end));
        html += name ? `<span class="${name}">${part}</span>` : part;
      }
    });
    rows.push(breaks?.has(line) ? `<span class="nb-break-line">${html || " "}</span>` : html || " ");
  }
  return rows.join("\n");
}

// The fields a cell's `#@param` lines draw: a line `name = value  #@param` with what it takes, as
// Colab writes it, a list of choices, or {type: ...} with a slider's bounds.
export function formsOf(text) {
  const fields = [];
  text.split("\n").forEach((line, index) => {
    const match = line.match(/^(\s*)([A-Za-z_]\w*)\s*=\s*(.*?)\s*#\s*@param\s*(.*)$/);
    if (!match) {
      return;
    }
    const [, indent, name, value, rest] = match;
    let kind = "string";
    let choices = null;
    const bounds = {};
    if (rest.trim().startsWith("[")) {
      kind = "choice";
      choices = [...rest.matchAll(/"([^"]*)"|'([^']*)'|(-?\d+(?:\.\d+)?)/g)].map((one) => one[1] ?? one[2] ?? one[3]);
    } else {
      const type = rest.match(/type\s*:\s*["']?(\w+)/)?.[1];
      kind = type ?? (/^["']/.test(value) ? "string" : /^(True|False)$/.test(value) ? "boolean" : /^-?\d/.test(value) ? "number" : "raw");
      for (const key of ["min", "max", "step"]) {
        const found = rest.match(new RegExp(`${key}\\s*:\\s*(-?\\d+(?:\\.\\d+)?)`));
        if (found) {
          bounds[key] = found[1];
        }
      }
    }
    fields.push({ line: index, indent, name, value, kind, choices, bounds, rest });
  });
  return fields;
}

// A field's value as the line writes it: a string in quotes, True or False, and else as given.
function written(field, given) {
  if (field.kind === "boolean") {
    return given ? "True" : "False";
  }
  if (field.kind === "string" || field.kind === "date" || (field.kind === "choice" && /^["']/.test(field.value))) {
    return JSON.stringify(String(given));
  }
  return String(given);
}

function unquoted(value) {
  return value.replace(/^(["'])(.*)\1$/, "$2");
}

// Writes a field's value into its line of the cell.
function setField(book, cell, field, given) {
  const doc = cell.session.doc;
  const text = doc.line(field.line);
  const at = text.indexOf("=") + 1;
  const comment = text.search(/#\s*@param/);
  const edit = { from: { line: field.line, col: at }, to: { line: field.line, col: comment }, text: ` ${written(field, given)}  ` };
  if (state.editor?.s === cell.session) {
    state.editor.change([edit], "form");
  } else {
    doc.change([edit], "form", cell.session.selections);
  }
  cell.edited = ++book.clock;
  drawSource(book, cell);
  drawMarks(book);
  state.hooks.changed();
}

export function drawForms(book, cell, host) {
  host.replaceChildren();
  if (cell.kind !== "code") {
    return;
  }
  for (const field of formsOf(cell.session.doc.text())) {
    let input;
    if (field.kind === "choice") {
      input = element("select");
      for (const choice of field.choices) {
        input.append(element("option", { value: choice, textContent: choice, selected: choice === unquoted(field.value) }));
      }
      input.addEventListener("change", () => setField(book, cell, field, input.value));
    } else if (field.kind === "boolean") {
      input = element("input", { type: "checkbox", checked: field.value === "True" });
      input.addEventListener("change", () => setField(book, cell, field, input.checked));
    } else if (field.kind === "slider") {
      input = element("input", { type: "range", value: field.value, min: field.bounds.min ?? 0, max: field.bounds.max ?? 100, step: field.bounds.step ?? 1 });
      const shown = element("output", { textContent: field.value });
      input.addEventListener("input", () => (shown.textContent = input.value));
      input.addEventListener("change", () => setField(book, cell, field, input.value));
      host.append(element("label", { className: "nb-field" }, element("span", { textContent: field.name }), input, shown));
      continue;
    } else {
      input = element("input", { type: field.kind === "number" || field.kind === "integer" ? "number" : field.kind === "date" ? "date" : "text", value: unquoted(field.value) });
      input.addEventListener("change", () => setField(book, cell, field, input.value));
    }
    host.append(element("label", { className: "nb-field" }, element("span", { textContent: field.name }), input));
  }
}

// The richest form of a display's data the window shows: an image, HTML, Markdown, then text.
function shownData(data, cell, index) {
  if (data["image/png"] || data["image/jpeg"] || data["image/gif"]) {
    const kind = data["image/png"] ? "image/png" : data["image/jpeg"] ? "image/jpeg" : "image/gif";
    return element("img", { className: "nb-image", src: `data:${kind};base64,${String(data[kind]).replace(/\s+/g, "")}` });
  }
  if (data["image/svg+xml"]) {
    return element("img", { className: "nb-image", src: `data:image/svg+xml;charset=utf-8,${encodeURIComponent(data["image/svg+xml"])}` });
  }
  if (data["text/html"]) {
    return htmlFrame(data["text/html"], `${cell.id}-${index}`);
  }
  if (data["text/markdown"]) {
    return element("div", { className: "nb-markdown", innerHTML: markdownHtml(data["text/markdown"]) });
  }
  if (data["application/json"]) {
    return textBlock(JSON.stringify(data["application/json"], null, 1), cell, `${index}`, "nb-text");
  }
  return textBlock(String(data["text/plain"] ?? data["text/latex"] ?? ""), cell, `${index}`, "nb-text");
}

// HTML of an output in a frame of its own, run with no reach into the window; the frame tells the
// window its height, as it draws, to be sized to it.
function htmlFrame(html, name) {
  const frame = element("iframe", { className: "nb-html" });
  frame.setAttribute("sandbox", "allow-scripts");
  frame.dataset.name = name;
  const teller = `<script>new ResizeObserver(()=>parent.postMessage({oriorFrame:${JSON.stringify(name)},height:document.documentElement.scrollHeight},"*")).observe(document.documentElement)</script>`;
  frame.srcdoc = `<!doctype html><html><head><meta charset="utf-8"><style>body{margin:0;font:13px sans-serif;color:#ddd;background:transparent}table{border-collapse:collapse}td,th{border:1px solid #555;padding:2px 6px}</style></head><body>${html}${teller}</body></html>`;
  return frame;
}

window.addEventListener("message", (event) => {
  const name = event.data?.oriorFrame;
  if (!name) {
    return;
  }
  for (const frame of document.querySelectorAll("iframe.nb-html")) {
    if (frame.dataset.name === name && frame.contentWindow === event.source) {
      frame.style.height = `${Math.min(2000, Math.max(20, Number(event.data.height) || 0)) + 4}px`;
    }
  }
});

// A text output, its ANSI colors drawn, cut to its first and last lines where it is long.
function textBlock(text, cell, key, className) {
  const lines = text.replace(/\n$/, "").split("\n");
  const block = element("pre", { className });
  if (lines.length <= LONG || cell.opened.has(key)) {
    block.innerHTML = coloredHtml(text);
    return block;
  }
  const head = lines.slice(0, ENDS).join("\n");
  const tail = lines.slice(-ENDS).join("\n");
  const more = element("button", { type: "button", className: "nb-more", textContent: `Show all ${lines.length} lines` });
  more.addEventListener("click", () => {
    cell.opened.add(key);
    block.replaceWith(textBlock(text, cell, key, className));
  });
  block.innerHTML = `${coloredHtml(head)}\n`;
  block.append(element("span", { className: "nb-cut", textContent: `… ${lines.length - ENDS * 2} lines …\n` }), more, document.createTextNode("\n"));
  block.insertAdjacentHTML("beforeend", coloredHtml(tail));
  return block;
}

export function drawOutputs(book, cell) {
  const host = cell.node?.querySelector(".nb-outputs");
  if (!host) {
    return;
  }
  host.replaceChildren();
  cell.outputs.forEach((output, index) => {
    if (output.output_type === "stream") {
      host.append(textBlock(output.text ?? "", cell, `${index}`, `nb-stream ${output.name === "stderr" ? "stderr" : ""}`));
    } else if (output.output_type === "error") {
      host.append(textBlock((output.traceback ?? [`${output.ename}: ${output.evalue}`]).join("\n"), cell, `${index}`, "nb-error"));
    } else if (output.data) {
      host.append(shownData(output.data, cell, index));
    }
  });
  host.hidden = !cell.outputs.length;
}

// The cell's source as it shows where it is not being written: its code in color, or its Markdown
// as it reads.
export function drawSource(book, cell) {
  const source = cell.node?.querySelector(".nb-source");
  if (!source || state.editor?.s === cell.session) {
    return;
  }
  if (cell.kind === "markdown") {
    const text = cell.session.doc.text();
    source.className = "nb-source nb-markdown";
    source.innerHTML = text.trim() ? markdownHtml(text) : '<p class="nb-empty">Markdown</p>';
  } else {
    const file = cellFile(book, cell);
    cell.marked = breaksKey(book, cell);
    source.className = "nb-source nb-code";
    source.innerHTML = `<pre>${codeHtml(cell.session, file ? breakpointsOf(file) : null)}</pre>`;
  }
  drawForms(book, cell, cell.node.querySelector(".nb-forms"));
}

// Each cell's run count, its running mark and its out-of-date mark, and the map's blocks.
export function drawMarks(book) {
  for (const cell of book.cells) {
    if (!cell.node) {
      continue;
    }
    const count = cell.node.querySelector(".nb-count");
    count.textContent = cell.kind === "code" ? (cell.running ? "[*]" : `[${cell.count ?? " "}]`) : "";
    const by = cell.kind === "code" ? staleBy(book, cell) : [];
    cell.node.classList.toggle("stale", by.length > 0);
    cell.node.classList.toggle("running", cell.running);
    cell.node.classList.toggle("failed", cell.outputs.some((output) => output.output_type === "error"));
    const mark = cell.node.querySelector(".nb-stale");
    mark.hidden = !by.length;
    mark.title = by.length ? `Out of date: it stands on ${by.map((one) => cellName(book, one)).join(", ")}, changed, run again or deleted since this cell ran` : "";
  }
  drawMap(book);
}

// The map at the notebook's right: a block for each cell, as tall as the cell, colored by its
// kind and its state, and the part in view framed.
export function drawMap(book) {
  const map = document.querySelector("#notebook .nb-map");
  const scroller = document.querySelector("#notebook .nb-cells");
  if (!map || !scroller || state.books.get(book.path) !== book) {
    return;
  }
  const total = Math.max(1, scroller.scrollHeight);
  const scale = map.clientHeight / total;
  const blocks = book.cells.map((cell) => {
    const node = cell.node;
    const block = element("div", { className: `nb-map-cell ${cell.kind}` });
    if (node) {
      block.style.top = `${node.offsetTop * scale}px`;
      block.style.height = `${Math.max(2, node.offsetHeight * scale - 1)}px`;
      block.classList.toggle("stale", node.classList.contains("stale"));
      block.classList.toggle("failed", node.classList.contains("failed"));
      block.classList.toggle("running", cell.running);
    }
    return block;
  });
  const seen = element("div", { className: "nb-map-seen" });
  seen.style.top = `${scroller.scrollTop * scale}px`;
  seen.style.height = `${scroller.clientHeight * scale}px`;
  map.replaceChildren(...blocks, seen);
}
