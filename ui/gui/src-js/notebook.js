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
import { breakpointsOf } from "./debug.js";
import { Session } from "./editor/session.js";
import { Editor } from "./editor/view.js";
import { say } from "./statusbar.js";
import { drawForms, drawMap, drawMarks, drawOutputs, drawSource } from "./notebook/draw.js";
import { checkFromTop, heard, heardCheck, kernelSpecs, restartKernel, runAll, runCell, startKernel, stopKernel } from "./notebook/kernel.js";
import { addBar, cellNode } from "./notebook/cells.js";
import { letMirrorGo } from "./edit/held.js";
import { mirrorCells } from "./notebook/mirror.js";

// The lines an output shows of itself before the rest is a press away, and how many of its first
// and last it shows then.
export const LONG = 200;
export const ENDS = 40;

// The folder of the tree the cells debugged are written to.
const CELLS = "build/orior/cells";

// The extension a kernel's language is written in, which names the cells' language to the editor.
const EXTENSIONS = { python: "py", r: "r", julia: "jl", javascript: "js", typescript: "ts", ruby: "rb", go: "go", rust: "rs", "c++": "cpp", c: "c", matlab: "m", octave: "m", bash: "sh", scala: "scala", sql: "sql", lua: "lua", haskell: "hs" };

export const state = {
  hooks: null,
  editor: null,
  host: null,
  specs: null,
  // Each notebook open, by its path.
  books: new Map(),
};

export function element(tag, props = {}, ...children) {
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
export function cellOf(book, raw) {
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
  await mirrorCells(book);
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
  book.cells.forEach((cell) => letMirrorGo(cell.session));
  state.books.delete(book.path);
}

// The cells that ran before `cell` and have changed since, run again since, or are gone, which the
// values `cell` left stand on.
export function staleBy(book, cell) {
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

export function cellName(book, cell) {
  const index = book.cells.indexOf(cell);
  return index >= 0 ? `cell ${index + 1}` : "a cell since deleted";
}

// The file of the tree a code cell of a Python notebook is written to as it is debugged, which its
// breakpoints are kept by; null for any other cell.
export function cellFile(book, cell) {
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
export function breaksKey(book, cell) {
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


// Moves the notebook's editor into `cell`, to write its text.
export function focusCell(book, cell, at = null) {
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

export function blurCell(book) {
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


// The bar over the cells: the kernel to run on, its state, and what runs.
export function drawBar(book) {
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
