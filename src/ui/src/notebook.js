// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook as cells: a Jupyter notebook, or a script of `# %%` cells opened as one. Each code cell
// runs on its own on the notebook's kernel, its output shown under it; a markdown cell shows as it
// reads, and as its text where it is being written.
//
// The cell being written holds the notebook's one editor; every other cell shows its code in the
// editor's colors, which keeps a long notebook as quick as a short one.
//
// Each cell shows when it ran in the kernel's order, and a cell is marked out of date where its
// output stands on a cell that ran before it and has changed since, has run again since, or is
// gone: the values such a cell left are not what the page shows. Check from the Top runs every code
// cell in a kernel of its own, from the first, and says the first that fails.
//
// An output's HTML shows in a frame of its own with no reach into the window, and an output of more
// than LONG lines shows its first and last lines, the whole a press away.
//
// A Python cell debugged is written to a file of the tree of its own under CELLS, which its
// breakpoints are kept by, and runs on the notebook's kernel under that file's name with the
// debugger attached to the kernel: it stops at the cell's breakpoints, or at its first line where it
// has none, and the debugger lets go of the kernel once the cell has run.

import { invoke, listen } from "./bridge.js";
import { breakpointsOf, debugNext } from "./debug.js";
import { Session } from "./editor/session.js";
import { Editor } from "./editor/view.js";
import { escapeHtml, markdownHtml } from "./markdown.js";
import { coloredHtml } from "./screen.js";
import { say } from "./statusbar.js";
import { runInTerminal } from "./terminal.js";

// The lines an output shows of itself before the rest is a press away, and how many of its first
// and last it shows then.
const LONG = 200;
const ENDS = 40;

// The folder of the tree the cells debugged are written to.
const CELLS = "build/orior/cells";

// The extension a kernel's language is written in, which names the cells' language to the editor.
const EXTENSIONS = { python: "py", r: "r", julia: "jl", javascript: "js", typescript: "ts", ruby: "rb", go: "go", rust: "rs", "c++": "cpp", c: "c", matlab: "m", octave: "m", bash: "sh", scala: "scala", sql: "sql", lua: "lua", haskell: "hs" };

const state = {
  hooks: null,
  editor: null,
  host: null,
  specs: null,
  // Each notebook open, by its path.
  books: new Map(),
};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// Whether `path` opens as a notebook: a .ipynb always.
export function isNotebook(path) {
  return /\.ipynb$/i.test(path);
}

// A cell of the model from a cell of the file; a cell the file gives no id takes one as Jupyter
// makes them.
function cellOf(book, raw) {
  const kind = raw.cell_type ?? "code";
  const language = kind === "code" ? book.language : state.hooks.languageOf("cell.md");
  const cell = {
    id: raw.id ?? crypto.randomUUID().replaceAll("-", "").slice(0, 8),
    kind,
    raw,
    session: new Session(raw.source ?? "", language),
    outputs: raw.outputs ? raw.outputs.slice() : [],
    count: raw.execution_count ?? null,
    runs: [],
    edited: 0,
    running: false,
    // What lets the debugger go of the kernel once the cell's run under it ends.
    debugged: null,
    node: null,
    opened: new Set(),
  };
  cell.saved = cell.session.doc.id;
  return cell;
}

// Opens the notebook at `path` from its file, as a model the view draws.
export async function openNotebook(path) {
  const raw = await invoke("notebook_read", { path });
  const kernelName = raw.metadata?.kernelspec?.name ?? null;
  const languageName = (raw.metadata?.kernelspec?.language ?? raw.metadata?.language_info?.name ?? raw.metadata?.language ?? "python").toLowerCase();
  const book = {
    path,
    raw,
    kernelName,
    languageName,
    language: state.hooks.languageOf(`cell.${EXTENSIONS[languageName] ?? "txt"}`),
    cells: [],
    gone: [],
    clock: 0,
    kernel: { state: "none", display: null },
    waiting: new Map(),
    // What a kernel said of a run before its id came back to the page, held until it does.
    early: new Map(),
    changed: false,
    focused: null,
    check: null,
  };
  book.cells = (raw.cells ?? []).map((one) => cellOf(book, one));
  state.books.set(path, book);
  return book;
}

// Whether a notebook has changes not saved.
export function notebookDirty(book) {
  return book.changed || book.cells.some((cell) => cell.session.doc.id !== cell.saved);
}

// The notebook as its file keeps it, the cells' text and outputs as they stand.
export function notebookFile(book) {
  const raw = structuredClone(book.raw);
  raw.cells = book.cells.map((cell) => {
    const out = { ...structuredClone(cell.raw), cell_type: cell.kind, source: cell.session.doc.text() };
    if (cell.kind === "code") {
      out.outputs = cell.outputs;
      out.execution_count = cell.count;
    } else {
      delete out.outputs;
      delete out.execution_count;
    }
    if (raw.format === "ipynb" && !out.id && (raw.nbformat ?? 4) >= 4 && (raw.nbformat_minor ?? 0) >= 5) {
      out.id = cell.id;
    }
    if (out.metadata === undefined) {
      out.metadata = {};
    }
    return out;
  });
  return raw;
}

// Marks a notebook saved as it stands.
export function markNotebookSaved(book) {
  book.changed = false;
  book.cells.forEach((cell) => (cell.saved = cell.session.doc.id));
}

// Ends the kernel a notebook runs on, as its tab closes.
export function closeNotebook(book) {
  if (book.kernel.state !== "none") {
    invoke("kernel_stop", { key: book.path }).catch(() => {});
  }
  state.books.delete(book.path);
}

// The cells that ran before `cell` and have changed since, run again since, or are gone, which the
// values `cell` left stand on.
function staleBy(book, cell) {
  const ran = cell.runs.at(-1);
  if (ran === undefined) {
    return [];
  }
  const by = [];
  for (const other of [...book.cells, ...book.gone]) {
    if (other === cell || !other.runs.some((at) => at < ran)) {
      continue;
    }
    const before = Math.max(...other.runs.filter((at) => at < ran));
    if (other.gone || other.edited > before || other.runs.some((at) => at > ran)) {
      by.push(other);
    }
  }
  return by;
}

function cellName(book, cell) {
  const index = book.cells.indexOf(cell);
  return index >= 0 ? `cell ${index + 1}` : "a cell since deleted";
}

// The file of the tree a code cell of a Python notebook is written to as it is debugged, which its
// breakpoints are kept by; null for any other cell.
function cellFile(book, cell) {
  if (cell.kind !== "code" || book.languageName !== "python") {
    return null;
  }
  return `${CELLS}/${book.path.replace(/[^\w.-]+/g, "_")}.${cell.id.replace(/[^\w-]+/g, "_")}.py`;
}

// The notebook open and its cell whose file is `path`, or null.
function cellAt(path) {
  for (const book of state.books.values()) {
    const cell = book.cells.find((one) => cellFile(book, one) === path);
    if (cell) {
      return { book, cell };
    }
  }
  return null;
}

// The file of the cell an editor's session holds, or null where it holds none that is debugged.
export function cellFileOf(session) {
  for (const book of state.books.values()) {
    const cell = book.cells.find((one) => one.session === session);
    if (cell) {
      return cellFile(book, cell);
    }
  }
  return null;
}

// The notebook a cell's file is of, by its path, or null where it is of none open.
export function notebookOfCellFile(path) {
  return cellAt(path)?.book.path ?? null;
}

// Writes the cell whose file is `path` at its line `line`, its column `col`, where its notebook
// shows.
export function showCellLine(path, line, col = 0) {
  const found = cellAt(path);
  if (!found || state.shown !== found.book || !found.cell.node) {
    return false;
  }
  focusCell(found.book, found.cell);
  state.editor.goTo(line, col);
  found.cell.node.scrollIntoView({ block: "nearest" });
  return true;
}

// The lines of a cell that hold breakpoints, as one string to compare.
function breaksKey(book, cell) {
  const file = cellFile(book, cell);
  const breaks = file ? breakpointsOf(file) : null;
  return breaks ? [...breaks.keys()].join(",") : "";
}

// Draws again what the debugger marks: the cell's editor, and each cell in view whose breakpoints
// have changed.
export function repaintNotebook() {
  const book = state.shown;
  if (!book) {
    return;
  }
  state.editor?.schedule();
  for (const cell of book.cells) {
    if (cell.kind === "code" && book.focused !== cell && (cell.marked ?? "") !== breaksKey(book, cell)) {
      drawSource(book, cell);
    }
  }
}

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

function drawForms(book, cell, host) {
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

function drawOutputs(book, cell) {
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
function drawSource(book, cell) {
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
function drawMarks(book) {
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
function drawMap(book) {
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

// Moves the notebook's editor into `cell`, to write its text.
function focusCell(book, cell, at = null) {
  if (book.focused && book.focused !== cell) {
    blurCell(book);
  }
  book.focused = cell;
  const source = cell.node.querySelector(".nb-source");
  source.className = "nb-source nb-editing";
  source.replaceChildren(state.host);
  state.host.hidden = false;
  sizeEditor(cell);
  state.editor.show(cell.session);
  state.editor.size();
  if (at) {
    state.editor.select([{ anchor: at, head: at, goal: null }]);
  }
  state.editor.focus();
  book.cells.forEach((one) => one.node?.classList.toggle("focused", one === cell));
}

function blurCell(book) {
  const cell = book.focused;
  if (!cell) {
    return;
  }
  book.focused = null;
  state.host.hidden = true;
  document.body.append(state.host);
  state.editor.show(null);
  cell.node?.classList.remove("focused");
  drawSource(book, cell);
}

// The editor as tall as the cell's lines.
function sizeEditor(cell) {
  const rows = Math.max(1, cell.session.doc.count);
  state.host.style.height = `${(rows + 1) * state.editor.lineHeight + 6}px`;
}

// Runs a code cell on the notebook's kernel, starting the kernel where none runs.
async function runCell(book, cell) {
  if (cell.kind !== "code") {
    if (book.focused === cell) {
      blurCell(book);
    }
    return;
  }
  if (!(await ensureKernel(book))) {
    return;
  }
  cell.running = true;
  cell.outputs = [];
  cell.runs.push(++book.clock);
  drawOutputs(book, cell);
  drawMarks(book);
  try {
    const id = await invoke("kernel_execute", { key: book.path, code: cell.session.doc.text() });
    book.waiting.set(id, cell);
    const held = book.early.get(id) ?? [];
    book.early.delete(id);
    held.forEach((message) => heard(book, message));
  } catch (error) {
    cell.running = false;
    say(String(error), { failed: true });
    drawMarks(book);
  }
}

// Runs a Python code cell under the debugger, which stops at the cell's breakpoints, or at its first
// line where it has none, and lets go of the kernel once the cell has run.
async function debugCell(book, cell) {
  const file = cellFile(book, cell);
  if (!file || !(await ensureKernel(book))) {
    return;
  }
  const code = cell.session.doc.text();
  try {
    const port = await invoke("kernel_debug", { key: book.path, file, code });
    const first = code.split("\n").findIndex((line) => line.trim() && !line.trim().startsWith("#"));
    cell.debugged = await debugNext("127.0.0.1", port, `${cellName(book, cell)} of ${book.path}`, first >= 0 ? { path: file, line: first } : null);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (!cell.debugged) {
    return;
  }
  await runCell(book, cell);
  if (!cell.running) {
    cell.debugged?.();
    cell.debugged = null;
  }
}

// What a kernel said, given to the cell whose run it answers.
function heard(book, message) {
  const cell = book.waiting.get(message.parent);
  const content = message.content ?? {};
  if (message.msg_type === "status" && !message.parent.startsWith("check")) {
    book.kernel.state = content.execution_state ?? book.kernel.state;
    drawBar(book);
  }
  if (!cell) {
    if (message.parent && message.channel !== "log") {
      book.early.set(message.parent, [...(book.early.get(message.parent) ?? []), message]);
      if (book.early.size > 64) {
        book.early.delete(book.early.keys().next().value);
      }
    }
    return;
  }
  switch (message.msg_type) {
    case "execute_input":
      cell.count = content.execution_count ?? cell.count;
      break;
    case "stream": {
      const last = cell.outputs.at(-1);
      if (last?.output_type === "stream" && last.name === content.name) {
        last.text += content.text;
      } else {
        cell.outputs.push({ output_type: "stream", name: content.name, text: content.text });
      }
      break;
    }
    case "execute_result":
      cell.count = content.execution_count ?? cell.count;
      cell.outputs.push({ output_type: "execute_result", data: content.data, metadata: content.metadata ?? {}, execution_count: content.execution_count });
      break;
    case "display_data":
      cell.outputs.push({ output_type: "display_data", data: content.data, metadata: content.metadata ?? {} });
      break;
    case "error":
      cell.outputs.push({ output_type: "error", ename: content.ename, evalue: content.evalue, traceback: content.traceback ?? [] });
      break;
    case "clear_output":
      cell.outputs = [];
      break;
    case "execute_reply":
      cell.running = false;
      book.waiting.delete(message.parent);
      if (content.status === "aborted") {
        cell.runs.pop();
      }
      cell.debugged?.();
      cell.debugged = null;
      break;
    default:
      return;
  }
  book.changed = true;
  drawOutputs(book, cell);
  drawMarks(book);
  state.hooks.changed();
}

// The kernel the notebook runs on, started where none runs: the one its file names, else the first
// of its language. Says whether one runs.
async function ensureKernel(book) {
  if (book.kernel.state !== "none" && book.kernel.state !== "dead") {
    return true;
  }
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === book.kernelName) ?? specs.find((one) => one.language.toLowerCase() === book.languageName && !one.install) ?? specs.find((one) => one.language.toLowerCase() === book.languageName);
  if (!spec) {
    say(`The machine has no kernel for ${book.languageName}: Jupyter's kernels for it are found where Jupyter keeps them`, { failed: true });
    return false;
  }
  return startKernel(book, spec);
}

async function kernelSpecs() {
  state.specs = state.specs ?? (await invoke("kernel_specs").catch(() => []));
  return state.specs;
}

async function startKernel(book, spec) {
  if (spec.install) {
    say(`${spec.display_name} has no ipykernel; press here to install it`, { failed: true, act: () => runInTerminal(spec.install) });
    return false;
  }
  book.kernel = { state: "starting", display: spec.display_name, name: spec.name };
  drawBar(book);
  try {
    await invoke("kernel_start", { key: book.path, name: spec.name });
    book.kernel.state = "idle";
    if (book.kernelName !== spec.name) {
      book.kernelName = spec.name;
      book.raw.metadata = { ...(book.raw.metadata ?? {}), kernelspec: { name: spec.name, display_name: spec.display_name, language: spec.language } };
      book.changed = true;
      state.hooks.changed();
    }
    drawBar(book);
    return true;
  } catch (error) {
    book.kernel.state = "dead";
    drawBar(book);
    say(String(error), { failed: true });
    return false;
  }
}

async function restartKernel(book) {
  for (const cell of book.cells) {
    cell.running = false;
  }
  book.waiting.clear();
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === book.kernel.name ?? book.kernelName);
  book.kernel.state = "none";
  for (const cell of book.cells) {
    cell.runs = [];
    cell.edited = 0;
  }
  book.gone = [];
  drawMarks(book);
  return spec ? startKernel(book, spec) : ensureKernel(book);
}

async function stopKernel(book) {
  if (book.kernel.state === "none") {
    return;
  }
  const ended = await invoke("kernel_stop", { key: book.path }).catch((error) => say(String(error), { failed: true }));
  book.kernel.state = "none";
  book.cells.forEach((cell) => (cell.running = false));
  book.waiting.clear();
  drawBar(book);
  drawMarks(book);
  if (ended) {
    say("The kernel did not stop when asked, and its process was ended");
  }
}

async function runAll(book) {
  for (const cell of book.cells) {
    await runCell(book, cell);
  }
}

// Runs every code cell from the first in a kernel of the notebook's own kind started for it alone,
// and says the first that fails, or that each ran; the notebook's own kernel and outputs stay as
// they are.
async function checkFromTop(book) {
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === (book.kernel.name ?? book.kernelName)) ?? specs.find((one) => one.language.toLowerCase() === book.languageName && !one.install);
  if (!spec) {
    say(`The machine has no kernel for ${book.languageName} to check the notebook in`, { failed: true });
    return;
  }
  const key = `check:${book.path}`;
  book.check = { cells: new Map(), early: new Map(), failed: null, left: 0, done: null };
  drawBar(book);
  try {
    await invoke("kernel_start", { key, name: spec.name });
    const finished = new Promise((resolve) => (book.check.done = resolve));
    const code = book.cells.filter((cell) => cell.kind === "code" && cell.session.doc.text().trim());
    book.check.left = code.length;
    for (const cell of code) {
      const id = await invoke("kernel_execute", { key, code: cell.session.doc.text() });
      book.check.cells.set(id, cell);
      const held = book.check.early.get(id) ?? [];
      book.check.early.delete(id);
      held.forEach((message) => heardCheck(book, message));
    }
    if (code.length === 0) {
      book.check.done();
    }
    await finished;
    const failed = book.check.failed;
    say(failed ? `From the top, ${cellName(book, failed.cell)} fails: ${failed.ename}: ${failed.evalue}` : `From the top, each of the ${code.length} code cells runs`, { failed: Boolean(failed), act: failed ? () => focusCell(book, failed.cell) : null });
  } catch (error) {
    say(String(error), { failed: true });
  } finally {
    invoke("kernel_stop", { key }).catch(() => {});
    book.check = null;
    drawBar(book);
  }
}

function heardCheck(book, message) {
  const check = book.check;
  const cell = check?.cells.get(message.parent);
  if (!cell) {
    if (check && message.parent) {
      check.early.set(message.parent, [...(check.early.get(message.parent) ?? []), message]);
    }
    return;
  }
  if (message.msg_type === "error" && !check.failed) {
    check.failed = { cell, ename: message.content.ename, evalue: message.content.evalue };
  }
  if (message.msg_type === "execute_reply") {
    check.left -= 1;
    if (check.left <= 0 || message.content.status !== "ok") {
      check.done?.();
    }
  }
}

// The bar over the cells: the kernel to run on, its state, and what runs.
function drawBar(book) {
  const bar = document.querySelector("#notebook .nb-bar");
  if (!bar || state.books.get(book.path) !== book || state.shown !== book) {
    return;
  }
  const kernelState = bar.querySelector(".nb-state");
  const busy = book.kernel.state === "busy" || book.kernel.state === "starting";
  kernelState.textContent = book.check ? "checking from the top…" : book.kernel.state === "none" ? "no kernel" : `${book.kernel.display ?? book.kernelName ?? ""}: ${book.kernel.state}`;
  kernelState.classList.toggle("busy", busy);
  bar.querySelector(".nb-pick").value = book.kernel.name ?? book.kernelName ?? "";
}

// Draws the notebook `book` into the notebook's node of the page.
export async function drawNotebook(book) {
  const node = document.getElementById("notebook");
  state.shown = book;
  const pick = element("select", { className: "nb-pick", title: "The kernel the notebook runs on" });
  const button = (label, act, title = "") => {
    const one = element("button", { type: "button", textContent: label, title });
    one.addEventListener("click", act);
    return one;
  };
  const bar = element(
    "div",
    { className: "nb-bar" },
    pick,
    button("Run All", () => runAll(book)),
    button("Restart and Run All", async () => (await restartKernel(book)) && runAll(book)),
    button("Check from the Top", () => checkFromTop(book), "Runs each code cell from the first in a kernel of its own and says the first that fails"),
    button("Interrupt", () => invoke("kernel_interrupt", { key: book.path }).catch((error) => say(String(error), { failed: true }))),
    button("Restart", () => restartKernel(book)),
    button("Stop", () => stopKernel(book), "Asks the kernel to stop, and ends its process where it does not"),
    element("span", { className: "nb-state" }),
  );
  const cells = element("div", { className: "nb-cells" });
  const map = element("div", { className: "nb-map" });
  map.addEventListener("click", (event) => {
    const rect = map.getBoundingClientRect();
    cells.scrollTop = ((event.clientY - rect.top) / rect.height) * cells.scrollHeight - cells.clientHeight / 2;
  });
  cells.addEventListener("scroll", () => drawMap(book));
  node.replaceChildren(bar, element("div", { className: "nb-body" }, cells, map));
  for (const cell of book.cells) {
    cells.append(cellNode(book, cell));
    cells.append(addBar(book, cell));
  }
  if (!book.cells.length) {
    cells.append(addBar(book, null));
  }
  for (const cell of book.cells) {
    drawSource(book, cell);
    drawOutputs(book, cell);
  }
  drawMarks(book);
  drawBar(book);
  const specs = await kernelSpecs();
  pick.replaceChildren(element("option", { value: "", textContent: "Kernel…" }), ...specs.map((spec) => element("option", { value: spec.name, textContent: spec.install ? `${spec.display_name} (no ipykernel)` : spec.display_name })));
  pick.value = book.kernel.name ?? book.kernelName ?? "";
  pick.addEventListener("change", () => {
    const spec = specs.find((one) => one.name === pick.value);
    if (spec) {
      startKernel(book, spec);
    }
  });
  requestAnimationFrame(() => drawMap(book));
}

// The bar under a cell that adds a cell after it, of code or of Markdown.
function addBar(book, after) {
  const add = (kind) => {
    const made = cellOf(book, { cell_type: kind, source: "", metadata: {} });
    const index = after ? book.cells.indexOf(after) + 1 : book.cells.length;
    book.cells.splice(index, 0, made);
    book.changed = true;
    state.hooks.changed();
    drawNotebook(book).then(() => focusCell(book, made));
  };
  const code = element("button", { type: "button", textContent: "+ Code" });
  code.addEventListener("click", () => add("code"));
  const text = element("button", { type: "button", textContent: "+ Markdown" });
  text.addEventListener("click", () => add("markdown"));
  return element("div", { className: "nb-add" }, code, text);
}

function cellNode(book, cell) {
  const run = element("button", { type: "button", className: "nb-run", textContent: "▶", title: "Run the cell (Shift+Enter)" });
  run.addEventListener("click", () => runCell(book, cell));
  const menu = element("button", { type: "button", className: "nb-cell-menu", textContent: "⋯", title: "The cell's menu" });
  menu.addEventListener("click", (event) => cellMenu(book, cell, event));
  const gutter = element("div", { className: "nb-gutter" }, run, element("span", { className: "nb-count" }), element("span", { className: "nb-stale", textContent: "!", hidden: true }));
  const source = element("div", { className: "nb-source" });
  source.addEventListener("mousedown", (event) => {
    if (book.focused !== cell && !event.target.closest("input, select, a")) {
      event.preventDefault();
      focusCell(book, cell);
    }
  });
  const main = element("div", { className: "nb-main" }, source, element("div", { className: "nb-forms" }), element("div", { className: "nb-outputs", hidden: true }));
  cell.node = element("div", { className: `nb-cell ${cell.kind}` }, gutter, main, menu);
  cell.node.dataset.id = cell.id;
  return cell.node;
}

function cellMenu(book, cell, event) {
  const items = [
    { label: "Run", run: () => runCell(book, cell) },
    { label: "Debug", disabled: !cellFile(book, cell), run: () => debugCell(book, cell) },
    { label: cell.kind === "code" ? "Change to Markdown" : "Change to Code", run: () => changeKind(book, cell) },
    { label: "Move Up", disabled: book.cells.indexOf(cell) === 0, run: () => moveCell(book, cell, -1) },
    { label: "Move Down", disabled: book.cells.indexOf(cell) === book.cells.length - 1, run: () => moveCell(book, cell, 1) },
    { label: "Clear Outputs", run: () => clearOutputs(book, cell) },
    { label: "Delete", run: () => deleteCell(book, cell) },
  ];
  state.hooks.menu(items, event);
}

function changeKind(book, cell) {
  if (book.focused === cell) {
    blurCell(book);
  }
  cell.kind = cell.kind === "code" ? "markdown" : "code";
  cell.session.setLanguage(cell.kind === "code" ? book.language : state.hooks.languageOf("cell.md"));
  cell.outputs = [];
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}

function moveCell(book, cell, by) {
  const index = book.cells.indexOf(cell);
  book.cells.splice(index, 1);
  book.cells.splice(index + by, 0, cell);
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}

function clearOutputs(book, cell) {
  cell.outputs = [];
  book.changed = true;
  state.hooks.changed();
  drawOutputs(book, cell);
  drawMarks(book);
}

function deleteCell(book, cell) {
  if (book.focused === cell) {
    blurCell(book);
  }
  book.cells = book.cells.filter((one) => one !== cell);
  if (cell.runs.length) {
    cell.gone = true;
    book.gone.push(cell);
  }
  book.changed = true;
  state.hooks.changed();
  drawNotebook(book);
}

// The notebook's editor, made once, and the kernel's messages heard.
export function startNotebooks(hooks) {
  state.hooks = hooks;
  state.host = element("div", { className: "nb-editor", hidden: true });
  document.body.append(state.host);
  state.editor = new Editor(state.host, {
    onChange: (session) => {
      for (const book of state.books.values()) {
        const cell = book.cells.find((one) => one.session === session);
        if (cell) {
          cell.edited = ++book.clock;
          sizeEditor(cell);
          state.editor.size();
          drawForms(book, cell, cell.node.querySelector(".nb-forms"));
          drawMarks(book);
          hooks.changed();
        }
      }
    },
  });
  // Shift+Enter runs the cell and goes to the next, Ctrl+Enter runs it and stays, and Escape
  // leaves it, before the editor takes the keys.
  state.editor.input.addEventListener(
    "keydown",
    (event) => {
      const book = state.shown;
      const cell = book?.focused;
      if (!cell) {
        return;
      }
      if (event.key === "Enter" && (event.shiftKey || event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        event.stopPropagation();
        runCell(book, cell);
        if (event.shiftKey) {
          const next = book.cells[book.cells.indexOf(cell) + 1];
          if (next) {
            focusCell(book, next);
          } else {
            blurCell(book);
          }
        }
      } else if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        blurCell(book);
      }
    },
    true,
  );
  // A page that starts has no notebook open, and the kernels a page before it left run for none.
  invoke("kernels_stop_all").catch(() => {});
  listen("kernel-message", ({ payload }) => {
    const key = payload?.key ?? "";
    const book = state.books.get(key.replace(/^check:/, ""));
    if (!book) {
      return;
    }
    if (key.startsWith("check:")) {
      heardCheck(book, payload.message);
    } else {
      heard(book, payload.message);
    }
  });
}

// A notebook a merge left in conflict, merged cell by cell, over the editor in `host`: each cell
// one side changed is taken from it, and each cell the two sides changed apart shows both, to take
// one; once each is taken, Mark Resolved gives `resolved` the notebook the cells make.
export function showNotebookMerge(host, path, merged, { resolved, close }) {
  const entries = merged.entries.map((entry) => ({ ...entry, chosen: entry.take ? entry.take : undefined }));
  const left = () => entries.filter((entry) => entry.chosen === undefined).length;
  const done = element("button", { className: "primary merge-done", type: "button", textContent: "Mark Resolved" });
  const where = element("span", { className: "diff-where" });
  const count = () => {
    where.textContent = left() ? `${left()} cell${left() === 1 ? "" : "s"} to choose` : "each cell is chosen";
    done.disabled = left() > 0;
  };
  const shut = element("button", { className: "diff-button", type: "button", textContent: "×", title: "Close (Escape)" });
  const bar = element("div", { className: "diff-bar" }, element("span", { className: "diff-name", textContent: path }), element("span", { className: "diff-sides", textContent: "the notebook's cells, yours and theirs where both changed one" }), where, done, shut);
  const body = element("div", { className: "diff-body nb-merge-body" });
  const view = element("section", { className: "diff-view merge-view nb-merge" }, bar, body);
  const sourceOf = (cell) => element("pre", { className: "nb-merge-source", textContent: cell ? cell.source : "deleted on this side" });
  entries.forEach((entry, index) => {
    if (entry.take) {
      body.append(element("div", { className: `nb-merge-cell ${entry.take.cell_type}` }, element("span", { className: "nb-merge-kind", textContent: entry.take.cell_type }), sourceOf(entry.take)));
      return;
    }
    const sides = element("div", { className: "nb-merge-sides" });
    for (const [label, cell] of [["Yours", entry.conflict.ours], ["Theirs", entry.conflict.theirs]]) {
      const take = element("button", { type: "button", className: "nb-merge-take", textContent: `Take ${label}` });
      const side = element("div", { className: "nb-merge-side" }, element("div", { className: "nb-merge-label", textContent: label }), sourceOf(cell), take);
      take.addEventListener("click", () => {
        entry.chosen = cell;
        sides.querySelectorAll(".nb-merge-side").forEach((one) => one.classList.toggle("chosen", one === side));
        count();
      });
      sides.append(side);
    }
    body.append(element("div", { className: "nb-merge-cell conflict" }, element("span", { className: "nb-merge-kind", textContent: `cell ${index + 1}: changed on both sides` }), sides));
  });
  done.addEventListener("click", () => {
    const book = structuredClone(merged.book);
    book.cells = entries.map((entry) => entry.chosen).filter(Boolean);
    view.remove();
    resolved(book);
  });
  shut.addEventListener("click", () => {
    view.remove();
    close?.();
  });
  count();
  host.append(view);
  return view;
}

// The notebook's editor, to bind the reader's keys in.
export function notebookEditor() {
  return state.editor;
}

// Leaves the cell being written, as the notebook goes out of view.
export function hideNotebook() {
  if (state.shown) {
    blurCell(state.shown);
  }
  state.shown = null;
}
