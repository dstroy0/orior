// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The edit view: the tree's files, an editor a tab a file, and beside it the definition of the open
// file's type as the tree holds it. A file that is not text opens as its bytes, its k-file head read
// out where it has one.

import { invoke } from "./bridge.js";
import { wordAt } from "./editor/document.js";
import { Session } from "./editor/session.js";
import { Editor } from "./editor/view.js";
import { drawBridge, inBridge, keepBridge, keyAt, loadBridge } from "./bridge_panel.js";
import { opening, registerLanguages, rowOf } from "./languages.js";
import { onScheme } from "./scheme.js";
import { calm, write } from "./status.js";

const state = {
  editor: null,
  known: null,
  head: null,
  tabs: [],
  active: null,
  expanded: new Set([""]),
  children: new Map(),
};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const extOf = (path) => (path.includes(".") ? path.split(".").pop().toLowerCase() : "");

// The tree, a folder at a time as each is opened.

async function childrenOf(dir) {
  if (!state.children.has(dir)) {
    state.children.set(dir, await invoke("tree_list", { dir }));
  }
  return state.children.get(dir);
}

async function drawTree() {
  const list = document.getElementById("files");
  const query = document.getElementById("file-filter").value.trim();
  if (query) {
    const found = await invoke("tree_find", { query });
    list.replaceChildren(...found.map((path) => node({ name: path, path, dir: false }, 0)));
    return;
  }
  const nodes = [];
  const walk = async (dir, depth) => {
    for (const entry of await childrenOf(dir)) {
      nodes.push(node(entry, depth));
      if (entry.dir && state.expanded.has(entry.path)) {
        await walk(entry.path, depth + 1);
      }
    }
  };
  await walk("", 0);
  list.replaceChildren(...nodes);
}

function node(entry, depth) {
  const known = !entry.dir && state.known?.typeOf(entry.path);
  const mark = entry.dir ? (state.expanded.has(entry.path) ? "▾ " : "▸ ") : "";
  const button = element("button", {
    className: `node${entry.dir ? " dir" : ""}${known ? " known" : ""}`,
    type: "button",
    textContent: `${mark}${entry.name}`,
    title: entry.path,
  });
  button.style.paddingLeft = `${0.5 + depth * 0.9}rem`;
  if (state.active === entry.path) {
    button.setAttribute("aria-current", "true");
  }
  button.addEventListener("click", async () => {
    if (entry.dir) {
      if (state.expanded.has(entry.path)) {
        state.expanded.delete(entry.path);
      } else {
        state.expanded.add(entry.path);
      }
      await drawTree();
    } else {
      await openFile(entry.path);
    }
  });
  return button;
}

// The tabs.

function tabOf(path) {
  return state.tabs.find((tab) => tab.path === path);
}

function dirty(tab) {
  return tab.session && tab.session.doc.id !== tab.saved;
}

function drawTabs() {
  const bar = document.getElementById("tabs");
  bar.replaceChildren(
    ...state.tabs.map((tab) => {
      const name = tab.path.split("/").pop();
      const close = element("span", { className: "close", textContent: tab.closing ? "×?" : "×", title: tab.path });
      const button = element("button", { className: "tab", type: "button", title: tab.path, role: "tab" });
      button.setAttribute("aria-selected", String(tab.path === state.active));
      button.append(dirty(tab) ? element("span", { className: "dirty", textContent: "●" }) : "", name, close);
      button.addEventListener("click", (event) => {
        if (event.target === close) {
          closeTab(tab);
        } else {
          show(tab.path);
        }
      });
      button.addEventListener("auxclick", (event) => event.button === 1 && closeTab(tab));
      return button;
    })
  );
}

// A tab with changes not yet saved asks for a second click before it closes.
function closeTab(tab) {
  if (dirty(tab) && !tab.closing) {
    tab.closing = true;
    drawTabs();
    return;
  }
  state.tabs = state.tabs.filter((one) => one !== tab);
  if (state.active === tab.path) {
    state.active = state.tabs.at(-1)?.path ?? null;
    show(state.active);
  } else {
    drawTabs();
  }
}

// A file too large to read whole opens as a window of HALF bytes either side of the line it was
// left at, and reads outward from there a SLICE at a time, below then above, between frames.
const HALF = 192 * 1024;
const SLICE = 384 * 1024;

const placeKey = (path) => `orior.place.${path}`;

function placeOf(path) {
  try {
    return JSON.parse(localStorage.getItem(placeKey(path)) ?? "null");
  } catch {
    return null;
  }
}

function keepPlace() {
  const tab = tabOf(state.active);
  const s = tab?.session;
  if (s) {
    const head = s.selections[s.primary].head;
    localStorage.setItem(placeKey(tab.path), JSON.stringify({ line: s.base + head.line, col: head.col }));
  }
}

// Puts a session's cursor at a remembered place and scrolls it a little way down the screen.
function settleAt(session, place) {
  const line = Math.max(0, Math.min(session.doc.count - 1, (place?.line ?? 0) - session.base));
  const col = Math.min(session.doc.line(line).length, place?.col ?? 0);
  session.selections = [{ anchor: { line, col }, head: { line, col }, goal: null }];
  session.top = Math.max(0, line - 8);
}

// Reads the rest of a windowed file, a slice at a time, standing back whenever the status block says
// the view is pressed, and writing how far it has read there.
async function readOutward(tab) {
  const s = tab.session;
  let below = true;
  while (s.window && (s.window.start > 0 || s.window.end < s.window.size)) {
    if (!state.tabs.includes(tab)) {
      write("reads", [tab.path, null]);
      return;
    }
    write("reads", [tab.path, { read: s.window.end - s.window.start, size: s.window.size }]);
    const canAbove = s.window.start > 0;
    const goBelow = s.window.end < s.window.size && (below || !canAbove);
    below = !below;
    if (goBelow) {
      const got = await invoke("file_slice", { path: tab.path, start: s.window.end, end: s.window.end + SLICE });
      s.grow(got.text, false);
      s.window.end = got.end > s.window.end ? got.end : s.window.size;
    } else {
      let got = await invoke("file_slice", { path: tab.path, start: Math.max(0, s.window.start - SLICE), end: s.window.start });
      if (got.start >= s.window.start) {
        got = await invoke("file_slice", { path: tab.path, start: 0, end: s.window.start });
      }
      s.grow(got.text, true);
      s.window.start = got.start;
    }
    await calm();
  }
  write("reads", [tab.path, null]);
  s.window = null;
  state.editor.schedule();
}

export async function openFile(path) {
  if (!tabOf(path)) {
    const opened = await invoke("file_read", { path });
    const tab = { path, size: opened.size, closing: false };
    const place = placeOf(path);
    if (opened.windowed) {
      const shown = await invoke("file_window", { path, line: place?.line ?? 0, half: HALF });
      const held = { start: shown.start, end: shown.end, size: shown.size };
      tab.session = new Session(shown.text, state.known.languageOf(path), { base: shown.line, window: held });
      tab.saved = tab.session.doc.id;
      settleAt(tab.session, place);
    } else if (opened.text !== null && opened.text !== undefined) {
      tab.session = new Session(opened.text, state.known.languageOf(path));
      tab.saved = tab.session.doc.id;
      settleAt(tab.session, place);
    } else {
      tab.bytes = new Uint8Array(opened.bytes);
    }
    state.tabs.push(tab);
    if (tab.session?.window) {
      tab.reading = readOutward(tab);
    }
  }
  show(path);
}

function show(path) {
  state.active = path;
  const tab = tabOf(path);
  const editorNode = document.getElementById("editor");
  const binaryNode = document.getElementById("binary");
  if (tab?.session) {
    editorNode.hidden = false;
    binaryNode.hidden = true;
    state.editor.show(tab.session);
    state.editor.focus();
  } else {
    state.editor.show(null);
    editorNode.hidden = Boolean(tab);
    binaryNode.hidden = !tab;
    if (tab) {
      drawBinary(tab);
    }
  }
  drawTabs();
  drawDefs();
  drawTree();
}

async function saveActive() {
  const tab = tabOf(state.active);
  if (!tab?.session) {
    return;
  }
  // A file still being read is written only once all of it is in.
  await tab.reading;
  const saving = tab.session.doc.id;
  await invoke("file_write", { path: tab.path, text: tab.session.doc.text() });
  tab.saved = saving;
  tab.closing = false;
  drawTabs();
  if (inBridge(tab.path)) {
    loadBridge().then(drawDefs);
  }
}

// Opens a file with the cursor on a line of it, counted from the file's first line. A line a file
// still being read has not reached yet is waited for; a file not open yet opens around the line.
export async function openAt(path, line) {
  if (!tabOf(path)) {
    localStorage.setItem(placeKey(path), JSON.stringify({ line, col: 0 }));
  }
  await openFile(path);
  const tab = tabOf(path);
  const s = tab?.session;
  if (!s) {
    return;
  }
  if ((line < s.base || line >= s.base + s.doc.count) && tab.reading) {
    await tab.reading;
  }
  if (state.active === path) {
    state.editor.goTo(line - s.base);
  }
}

// The bridge's key the panel shows: the one the cursor is on, or one picked from the list.
function bridgeKey() {
  const s = state.editor?.s;
  if (!s || !state.active) {
    return null;
  }
  const head = state.editor.head();
  const found = keyAt(state.active, s.doc, head.line, wordAt(s.doc, head)?.text);
  return found ?? state.picked ?? null;
}

// A file that is not text: its k-file head where the bytes open with the tree's magic, then the bytes.

function drawBinary(tab) {
  const node = document.getElementById("binary");
  const bytes = tab.bytes;
  const head = state.head;
  const parts = [element("h3", { textContent: `${tab.path}, ${tab.size} bytes` })];
  const ascii = (from, to) => String.fromCharCode(...bytes.slice(from, to)).replace(/\0+$/, "");
  const isKrep = head && bytes.length >= head.bytes && ascii(0, 8) === head.magic;
  if (isKrep) {
    const version = new DataView(bytes.buffer, bytes.byteOffset).getUint32(12, true);
    const type = state.known.typeOf(tab.path);
    const named = (index, fallback) => {
      if (!type) {
        return fallback;
      }
      const row = type.table.rows.map((r) => rowOf(type.table, r)).find((r) => r.where === `head ${index}`);
      return row ? row.kind : fallback;
    };
    const grid = element("div", { className: "head" });
    grid.append(element("span", { textContent: named(0, "magic") }), element("span", { textContent: ascii(0, 8) }));
    grid.append(element("span", { textContent: named(1, "kind") }), element("span", { textContent: ascii(8, 12) }));
    grid.append(
      element("span", { textContent: named(2, "version") }),
      element("span", { textContent: String(version), className: version === head.version ? "" : "bad" })
    );
    parts.push(grid);
  }
  const rows = [];
  for (let at = 0; at < bytes.length; at += 16) {
    const line = element("div");
    line.append(at.toString(16).padStart(8, "0"), "  ");
    for (let index = at; index < at + 16; index += 1) {
      const text = index < bytes.length ? bytes[index].toString(16).padStart(2, "0") : "  ";
      line.append(isKrep && index < head.bytes ? element("b", { textContent: text }) : text, index === at + 7 ? "  " : " ");
    }
    const shown = [...bytes.slice(at, at + 16)].map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : ".")).join("");
    line.append(" ", shown);
    rows.push(line);
  }
  parts.push(element("pre", {}, ...rows));
  node.replaceChildren(...parts);
}

// The definitions beside the editor.

function drawDefs() {
  const panel = document.getElementById("defs");
  const path = state.active;
  const type = path ? state.known.typeOf(path) : null;
  const ext = path ? extOf(path) : "";
  const tables = state.known.tablesOf(ext);
  const bridged = inBridge(path);
  panel.hidden = !type && !tables.length && !bridged;
  if (panel.hidden) {
    return;
  }
  const parts = [];
  if (bridged) {
    state.shownKey = bridgeKey();
    const choose = (key) => {
      state.picked = key;
      drawDefs();
    };
    parts.push(element("section", { className: "bridge" }, ...drawBridge(state.shownKey, openAt, choose)));
  }
  if (type) {
    const source = element("a", { className: "source", textContent: type.table.path, tabIndex: 0 });
    source.addEventListener("click", () => openFile(type.table.path));
    parts.push(element("h3", { textContent: `.${type.ext}` }), source);
    if (type.header) {
      parts.push(element("p", { className: "header", textContent: type.header }));
    }
    let where = null;
    for (const row of type.table.rows.map((r) => rowOf(type.table, r))) {
      const place = row.where.replace(/\s+\d+$/, "");
      if (place !== where) {
        where = place;
        parts.push(element("div", { className: "where", textContent: place }));
      }
      const item = element("div", { className: "row", title: row.kind });
      item.dataset.key = opening(row.form);
      item.dataset.kind = row.kind;
      item.append(element("code", { textContent: row.form }), element("span", { textContent: row.gloss }));
      item.append(element("small", { textContent: row.who && row.who !== "-" ? `${row.kind}, ${row.who}` : row.kind }));
      parts.push(item);
    }
  }
  for (const table of tables) {
    parts.push(...languageTable(table));
  }
  panel.replaceChildren(...parts);
  if (bridged) {
    panel.scrollTop = 0;
  }
  light();
}

function languageTable(table) {
  const at = (row, name) => row[table.columns.indexOf(name)] ?? "";
  const asm = table.columns.includes("mnemonic");
  const key = asm ? "mnemonic" : table.columns[0];
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", spellcheck: false });
  const source = element("a", { className: "source", textContent: table.path, tabIndex: 0 });
  source.addEventListener("click", () => openFile(table.path));
  const rows = element("div");
  const draw = () => {
    const query = filter.value.trim().toLowerCase();
    rows.replaceChildren(
      ...table.rows
        .filter((row) => !query || row.join(" ").toLowerCase().includes(query))
        .slice(0, 400)
        .map((row) => {
          const item = element("div", { className: "row" });
          item.dataset.key = at(row, key);
          const said = asm
            ? `${at(row, "table")}: ${at(row, "past")} ${at(row, "arrow") || "-"} ${at(row, "present")}`
            : [at(row, "kind"), at(row, "names")].filter(Boolean).join(": ");
          item.append(element("code", { textContent: at(row, key) }), element("span", { textContent: said }));
          return item;
        })
    );
    light();
  };
  filter.addEventListener("input", draw);
  draw();
  return [element("h3", { textContent: table.name }), source, filter, rows];
}

// Marks the rows that define what the cursor is on: the line's first word for a line language, the
// word itself for .g and .gsm.
function light() {
  if (inBridge(state.active) && bridgeKey() !== state.shownKey) {
    drawDefs();
    return;
  }
  const panel = document.getElementById("defs");
  const session = state.editor?.s;
  let keys = [];
  if (session) {
    const head = state.editor.head();
    const lead = session.doc.line(head.line).match(/^\s*([A-Za-z_][\w-]*)/);
    const word = wordAt(session.doc, head)?.text;
    keys = [lead?.[1], word].filter(Boolean);
  }
  let first = null;
  panel.querySelectorAll(".row").forEach((row) => {
    const lit = keys.includes(row.dataset.key);
    row.dataset.lit = String(lit);
    first = first ?? (lit ? row : null);
  });
  // In a file of the bridge the bridge section stays in sight, above the rows.
  if (!inBridge(state.active)) {
    first?.scrollIntoView({ block: "nearest" });
  }
}

export async function startEdit(defs) {
  state.known = registerLanguages(defs);
  state.head = defs.head;
  let lighting = 0;
  let keeping = 0;
  state.editor = new Editor(document.getElementById("editor"), {
    onCursor: () => {
      cancelAnimationFrame(lighting);
      lighting = requestAnimationFrame(light);
      window.clearTimeout(keeping);
      keeping = window.setTimeout(keepPlace, 500);
    },
    onChange: (session) => {
      const tab = state.tabs.find((one) => one.session === session);
      if (tab) {
        tab.closing = false;
      }
      drawTabs();
    },
  });
  onScheme(() => state.editor.refreshColors());
  let wait = 0;
  document.getElementById("file-filter").addEventListener("input", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawTree, 180);
  });
  window.addEventListener("keydown", (event) => {
    if ((event.ctrlKey || event.metaKey) && event.code === "KeyS") {
      event.preventDefault();
      saveActive();
    }
  });
  document.getElementById("defs").hidden = true;
  loadBridge();
  keepBridge(() => inBridge(state.active) && drawDefs());
  await drawTree();
}

// Forgets the folders read so far, for a tree opened in place of this one.
export function forgetTree() {
  state.children.clear();
  state.expanded = new Set([""]);
  loadBridge();
}

