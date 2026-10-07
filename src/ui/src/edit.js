// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The edit view: the explorer with the tree's files, an editor a tab a file, and beside it the
// definition of the open file's type as the tree holds it. A file that is not text opens as its
// bytes, its k-file head read out where it has one. A file as a commit left it opens read only, in a
// tab of its own beside the file's.
//
// The tree marks what git says of each file: its name in the color of how it differs from the last
// commit, the state's letter after it, and a folder holding a changed file in that file's color with
// a dot. What the ignore files leave out is dimmed.
//
// The explorer collapses, sliding left and up, when the editor is pressed, where the reader keeps it
// collapsing; Ctrl+B and the Edit view's keys bring it back.

import { invoke } from "./bridge.js";
import { wordAt } from "./editor/document.js";
import { Session } from "./editor/session.js";
import { Editor } from "./editor/view.js";
import { drawBridge, inBridge, keepBridge, keyAt, loadBridge } from "./bridge_panel.js";
import { drawOpenEditors, drawOutline, drawTimeline, guides, iconOf, lightOutline, startExplorer } from "./explorer.js";
import { opening, registerLanguages, rowOf } from "./languages.js";
import { focusedKey, keepListKeys, refocus } from "./lists.js";
import { clipText, copyText, menuOn } from "./menu.js";
import { terminalAt } from "./terminal.js";
import { onScheme } from "./scheme.js";
import { calm, write } from "./status.js";
import { drawBranch } from "./statusbar.js";

const state = {
  editor: null,
  known: null,
  head: null,
  tabs: [],
  active: null,
  expanded: new Set([""]),
  children: new Map(),
  // What git says of each changed file, and of each folder holding one.
  changes: new Map(),
  rolled: new Map(),
};

// Which git state a folder takes from the files in it: the first of these that any of them is.
const ROLL_ORDER = ["C", "M", "D", "A", "R", "U"];

// The explorer's place: shown or collapsed, and whether pressing the editor collapses it.
const SHOWN_KEY = "orior.explorer.shown";
const AUTO_KEY = "orior.explorer.auto";

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

// Reads what git says of the tree again: each changed file's state, and each folder's from the files
// under it.
async function loadChanges() {
  const [changed, branch] = await Promise.all([invoke("tree_changed").catch(() => []), invoke("tree_branch").catch(() => null)]);
  state.changes = new Map(changed.map(({ path, state: mark }) => [path, mark]));
  state.rolled = new Map();
  for (const [path, mark] of state.changes) {
    let at = path.lastIndexOf("/");
    while (at > 0) {
      const folder = path.slice(0, at);
      const held = state.rolled.get(folder);
      if (!held || ROLL_ORDER.indexOf(mark) < ROLL_ORDER.indexOf(held)) {
        state.rolled.set(folder, mark);
      }
      at = folder.lastIndexOf("/");
    }
  }
  drawBranch(branch, state.changes.size > 0);
}

async function drawTree() {
  const list = document.getElementById("files");
  const focused = focusedKey(list);
  const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
  document.querySelector('.pane[data-pane="folder"] .pane-head').textContent = top.split("/").filter(Boolean).pop() ?? "";
  const query = document.getElementById("file-filter").value.trim();
  if (query) {
    const found = await invoke("tree_find", { query });
    list.replaceChildren(...found.map((path) => node({ name: path, path, dir: false, ignored: false }, 0)));
    refocus(list, focused);
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
  refocus(list, focused);
}

// A row of the tree: a line down from each folder above it, the folder's arrow, the file's icon, the
// name, and git's letter for it.
function node(entry, depth) {
  const known = !entry.dir && state.known?.typeOf(entry.path);
  const mark = entry.dir ? state.rolled.get(entry.path) : state.changes.get(entry.path);
  const button = element("button", {
    className: `node${entry.dir ? " dir" : ""}${known ? " known" : ""}${entry.ignored ? " ignored" : ""}`,
    type: "button",
    title: entry.path,
  });
  button.append(...guides(depth), element("span", { className: "twisty" }));
  if (!entry.dir) {
    button.append(iconOf(entry.name, Boolean(known)));
  }
  button.append(element("span", { className: "name", textContent: entry.name }));
  if (mark) {
    button.dataset.change = mark;
    button.append(element("span", { className: "change", textContent: entry.dir ? "●" : mark }));
  }
  button.dataset.key = entry.path;
  button.dataset.depth = String(depth);
  if (entry.dir) {
    button.setAttribute("aria-expanded", String(state.expanded.has(entry.path)));
  }
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
  return Boolean(tab.session && !tab.readOnly && tab.session.doc.id !== tab.saved);
}

// The file a tab shows: its own path, or for a file as a commit left it, the file's.
const fileOf = (path) => tabOf(path)?.file ?? path;

// A tab's name: the file's, and for a file as a commit left it, the commit's short id after.
function tabName(tab) {
  const name = tab.file.split("/").pop();
  return tab.commit ? `${name} @ ${tab.commit.slice(0, 7)}` : name;
}

function drawTabs() {
  const bar = document.getElementById("tabs");
  drawOpenEditors(
    state.tabs.map((tab) => ({
      path: tab.path,
      file: tab.file,
      name: tabName(tab),
      title: tab.commit ? `${tab.file} @ ${tab.commit}` : tab.file,
      dirty: dirty(tab),
      active: tab.path === state.active,
      known: Boolean(state.known?.typeOf(tab.file)),
    }))
  );
  bar.replaceChildren(
    ...state.tabs.map((tab) => {
      const name = tabName(tab);
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
  if (s && !tab.readOnly) {
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
    const tab = { path, file: path, size: opened.size, closing: false };
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

// Opens a file as commit `commit` left it, read only, in a tab of its own.
async function openCommit(path, commit) {
  const key = `${path}@${commit.id}`;
  if (!tabOf(key)) {
    const text = await invoke("file_at", { path, id: commit.id });
    const tab = { path: key, file: path, commit: commit.id, readOnly: true, size: text.length, closing: false };
    tab.session = new Session(text, state.known.languageOf(path), { readOnly: true });
    tab.saved = tab.session.doc.id;
    state.tabs.push(tab);
  }
  show(key);
}

// Opens every folder above a file in the tree and brings its row into sight.
async function reveal(path) {
  if (!path || tabOf(path)?.commit) {
    return;
  }
  let at = path.lastIndexOf("/");
  while (at > 0) {
    state.expanded.add(path.slice(0, at));
    at = path.lastIndexOf("/", at - 1);
  }
  await drawTree();
  [...document.querySelectorAll("#files .node")].find((row) => row.dataset.key === path)?.scrollIntoView({ block: "nearest" });
}

function show(path) {
  state.active = path;
  const tab = tabOf(path);
  const editorNode = document.getElementById("editor");
  const binaryNode = document.getElementById("binary");
  drawEmpty(!tab);
  if (tab?.session) {
    editorNode.hidden = false;
    binaryNode.hidden = true;
    state.editor.show(tab.session);
    state.editor.focus();
  } else {
    state.editor.show(null);
    editorNode.hidden = true;
    binaryNode.hidden = !tab;
    if (tab) {
      drawBinary(tab);
    }
  }
  drawTabs();
  drawDefs();
  reveal(path);
  drawOutline(tab?.session ?? null);
  drawTimeline(tab ? tab.file : null);
}

// With no file open the desk shows the name over the lattice, as the run view does with no job
// chosen, and the tab bar goes until a tab is in it.
function drawEmpty(shown) {
  document.getElementById("tabs").hidden = shown;
  document.getElementById("edit-empty").hidden = !shown;
}

async function saveActive() {
  const tab = tabOf(state.active);
  if (!tab?.session || tab.readOnly) {
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
  await loadChanges();
  drawTree();
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
  const type = path ? state.known.typeOf(fileOf(path)) : null;
  const ext = path ? extOf(fileOf(path)) : "";
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
  let outlining = 0;
  const cursorLine = () => (state.editor?.s ? state.editor.head().line : 0);
  state.editor = new Editor(document.getElementById("editor"), {
    statusHost: document.getElementById("statusbar"),
    onCursor: () => {
      cancelAnimationFrame(lighting);
      lighting = requestAnimationFrame(() => {
        light();
        lightOutline(cursorLine());
      });
      window.clearTimeout(keeping);
      keeping = window.setTimeout(keepPlace, 500);
    },
    onChange: (session) => {
      const tab = state.tabs.find((one) => one.session === session);
      if (tab) {
        tab.closing = false;
      }
      drawTabs();
      window.clearTimeout(outlining);
      outlining = window.setTimeout(() => drawOutline(tabOf(state.active)?.session ?? null), 300);
    },
  });
  onScheme(() => state.editor.refreshColors());
  let wait = 0;
  document.getElementById("file-filter").addEventListener("input", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawTree, 180);
  });
  startExplorer({
    show,
    close: (path) => tabOf(path) && closeTab(tabOf(path)),
    tabMenu: (path) => tabMenu(tabOf(path)),
    goTo: (line) => {
      state.editor.goTo(line);
      state.editor.focus();
    },
    cursorLine,
    openCommit,
    refresh: async () => {
      state.children.clear();
      await loadChanges();
      await drawTree();
      drawTimeline(fileOf(state.active), true);
    },
    collapse: () => {
      state.expanded = new Set([""]);
      drawTree();
    },
    panesChanged: () => {
      drawOutline(tabOf(state.active)?.session ?? null);
      drawTimeline(state.active ? fileOf(state.active) : null);
    },
  });
  keepListKeys(document.getElementById("panes"), document.getElementById("file-filter"));
  menuOn(document.getElementById("files"), fileItems);
  menuOn(document.getElementById("tabs"), tabItems);
  menuOn(document.getElementById("editor"), editorItems);
  setExplorer(localStorage.getItem(SHOWN_KEY) !== "false");
  document.querySelector("#mode-edit .desk").addEventListener("pointerdown", () => explorerAuto() && explorerShown() && setExplorer(false));
  window.addEventListener("focus", async () => {
    await loadChanges();
    drawTree();
  });
  document.getElementById("defs").hidden = true;
  document.getElementById("editor").hidden = true;
  drawEmpty(true);
  loadBridge();
  keepBridge(() => inBridge(state.active) && drawDefs());
  await loadChanges();
  await drawTree();
}

// The explorer shown, or collapsed out of the way with nothing in it taking the keys.
function setExplorer(shown) {
  const side = document.getElementById("explorer");
  side.classList.toggle("collapsed", !shown);
  side.inert = !shown;
  localStorage.setItem(SHOWN_KEY, String(shown));
}

export function explorerShown() {
  return !document.getElementById("explorer").classList.contains("collapsed");
}

export function toggleExplorer(shown = !explorerShown()) {
  setExplorer(shown);
}

// Whether pressing the editor collapses the explorer. It does until the reader turns it off.
export function explorerAuto() {
  return localStorage.getItem(AUTO_KEY) !== "false";
}

export function setExplorerAuto(on = !explorerAuto()) {
  localStorage.setItem(AUTO_KEY, String(on));
}

// The menus.

// Files a page window draws, which open in one from the tree's menu.
const SHOWN_IN_WINDOW = new Set(["html", "htm", "svg", "png", "jpg", "jpeg"]);

// A row of the tree's menu: open or close a folder, open a file in the editor or a page in a window
// of its own, copy where it is, or open the terminal in its folder.
function fileItems(event) {
  const row = event.target.closest(".node");
  if (!row) {
    return null;
  }
  const path = row.dataset.key;
  const folder = row.classList.contains("dir");
  const parent = path.includes("/") ? path.slice(0, path.lastIndexOf("/")) : "";
  const open = folder
    ? { label: state.expanded.has(path) ? "Close" : "Open", run: () => row.click() }
    : { label: "Open", run: () => openFile(path) };
  return [
    open,
    ...(!folder && SHOWN_IN_WINDOW.has(extOf(path)) ? [{ label: "Open in a window", run: () => invoke("view_open", { path }) }] : []),
    "-",
    { label: "Copy path", run: () => copyText(path) },
    { label: "Open in terminal", run: () => terminalAt(folder ? path : parent) },
  ];
}

// A tab's menu, from the tab bar or from Open Editors: close it, the others or all of them, a tab with changes asking first as its ×
// does, or copy where its file is.
function tabItems(event) {
  return tabMenu(tabOf(event.target.closest(".tab")?.title));
}

function tabMenu(tab) {
  if (!tab) {
    return null;
  }
  const others = state.tabs.filter((one) => one !== tab);
  return [
    { label: "Close", run: () => closeTab(tab) },
    { label: "Close others", disabled: !others.length, run: () => others.forEach(closeTab) },
    { label: "Close all", run: () => [...state.tabs].forEach(closeTab) },
    "-",
    { label: "Copy path", run: () => copyText(tab.file) },
  ];
}

// The editor's menu: the clipboard, choosing all, find, go to a line, and save, each the same as its
// keys. Cut and copy with nothing chosen take the cursor's whole line, as the keys do.
function editorItems() {
  const editor = state.editor;
  if (!editor?.s) {
    return null;
  }
  // A right click leaves the keys with the page, and the clipboard's commands act on what holds them.
  const held = (command) => () => {
    editor.focus();
    document.execCommand(command);
  };
  const pasted = async () => {
    const text = await clipText();
    editor.focus();
    editor.paste({ preventDefault() {}, clipboardData: { getData: () => text } });
  };
  return [
    { label: "Cut", keys: "Ctrl+X", run: held("cut") },
    { label: "Copy", keys: "Ctrl+C", run: held("copy") },
    { label: "Paste", keys: "Ctrl+V", run: pasted },
    "-",
    { label: "Select all", keys: "Ctrl+A", run: () => editor.selectAll() },
    { label: "Find", keys: "Ctrl+F", run: () => editor.find.open(false) },
    { label: "Replace", keys: "Ctrl+H", run: () => editor.find.open(true) },
    { label: "Go to line", keys: "Ctrl+G", run: () => editor.goto.open() },
    "-",
    { label: "Save", keys: "Ctrl+S", disabled: !dirty(tabOf(state.active)), run: saveActive },
  ];
}

// What the menu bar does to the editor: the editor where a file of text is open in it, saving one
// file or all of them, and closing one tab or all of them.
export function editing() {
  return {
    editor: state.editor?.s ? state.editor : null,
    active: state.active,
    changed: state.tabs.some(dirty),
    activeChanged: Boolean(tabOf(state.active) && dirty(tabOf(state.active))),
    open: state.tabs.length > 0,
    save: saveActive,
    saveAll: async () => {
      const shown = state.active;
      for (const tab of state.tabs.filter(dirty)) {
        await tab.reading;
        const saving = tab.session.doc.id;
        await invoke("file_write", { path: tab.path, text: tab.session.doc.text() });
        tab.saved = saving;
        tab.closing = false;
        if (inBridge(tab.path)) {
          loadBridge().then(drawDefs);
        }
      }
      state.active = shown;
      drawTabs();
      await loadChanges();
      drawTree();
    },
    close: () => tabOf(state.active) && closeTab(tabOf(state.active)),
    closeAll: () => [...state.tabs].forEach(closeTab),
    find: () => {
      setExplorer(true);
      const filter = document.getElementById("file-filter");
      filter.focus();
      filter.select();
    },
  };
}

// Forgets the folders read so far, for a tree opened in place of this one.
export function forgetTree() {
  state.children.clear();
  state.expanded = new Set([""]);
  state.changes = new Map();
  state.rolled = new Map();
  loadChanges().then(drawTree);
  loadBridge();
  if (!state.active) {
    drawEmpty(true);
  }
}

