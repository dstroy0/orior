// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The edit view: the explorer with the tree's files, an editor a tab a file, and beside it the
// definition of the open file's type as the tree holds it. A file that is not text opens as its
// bytes, its k-file head read out where it has one. A file as a commit left it opens read only, in a
// tab of its own beside the file's.
//
// Over the editor, the breadcrumbs: the folders the file is in and its name, then each symbol the
// cursor is inside, the outermost first. A folder or the name shows it in the explorer, and a symbol
// takes the cursor to it.
//
// The editor's gutter marks each line that differs from the last commit, and where lines were taken
// out, against the file as the last commit left it, read again each time the window comes back. A
// press on a mark shows that change under it: the lines the commit had and the lines there now, with
// the change before and after it a press away, and the commit's lines put back with Revert.
//
// The tree marks what git says of each file: its name in the color of how it differs from the last
// commit, the state's letter after it, and a folder holding a changed file in that file's color with
// a dot. What the ignore files leave out is dimmed.
//
// Each tree keeps its own tabs, the one shown, the files opened last, and the text of every file with
// changes not yet saved: the app closed with changes open comes back with them still unsaved. Back and
// Forward walk the places the cursor jumped from and to, and Last Editor steps through the tabs by
// when each was last shown, while Ctrl is held.

import { invoke } from "./bridge.js";
import { wordAt } from "./editor/document.js";
import { lineChanges } from "./editor/diff.js";
import { Session } from "./editor/session.js";
import { Editor } from "./editor/view.js";
import { drawBridge, inBridge, keepBridge, keyAt, loadBridge } from "./bridge_panel.js";
import { drawGit, drawLocalHistory, drawTodo, drawOpenEditors, drawOutline, drawProblems, drawTimeline, guides, iconOf, lightOutline, paneOpen, shownGroup, startExplorer } from "./explorer.js";
import { drawCommit, startCommit } from "./commit.js";
import { closeDiff, showDiff } from "./diffview.js";
import { symbolsOf } from "./outline.js";
import { opening, registerLanguages, rowOf } from "./languages.js";
import { loadPlugins, onPlugins, toolFor } from "./plugins.js";
import { changed, definition, serve, startServers, stopServing, wrap } from "./servers.js";
import { closeSignature, findUsages, moved, parameterInfo, quickDoc, quickFix, renameSymbol, startIntel, typed } from "./intel.js";
import { extractConstant, extractVariable, inlineVariable } from "./refactor.js";
import { extractFunction } from "./extract.js";
import { breakpointsOf, pausedLineOf, startDebug, stopDebug, toggleBreakpoint } from "./debug.js";
import { bookmarksOf, startBookmarks } from "./bookmarks.js";
import { focusedKey, keepListKeys, refocus } from "./lists.js";
import { clipText, copyText, menuOn } from "./menu.js";
import { runInTerminal, terminalAt } from "./terminal.js";
import { onScheme } from "./scheme.js";
import { onFonts } from "./fonts.js";
import { calm, write } from "./status.js";
import { togglePane } from "./sides.js";
import { drawBranch, say } from "./statusbar.js";

const state = {
  editor: null,
  // The split editor beside the first, { node, editor, session, focused }, or null.
  split: null,
  known: null,
  head: null,
  tabs: [],
  active: null,
  expanded: new Set([""]),
  children: new Map(),
  // What git says of each changed file, and of each folder holding one.
  changes: new Map(),
  rolled: new Map(),
  // The places jumped from, and the ones gone back from.
  back: [],
  forward: [],
  moving: false,
  // The tabs by when each was last shown, and the walk Last Editor is on while Ctrl is held.
  used: [],
  cycle: null,
  // Each file's text as the last commit left it, as it is being read or once it is: null for none.
  heads: new Map(),
  // The keys of the menus' commands for the editor.
  editorKeys: [],
};

// How many places Back holds, how many files the quick open lists as opened last, and the largest
// text kept for a file with changes not saved.
const PLACES = 50;
const RECENT = 30;
const BACKUP_MOST = 2 * 1024 * 1024;

// Which git state a folder takes from the files in it: the first of these that any of them is.
const ROLL_ORDER = ["C", "M", "D", "A", "R", "U"];

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
  state.branch = branch;
  drawCommit(state.changes);
  drawGit(branch);
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

// The tab whose file a session shows, a split's session among them, and not a commit's.
function tabOfSession(s) {
  return state.tabs.find((tab) => tab.session === (s?.of ?? s) && !tab.commit) ?? null;
}

// The lines a tab's file holds as saved: the document's own where its text is the file's, each
// costing a reference and not a copy, or `text` split as the document splits it.
function keepSaved(tab, text) {
  const doc = tab.session.doc;
  tab.savedLines = text === undefined ? doc.lines.slice() : text.split(/\r?\n/);
  tab.savedEol = doc.eol;
  tab.comparedAt = null;
}

// Marks a tab saved at its document's state now, `text` being what the file holds where the
// document does not hold it. A file still being read as a window keeps no lines until it is whole.
function markSaved(tab, text) {
  tab.saved = tab.session.doc.id;
  if (tab.session.window) {
    tab.savedLines = null;
  } else {
    keepSaved(tab, text);
  }
}

// A tab has changes not saved while its text differs from what was saved. The document's state
// says so at once where it is the saved one; where it is not, as after a letter typed and taken
// out again, its lines are held against the saved ones, once for each state, a line not edited
// since the save matching on its reference alone. A file still being read has no saved lines, and
// any edit to it is a change.
function dirty(tab) {
  if (!tab.session || tab.readOnly) {
    return false;
  }
  const doc = tab.session.doc;
  if (doc.id === tab.saved) {
    return false;
  }
  if (!tab.savedLines) {
    return true;
  }
  if (tab.comparedAt !== doc.id) {
    tab.comparedAt = doc.id;
    const saved = tab.savedLines;
    tab.same = doc.eol === tab.savedEol && doc.lines.length === saved.length && doc.lines.every((line, at) => line === saved[at]);
  }
  return !tab.same;
}

// The file a tab shows: its own path, or for a file as a commit left it, the file's.
const fileOf = (path) => tabOf(path)?.file ?? path;

// A tab's name: the file's, and for a file as a commit left it, the commit's short id after.
function tabName(tab) {
  const name = tab.file.split("/").pop();
  return tab.commit ? `${name} @ ${tab.commit.slice(0, 7)}` : name;
}

function drawTabs() {
  keepSession();
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
  state.used = state.used.filter((path) => path !== tab.path);
  if (state.split && state.split.session.of === tab.session) {
    unsplit();
  }
  stopServing(tab);
  forgetBackup(tab.path);
  if (state.active === tab.path) {
    state.active = state.used.find(tabOf) ?? state.tabs.at(-1)?.path ?? null;
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

const textOf = new TextDecoder();

// A slice of a file as file_slice sends it: its start, its end and the file's size, each 8 bytes
// little-endian, then the text.
async function sliceAt(path, start, end) {
  const sent = await invoke("file_slice", { path, start, end });
  const head = new DataView(sent, 0, 24);
  const at = (offset) => Number(head.getBigUint64(offset, true));
  return { start: at(0), end: at(8), size: at(16), text: textOf.decode(new Uint8Array(sent, 24)) };
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
      const got = await sliceAt(tab.path, s.window.end, s.window.end + SLICE);
      s.grow(got.text, false);
      s.window.end = got.end > s.window.end ? got.end : s.window.size;
    } else {
      let got = await sliceAt(tab.path, Math.max(0, s.window.start - SLICE), s.window.start);
      if (got.start >= s.window.start) {
        got = await sliceAt(tab.path, 0, s.window.start);
      }
      s.grow(got.text, true);
      s.window.start = got.start;
    }
    await calm();
  }
  write("reads", [tab.path, null]);
  s.window = null;
  // Read whole with no edit made on the way, the file's lines are the document's.
  if (s.doc.id === tab.saved) {
    keepSaved(tab);
  }
  state.editor.schedule();
}

export async function openFile(path) {
  await load(path);
  show(path);
}

// Opens a file's tab without showing it, with the text kept for it where it had changes not saved.
async function load(path) {
  if (!tabOf(path)) {
    const opened = await invoke("file_read", { path });
    const tab = { path, file: path, size: opened.size, closing: false };
    const place = placeOf(path);
    if (opened.windowed) {
      const shown = await invoke("file_window", { path, line: place?.line ?? 0, half: HALF });
      const held = { start: shown.start, end: shown.end, size: shown.size, lines: shown.lines };
      tab.session = new Session(shown.text, state.known.languageOf(path), { base: shown.line, window: held });
      markSaved(tab);
      settleAt(tab.session, place);
    } else if (opened.text !== null && opened.text !== undefined) {
      const kept = localStorage.getItem(backupKey(path));
      tab.session = new Session(kept ?? opened.text, state.known.languageOf(path));
      // Kept text that differs from the file's is a change not saved, and the tab says so.
      markSaved(tab, opened.text);
      if (kept !== null && kept !== opened.text) {
        tab.saved = -1;
      }
      settleAt(tab.session, place);
    } else {
      tab.bytes = new Uint8Array(opened.bytes);
    }
    state.tabs.push(tab);
    if (tab.session?.window) {
      tab.reading = readOutward(tab);
    }
    serve(tab).then(() => tab.served && state.editor?.s === tab.session && state.editor.schedule());
    // The width the file's formatter keeps its lines to, drawn as a line down the editor.
    if (tab.session) {
      invoke("format_width", { path: tab.path, language: tab.session.language?.id ?? "" })
        .then((width) => {
          tab.session.margin = width;
          state.editor?.schedule();
        })
        .catch(() => {});
    }
  }
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

// Split: a second editor beside the first or under it, on the file the first one shows, the text
// the same in both and the selections, folds and place each its own. The menus act on the one last
// pressed in. It closes with Unsplit, its own close button, or the file's tab.
function splitEditor(direction) {
  const tab = tabOf(state.active);
  if (!tab?.session || tab.session.window || tab.commit) {
    say("Split shows a file of the tree that is open and read whole.");
    return;
  }
  unsplit();
  const desk = document.querySelector("#mode-edit .desk");
  const host = element("div", { className: "editor split-editor" });
  const close = element("button", { className: "split-close", type: "button", title: "Unsplit" }, "×");
  close.setAttribute("aria-label", "Unsplit");
  close.addEventListener("click", unsplit);
  const node = element("section", { className: "split" }, element("div", { className: "split-head" }, element("span", { className: "split-name", textContent: tab.file }), close), host);
  node.setAttribute("aria-label", `${tab.file}, split`);
  desk.append(node);
  desk.dataset.split = direction;
  // Its status line stands in the status bar beside the first one's, the one shown being that of the
  // editor last pressed in.
  const editor = new Editor(host, { ...state.editorHooks, statusHost: document.getElementById("statusbar"), onChange: (s) => state.editorHooks.onChange(s.of ?? s) });
  state.editor.status.after(editor.status);
  state.lendTo(editor);
  const session = tab.session.twin();
  state.split = { node, editor, session, focused: true };
  editor.input.addEventListener("focus", () => splitFocused(true));
  editor.show(session);
  editor.focus();
  splitFocused(true);
}

// Marks which editor of a split the menus act on, and shows its status line.
function splitFocused(focused) {
  if (!state.split) {
    return;
  }
  state.split.focused = focused;
  state.split.editor.status.hidden = !focused;
  state.editor.status.hidden = focused;
}

function unsplit() {
  if (!state.split) {
    return;
  }
  const { node, editor, session } = state.split;
  state.split = null;
  editor.show(null);
  session.drop();
  node.remove();
  editor.status.remove();
  state.editor.status.hidden = false;
  delete document.querySelector("#mode-edit .desk").dataset.split;
  state.editor.schedule();
  state.editor.focus();
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
  closePeek();
  closeSignature();
  if (path !== state.active && state.active && !state.moving) {
    markPlace();
  }
  state.active = path;
  const tab = tabOf(path);
  if (tab && !state.cycle) {
    state.used = [path, ...state.used.filter((one) => one !== path)];
  }
  if (tab && !tab.commit) {
    keepRecent(tab.file);
  }
  const editorNode = document.getElementById("editor");
  const binaryNode = document.getElementById("binary");
  drawEmpty(!tab);
  if (tab?.session) {
    editorNode.hidden = false;
    binaryNode.hidden = true;
    state.editor.show(tab.session);
    state.editor.focus();
    markChanges(tab);
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
  drawCrumbs();
  reveal(path);
  drawOutline(tab?.session ?? null);
  drawTimeline(tab ? tab.file : null);
  drawLocalHistory(tab?.commit ? null : (tab?.file ?? null));
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
  if (saving("format")) {
    await formatTab(tab, { saving: true });
  }
  tidy(tab);
  const writing = tab.session.doc.id;
  const lines = tab.session.doc.lines.slice();
  const eol = tab.session.doc.eol;
  const written = tab.session.doc.text();
  await invoke("file_write", { path: tab.path, text: written });
  tab.saved = writing;
  tab.savedLines = lines;
  tab.savedEol = eol;
  tab.comparedAt = null;
  tab.closing = false;
  forgetBackup(tab.path);
  drawTabs();
  if (inBridge(tab.path)) {
    loadBridge().then(drawDefs);
  }
  drawLocalHistory(tab.file, true);
  await loadChanges();
  drawTree();
}

// Opens a file with the cursor on a line and column of it, each counted from the file's first. A
// line a file still being read has not reached yet is waited for; a file not open yet opens around
// the line.
export async function openAt(path, line, col = 0) {
  if (!tabOf(path)) {
    localStorage.setItem(placeKey(path), JSON.stringify({ line, col }));
  }
  markPlace();
  state.moving = true;
  try {
    await openFile(path);
  } finally {
    state.moving = false;
  }
  const tab = tabOf(path);
  const s = tab?.session;
  if (!s) {
    return;
  }
  if ((line < s.base || line >= s.base + s.doc.count) && tab.reading) {
    await tab.reading;
  }
  if (state.active === path) {
    state.editor.goTo(line - s.base, col);
  }
}

// Saving: on its own a moment after typing rests where Auto Save is on, and each file saved with
// the spaces and tabs at its lines' ends taken off and a line end after its last line where those
// are on. A Markdown file keeps its lines' ends, where two spaces break a line. Where Format on Save
// is on, a file is formatted first, on every save but Auto Save's, which comes while the reader is
// still at work in it.

const AUTO_SAVE_REST = 1000;
const SAVING = {
  "auto-save": ["orior.autosave", false],
  trim: ["orior.trim", false],
  "final-newline": ["orior.final-newline", false],
  format: ["orior.format-on-save", false],
};

export function saving(name) {
  const [key, fallback] = SAVING[name];
  const kept = localStorage.getItem(key);
  return kept === null ? fallback : kept === "true";
}

export function setSaving(name, on = !saving(name)) {
  localStorage.setItem(SAVING[name][0], String(on));
}

// Formatting: Edit, Format Document, and saving where Format on Save is on. The tab's text goes to the
// formatter its language has, as format.rs in the command line's crate runs it, and comes back as
// one edit for each run of lines that changed, which one undo takes back. A text changed while the
// formatter worked is left as it is. A save says nothing of a language no formatter knows.

let formattable = null;

// The edit for a run of `doc`'s lines [from, to) written again as `lines`.
function linesEdit(doc, from, to, lines) {
  if (to < doc.count) {
    return { from: { line: from, col: 0 }, to: { line: to, col: 0 }, text: lines.map((line) => `${line}\n`).join("") };
  }
  if (lines.length) {
    return from < doc.count ? { from: { line: from, col: 0 }, to: doc.end(), text: lines.join("\n") } : { from: doc.end(), to: doc.end(), text: `\n${lines.join("\n")}` };
  }
  return from === 0 ? { from: { line: 0, col: 0 }, to: doc.end(), text: "" } : { from: { line: from - 1, col: doc.line(from - 1).length }, to: doc.end(), text: "" };
}

async function formatTab(tab, { saving: onSave = false } = {}) {
  const s = tab?.session;
  if (!s || s.readOnly || s.window) {
    return;
  }
  formattable ??= new Set(await invoke("format_languages").catch(() => []));
  const language = s.language?.id ?? "plaintext";
  if (!formattable.has(language)) {
    if (!onSave) {
      say(`No formatter formats ${s.language?.name ?? "plain text"}: File, Toolchains lists those there are.`);
    }
    return;
  }
  await tab.reading;
  const doc = s.doc;
  const version = doc.id;
  const before = doc.lines.join("\n");
  let formatted;
  try {
    formatted = await invoke("format_text", { path: tab.path, language, text: before });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (doc.id !== version) {
    say("The file changed while it was formatted, and was left as it is.");
    return;
  }
  const lines = formatted.replace(/\r\n?/g, "\n").split("\n");
  if (lines.join("\n") === before) {
    if (!onSave) {
      say("Already formatted.");
    }
    return;
  }
  const changes = lineChanges(doc.lines, lines);
  const edits = changes ? changes.hunks.map((hunk) => linesEdit(doc, hunk.then[0], hunk.then[1], lines.slice(hunk.now[0], hunk.now[1]))) : [{ from: { line: 0, col: 0 }, to: doc.end(), text: lines.join("\n") }];
  if (state.editor?.s === s) {
    state.editor.change(edits, "format");
  } else {
    doc.change(edits, "format", s.selections);
    s.selections = s.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
    doc.settle(s.selections);
  }
  if (!onSave) {
    say(`Formatted ${changes ? changes.hunks.length : 1} ${changes?.hunks.length === 1 ? "place" : "places"}.`);
  }
}

// Run, Run File: the tab's file saved where it has changes, then run in the terminal with the
// toolchain its language has, by the line run_file.rs in the command line's crate gives.
async function runTab(tab) {
  const s = tab?.session;
  if (!s || s.window) {
    return;
  }
  if (dirty(tab) && !tab.readOnly) {
    state.active = tab.path;
    await saveActive();
  }
  try {
    const run = await invoke("run_file_line", { path: tab.path, language: s.language?.id ?? "plaintext" });
    runInTerminal(run.line);
    say(`Running ${tab.path.split("/").pop()} with ${run.tool}.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Validate: the tab's file saved where it has changes, then checked by the tool plugin for its
// language, as validate.rs in the command line's crate checks it. Its findings are drawn under the
// text as a language server's diagnostics are, until the text changes, and its verdict is said.
async function validateTab(tab) {
  const s = tab?.session;
  if (!s || s.window) {
    return;
  }
  const language = s.language?.id ?? "plaintext";
  const tool = toolFor(language);
  const name = tab.file.split("/").pop();
  if (!tool) {
    say(`No tool plugin validates ${s.language?.name ?? "plain text"}: File, Plugins lists them.`);
    return;
  }
  if (dirty(tab) && !tab.readOnly) {
    state.active = tab.path;
    await saveActive();
  }
  say(`Validating ${name} with ${tool.name}…`);
  try {
    const report = await invoke("validate_file", { path: tab.file, language });
    s.diagnostics = report.findings.map((finding) => ({
      from: { line: finding.line, col: finding.col },
      to: { line: finding.end_line, col: finding.end_col },
      severity: finding.severity,
      message: finding.lifted ? `${finding.message}

${finding.lifted}` : finding.message,
      source: finding.kind,
    }));
    tab.validated = true;
    problemsChanged();
    state.editor.schedule();
    say(`${name} ${report.holds ? "holds" : "does not hold"}: ${report.verdict}`, { failed: !report.holds });
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// A file's changes from the last commit side by side over the editor: its text as the open tab
// holds it, or as the tree does where no tab holds it.
async function openDiff(path, mark) {
  const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
  const then = mark === "U" || mark === "A" ? null : await invoke("file_head", { path }).catch(() => null);
  let now = "";
  if (tab) {
    now = tab.session.doc.text();
  } else if (mark !== "D") {
    now = (await invoke("file_read", { path }).catch(() => null))?.text ?? "";
  }
  showDiff(document.querySelector("#mode-edit .desk"), path, then, now);
}

// Gives each open tab of `paths` its file's text as the tree holds it now, as one step undo takes
// back, after the Commit window rolled the files back.
async function reloadFromDisk(paths) {
  for (const path of paths) {
    const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
    if (!tab) {
      continue;
    }
    const opened = await invoke("file_read", { path }).catch(() => null);
    if (typeof opened?.text !== "string") {
      continue;
    }
    if (replaceText(tab, opened.text, "rollback")) {
      markSaved(tab);
      forgetBackup(tab.path);
    }
  }
  drawTabs();
}

// Gives a tab `text` in place of all it holds, as one step undo takes back, and says whether that
// changed it.
function replaceText(tab, text, why) {
  const doc = tab.session.doc;
  const fresh = text.replace(/\r\n/g, "\n");
  if (doc.text() === fresh) {
    return false;
  }
  const edit = [{ from: { line: 0, col: 0 }, to: doc.end(), text: fresh }];
  if (state.editor.s === tab.session) {
    state.editor.change(edit, why);
  } else {
    doc.change(edit, why, tab.session.selections);
    tab.session.selections = tab.session.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
    doc.settle(tab.session.selections);
  }
  return true;
}

// The text of a file as it stands: its open tab's, or the tree's where no tab holds it.
async function textNow(path) {
  const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
  return tab ? tab.session.doc.text() : ((await invoke("file_read", { path }).catch(() => null))?.text ?? "");
}

// Shows a file's Local History snapshot kept at `at` beside the file as it stands.
async function showSnapshot(path, at) {
  const then = await invoke("history_read", { path, at });
  const date = new Date(at);
  const when = `${date.toLocaleDateString()} ${date.toLocaleTimeString()}`;
  showDiff(document.querySelector("#mode-edit .desk"), path, then, await textNow(path), { sides: `saved ${when}, then as it stands`, same: `No change from the save at ${when}.` });
}

// Takes a file back to its Local History snapshot kept at `at`, in its tab as one step undo takes
// back, the tab opened for it where none is.
async function revertSnapshot(path, at) {
  const then = await invoke("history_read", { path, at });
  if (!tabOf(path)) {
    await openFile(path);
  }
  const tab = tabOf(path);
  await tab?.reading;
  if (tab?.session && !tab.readOnly) {
    closeDiff();
    replaceText(tab, then, "revert");
    drawTabs();
  }
}

// The open files' diagnostics, a file at a time, for the Problems pane.
function problemFiles() {
  return state.tabs.filter((tab) => !tab.commit && tab.session?.diagnostics?.length).map((tab) => ({ path: tab.file, items: tab.session.diagnostics }));
}

// Draws the Problems pane again, and tells the tool strip how many errors and warnings there are.
function problemsChanged() {
  const files = problemFiles();
  drawProblems(files);
  const all = files.flatMap((file) => file.items);
  window.dispatchEvent(new CustomEvent("problems-changed", { detail: { errors: all.filter((one) => one.severity === 1).length, warnings: all.filter((one) => one.severity === 2).length } }));
}

// Go to Definition, and a click with Ctrl held: where the language server says the symbol at `p`
// is defined. A file under the tree opens there; one outside it, as a system header is, is named.
async function goToDefinition(p) {
  const tab = tabOf(state.active);
  if (!tab?.served) {
    say(`No language server serves ${tab?.session?.language?.name ?? "plain text"}: File, Toolchains lists those there are.`);
    return;
  }
  const [found] = await definition(tab, p);
  if (!found) {
    say("No definition found.");
  } else if (/^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(found.path)) {
    say(`Defined at ${found.path}:${found.line + 1}`);
  } else {
    await openAt(found.path, found.line, found.col);
  }
}

// Takes the ends of a tab's lines off and puts a line end after its last line, as the settings say,
// as one edit that undo takes back.
function tidy(tab) {
  const s = tab.session;
  if (!s || s.readOnly || s.window) {
    return;
  }
  const doc = s.doc;
  const edits = [];
  if (saving("trim") && !/\.(?:md|markdown)$/i.test(tab.file)) {
    for (let line = 0; line < doc.count; line += 1) {
      const text = doc.line(line);
      const kept = text.replace(/[ \t]+$/, "").length;
      if (kept !== text.length) {
        edits.push({ from: { line, col: kept }, to: { line, col: text.length }, text: "" });
      }
    }
  }
  const last = doc.count - 1;
  if (saving("final-newline") && doc.line(last) !== "") {
    const end = { line: last, col: doc.line(last).length };
    edits.push({ from: end, to: end, text: "\n" });
  }
  if (!edits.length) {
    return;
  }
  if (state.editor.s === s) {
    state.editor.change(edits, "tidy");
    return;
  }
  doc.change(edits, "tidy", s.selections);
  s.selections = s.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
  doc.settle(s.selections);
}

// The breadcrumbs.

const CRUMBS_KEY = "orior.crumbs";

// How long a text's symbols are kept for the breadcrumbs after an edit before they are read again,
// in milliseconds.
const CRUMB_SYMBOLS_HELD = 500;

export function crumbsShown() {
  return localStorage.getItem(CRUMBS_KEY) !== "false";
}

export function setCrumbs(on = !crumbsShown()) {
  localStorage.setItem(CRUMBS_KEY, String(on));
  drawCrumbs();
}

function crumbSymbols(s) {
  const now = performance.now();
  if (!s.crumbs || (s.crumbs.id !== s.doc.id && now - s.crumbs.at > CRUMB_SYMBOLS_HELD)) {
    s.crumbs = { id: s.doc.id, at: now, symbols: symbolsOf(s.language?.id, s.doc) };
  }
  return s.crumbs.symbols;
}

// The symbols the cursor's line is inside: the last at each depth that starts at or above it, where
// the region it opens reaches the line or it stands on the line itself.
function symbolsAround(s, line) {
  const held = [];
  for (const symbol of crumbSymbols(s)) {
    if (symbol.line > line) {
      break;
    }
    held.length = Math.min(held.length, symbol.depth);
    held.push(symbol);
  }
  return held.filter((symbol) => symbol.line === line || s.endOf(symbol.line) >= line);
}

// Opens a folder of the tree and every folder above it, and gives its row the keys.
async function revealFolder(folder) {
  togglePane(true);
  let at = folder.length;
  while (at > 0) {
    state.expanded.add(folder.slice(0, at));
    at = folder.lastIndexOf("/", at - 1);
  }
  await drawTree();
  const row = [...document.querySelectorAll("#files .node")].find((one) => one.dataset.key === folder);
  row?.scrollIntoView({ block: "nearest" });
  row?.focus();
}

function drawCrumbs() {
  const bar = document.getElementById("crumbs");
  const tab = tabOf(state.active);
  bar.hidden = !tab || !crumbsShown();
  if (bar.hidden) {
    return;
  }
  const parts = tab.file.split("/");
  const crumb = (text, run, className = "") => {
    const button = element("button", { type: "button", className: `crumb ${className}`.trim(), textContent: text });
    button.addEventListener("click", run);
    return button;
  };
  const nodes = [];
  parts.slice(0, -1).forEach((name, at) => nodes.push(crumb(name, () => revealFolder(parts.slice(0, at + 1).join("/")))));
  const file = crumb(tabName(tab), () => {
    togglePane(true);
    reveal(tab.path).then(() => [...document.querySelectorAll("#files .node")].find((one) => one.dataset.key === tab.path)?.focus());
  }, "file");
  file.prepend(iconOf(tab.file.split("/").pop(), Boolean(state.known?.typeOf(tab.file))));
  nodes.push(file);
  const s = tab.session;
  if (s && state.editor?.s === s) {
    for (const symbol of symbolsAround(s, state.editor.head().line)) {
      nodes.push(crumb(symbol.name, () => jumpTo(symbol.line), "symbol"));
    }
  }
  const key = nodes.map((node) => node.textContent).join("/");
  if (bar.dataset.key === key) {
    return;
  }
  bar.dataset.key = key;
  bar.replaceChildren(...nodes.flatMap((node, at) => (at ? [element("span", { className: "crumb-gap", textContent: "›", ariaHidden: "true" }), node] : [node])));
}

// Marks the lines of a tab's text that differ from the last commit. A file read only in part, a file
// as a commit left it, and a file the last commit does not hold have no marks.
async function markChanges(tab) {
  const s = tab?.session;
  if (!s) {
    return;
  }
  if (tab.commit || s.window || s.base) {
    state.editor.setChanges(s, null);
    return;
  }
  if (!state.heads.has(tab.file)) {
    state.heads.set(tab.file, invoke("file_head", { path: tab.file }).catch(() => null));
  }
  const head = await state.heads.get(tab.file);
  if (typeof head !== "string") {
    state.editor.setChanges(s, null);
    return;
  }
  if (s.changesFor === s.doc.id && s.changesHead === head) {
    return;
  }
  s.changesFor = s.doc.id;
  s.changesHead = head;
  s.changesThen = head.split(/\r?\n/);
  state.editor.setChanges(s, lineChanges(s.changesThen, s.doc.lines));
}

// The change peek. A scroll in the first PEEK_SETTLES milliseconds after it shows brought its change
// into sight, and leaves it.

const PEEK_SETTLES = 400;

// The change a line of the text stands in, or for lines taken out, the one above the line.
function hunkAt(s, line) {
  return s.changes?.hunks.find((hunk) => (hunk.now[0] === hunk.now[1] ? hunk.now[0] === line : line >= hunk.now[0] && line < hunk.now[1])) ?? null;
}

function closePeek() {
  state.peek?.remove();
  state.peek = null;
}

// Puts a change's lines back as the last commit had them, as one edit undo takes back.
function revertHunk(hunk) {
  const s = state.editor.s;
  const doc = s.doc;
  const [first, end] = hunk.now;
  const then = s.changesThen.slice(hunk.then[0], hunk.then[1]);
  const lineEnd = (line) => ({ line, col: doc.line(line).length });
  let edit;
  if (end > first && then.length) {
    edit = { from: { line: first, col: 0 }, to: lineEnd(end - 1), text: then.join("\n") };
  } else if (end > first) {
    edit = end < doc.count ? { from: { line: first, col: 0 }, to: { line: end, col: 0 }, text: "" } : { from: first > 0 ? lineEnd(first - 1) : { line: 0, col: 0 }, to: lineEnd(end - 1), text: "" };
  } else {
    edit = first < doc.count ? { from: { line: first, col: 0 }, to: { line: first, col: 0 }, text: `${then.join("\n")}\n` } : { from: lineEnd(first - 1), to: lineEnd(first - 1), text: `\n${then.join("\n")}` };
  }
  closePeek();
  state.editor.change([edit], "revert");
  state.editor.focus();
  markChanges(tabOf(state.active));
}

// Goes to the change `by` changes from this one, the first after the last and the last before the
// first, and shows it where `peek` is set.
function stepChange(by, peek = false) {
  const s = state.editor?.s;
  const hunks = s?.changes?.hunks ?? [];
  if (!hunks.length) {
    return;
  }
  const line = state.editor.head().line;
  let at;
  if (by > 0) {
    at = hunks.findIndex((hunk) => hunk.now[0] > line);
    at = at < 0 ? 0 : at;
  } else {
    at = hunks.findLastIndex((hunk) => Math.max(hunk.now[0], hunk.now[1] - 1) < line && hunk.now[0] < line);
    at = at < 0 ? hunks.length - 1 : at;
  }
  const hunk = hunks[at];
  jumpTo(hunk.now[0]);
  if (peek) {
    showPeek(hunk.now[0]);
  }
}

function showPeek(line) {
  const s = state.editor?.s;
  const hunk = s && hunkAt(s, line);
  if (!hunk || !s.changesThen) {
    return;
  }
  closePeek();
  const hunks = s.changes.hunks;
  const index = hunks.indexOf(hunk);
  const button = (text, title, run) => {
    const made = element("button", { type: "button", className: "peek-button", textContent: text, title });
    made.addEventListener("click", run);
    return made;
  };
  const go = (by) => () => {
    const next = hunks[(index + by + hunks.length) % hunks.length];
    jumpTo(next.now[0]);
    showPeek(next.now[0]);
  };
  const lines = [
    ...s.changesThen.slice(hunk.then[0], hunk.then[1]).map((text) => element("div", { className: "peek-line removed", textContent: text || " " })),
    ...s.doc.lines.slice(hunk.now[0], hunk.now[1]).map((text) => element("div", { className: "peek-line added", textContent: text || " " })),
  ];
  const panel = element(
    "div",
    { className: "peek", role: "dialog", ariaLabel: `Change ${index + 1} of ${hunks.length}` },
    element(
      "div",
      { className: "peek-head" },
      element("span", { className: "peek-title", textContent: `Change ${index + 1} of ${hunks.length}` }),
      button("Revert", "Revert Change", () => revertHunk(hunk)),
      button("↑", "Previous Change", go(-1)),
      button("↓", "Next Change", go(1)),
      button("×", "Close", () => {
        closePeek();
        state.editor.focus();
      })
    ),
    element("div", { className: "peek-lines" }, ...lines)
  );
  const host = document.getElementById("editor");
  panel.dataset.line = String(line);
  host.append(panel);
  state.peek = panel;
  state.peekAt = performance.now();
  const box = host.getBoundingClientRect();
  const last = hunk.now[1] > hunk.now[0] ? hunk.now[1] - 1 : Math.max(0, hunk.now[0] - 1);
  const below = state.editor.rectOf({ line: last, col: 0 }).bottom - box.top;
  const above = state.editor.rectOf({ line: hunk.now[0], col: 0 }).top - box.top;
  panel.style.left = `${state.editor.gutter.offsetWidth}px`;
  panel.style.top = `${below + panel.offsetHeight <= host.clientHeight ? below : Math.max(0, above - panel.offsetHeight)}px`;
}

// Places: where the cursor stands, kept before each jump for Back and Forward.

function here() {
  const tab = tabOf(state.active);
  if (!tab?.session) {
    return tab ? { path: tab.path, line: 0, col: 0 } : null;
  }
  const head = tab.session.selections[tab.session.primary].head;
  return { path: tab.path, line: tab.session.base + head.line, col: head.col };
}

// Keeps the place the cursor stands at for Back, unless it is the last one kept or next to it.
function markPlace() {
  const place = here();
  const last = state.back.at(-1);
  if (!place || (last && last.path === place.path && Math.abs(last.line - place.line) < 2)) {
    return;
  }
  state.back.push(place);
  state.back.splice(0, state.back.length - PLACES);
  state.forward = [];
}

// Goes to a line of the file shown, keeping the place it leaves for Back.
export function jumpTo(line, col = 0) {
  if (!state.editor?.s) {
    return;
  }
  markPlace();
  state.editor.goTo(line, col);
  state.editor.focus();
}

async function goPlace(place) {
  if (!tabOf(place.path) && place.path.includes("@")) {
    return false;
  }
  state.moving = true;
  try {
    await openFile(place.path);
  } catch {
    return false;
  } finally {
    state.moving = false;
  }
  const s = tabOf(place.path)?.session;
  if (s && state.active === place.path) {
    state.editor.goTo(place.line - s.base, place.col);
  }
  return true;
}

// Steps back, by -1, or forward, by 1, through the places kept.
async function step(by) {
  const [from, to] = by < 0 ? [state.back, state.forward] : [state.forward, state.back];
  while (from.length) {
    const place = from.pop();
    const left = here();
    if (await goPlace(place)) {
      if (left) {
        to.push(left);
      }
      return;
    }
  }
}

// Last Editor: each press while Ctrl is held steps one tab further back in when each was shown, and
// letting Ctrl go keeps the tab it stopped on as the one shown last.
function lastEditor() {
  const order = state.used.filter(tabOf);
  if (order.length < 2) {
    return;
  }
  if (!state.cycle) {
    state.cycle = { order, at: 0 };
  }
  state.cycle.at = (state.cycle.at + 1) % state.cycle.order.length;
  show(state.cycle.order[state.cycle.at]);
}

function endCycle() {
  if (!state.cycle) {
    return;
  }
  state.cycle = null;
  if (state.active) {
    state.used = [state.active, ...state.used.filter((one) => one !== state.active)];
  }
}

// What each tree keeps: its tabs and the one shown, the files opened last, and the text of each file
// with changes not saved.

const treeKey = () => document.getElementById("tree-path").textContent;
const sessionKey = () => `orior.session.${treeKey()}`;
const recentKey = () => `orior.recent.${treeKey()}`;
const backupKey = (path) => `orior.backup.${treeKey()}\n${path}`;

function readKept(key, fallback) {
  try {
    return JSON.parse(localStorage.getItem(key) ?? "null") ?? fallback;
  } catch {
    return fallback;
  }
}

function keepSession() {
  if (!state.restored) {
    return;
  }
  const tabs = state.tabs.filter((tab) => !tab.commit).map((tab) => tab.path);
  localStorage.setItem(sessionKey(), JSON.stringify({ tabs, active: tabOf(state.active)?.commit ? null : state.active }));
}

function keepRecent(path) {
  localStorage.setItem(recentKey(), JSON.stringify([path, ...readKept(recentKey(), []).filter((one) => one !== path)].slice(0, RECENT)));
}

// The files of the tree opened last, the last first.
export function recentFiles() {
  return readKept(recentKey(), []);
}

function forgetBackup(path) {
  localStorage.removeItem(backupKey(path));
}

// Keeps the text of each tab with changes not saved, and lets go of each without.
function keepBackups() {
  for (const tab of state.tabs) {
    if (!tab.session || tab.readOnly || tab.session.window) {
      continue;
    }
    if (!dirty(tab)) {
      forgetBackup(tab.path);
      continue;
    }
    const text = tab.session.doc.text();
    if (text.length > BACKUP_MOST) {
      continue;
    }
    try {
      localStorage.setItem(backupKey(tab.path), text);
    } catch {
      forgetBackup(tab.path);
    }
  }
}


// Opens the tabs the tree had open, and shows the one it showed.
export async function restoreSession() {
  const kept = readKept(sessionKey(), { tabs: [], active: null });
  for (const path of kept.tabs ?? []) {
    await load(path).catch(() => {});
  }
  state.restored = true;
  const shown = tabOf(kept.active) ? kept.active : state.tabs.at(-1)?.path;
  if (shown) {
    state.moving = true;
    show(shown);
    state.moving = false;
  } else {
    keepSession();
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
  const side = document.getElementById("defs-side");
  side.hidden = !type && !tables.length && !bridged;
  if (side.hidden) {
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
  await loadPlugins();
  state.known = registerLanguages(defs);
  // A plugin read again, turned on or turned off colors every open file anew.
  onPlugins(() => {
    for (const tab of state.tabs) {
      tab.session?.setLanguage(state.known.languageOf(tab.file));
      wrap(tab);
    }
    state.editor?.restyle();
  });
  state.head = defs.head;
  let lighting = 0;
  let keeping = 0;
  let outlining = 0;
  let backing = 0;
  let marking = 0;
  let autoSaving = 0;
  window.addEventListener("keyup", (event) => event.key === "Control" && endCycle());
  window.addEventListener("blur", endCycle);
  const cursorLine = () => (state.editor?.s ? state.editor.head().line : 0);
  onFonts(() => state.editor?.restyle());
  state.editorHooks = {
    onChangeMark: (line) => (state.peek && hunkAt(state.editor.s, line) && state.peek.dataset.line === String(line) ? closePeek() : showPeek(line)),
    onCursor: () => {
      moved();
      cancelAnimationFrame(lighting);
      lighting = requestAnimationFrame(() => {
        light();
        drawCrumbs();
        lightOutline(cursorLine());
      });
      window.clearTimeout(keeping);
      keeping = window.setTimeout(keepPlace, 500);
    },
    onChange: (session) => {
      const tab = state.tabs.find((one) => one.session === session);
      if (tab) {
        tab.closing = false;
        changed(tab);
        typed();
        if (tab.validated) {
          tab.validated = false;
          session.diagnostics = null;
          problemsChanged();
        }
      }
      window.clearTimeout(backing);
      backing = window.setTimeout(keepBackups, 800);
      window.clearTimeout(marking);
      marking = window.setTimeout(() => markChanges(tabOf(state.active)), 400);
      window.clearTimeout(autoSaving);
      if (saving("auto-save")) {
        autoSaving = window.setTimeout(() => editing().saveAll({ auto: true }), AUTO_SAVE_REST);
      }
      drawTabs();
      window.clearTimeout(outlining);
      outlining = window.setTimeout(() => drawOutline(tabOf(state.active)?.session ?? null), 300);
    },
  };
  state.editor = new Editor(document.getElementById("editor"), { ...state.editorHooks, statusHost: document.getElementById("statusbar") });
  state.editor.input.addEventListener("focus", () => splitFocused(false));
  state.editor.onDefinition = (p) => goToDefinition(p);
  state.editor.addKeys(state.editorKeys);
  // The debugger's marks in the gutter, by the file a session shows, a split's the same as the
  // session it was made from. A file as a commit left it has none.
  const fileOfSession = (s) => state.tabs.find((tab) => tab.session === (s?.of ?? s) && !tab.commit)?.file ?? null;
  state.editor.breakpointsOf = (s) => {
    const file = fileOfSession(s);
    return file ? breakpointsOf(file) : null;
  };
  state.editor.bookmarksOf = (s) => {
    const file = fileOfSession(s);
    return file ? bookmarksOf(file) : null;
  };
  startBookmarks({
    here: () => {
      const tab = tabOf(state.active);
      return tab?.session && !tab.commit && state.editor.s === tab.session ? { path: tab.file, line: tab.session.base + state.editor.head().line } : null;
    },
    lineOf: (path, line) => {
      const s = state.tabs.find((tab) => tab.file === path && !tab.commit && tab.session && !tab.session.window)?.session;
      return s ? s.doc.line(line - s.base) : null;
    },
    openAt: (path, line, col) => openAt(path, line, col),
    repaint: () => [state.editor, state.split?.editor].forEach((one) => one?.schedule()),
  });
  state.editor.pausedOf = (s) => {
    const file = fileOfSession(s);
    return file ? pausedLineOf(file) : null;
  };
  // The status bar's lock: a file of the tree turns between read-only and writable for as long as its
  // tab is open, and a file as a commit left it stays read-only.
  state.editor.onLock = (given) => {
    const s = given.of ?? given;
    const tab = state.tabs.find((one) => one.session === s);
    if (!tab || tab.commit) {
      say("A file as a commit left it is read-only.");
      return;
    }
    s.readOnly = !s.readOnly;
    tab.readOnly = s.readOnly;
    state.editor.schedule();
    state.split?.editor.schedule();
    drawTabs();
  };
  state.editor.onBreakpoint = (line) => {
    const file = fileOfSession(state.editor.s);
    if (file) {
      toggleBreakpoint(file, line);
    }
  };
  // A split editor takes the first one's keys and marks, and its breakpoints are the file's.
  state.lendTo = (editor) => {
    editor.onDefinition = state.editor.onDefinition;
    editor.addKeys(state.editorKeys);
    for (const name of ["breakpointsOf", "bookmarksOf", "pausedOf", "onLock"]) {
      editor[name] = state.editor[name];
    }
    editor.onBreakpoint = (line) => {
      const file = fileOfSession(editor.s);
      if (file) {
        toggleBreakpoint(file, line);
      }
    };
  };
  startCommit({
    shown: () => paneOpen("changes") && shownGroup() === "commit",
    saveAll: () => editing().saveAll(),
    reload: (paths) => reloadFromDisk(paths),
    refresh: async () => {
      await loadChanges();
      await drawTree();
    },
    diff: (path, mark) => openDiff(path, mark),
    open: (path) => openFile(path),
  });
  startDebug({
    editor: () => state.editor,
    tab: () => tabOf(state.active),
    save: async (tab) => {
      if (dirty(tab) && !tab.readOnly) {
        state.active = tab.path;
        await saveActive();
      }
    },
    open: (path) => openFile(path),
    openAt: (path, line, col) => openAt(path, line, col),
    repaint: () => [state.editor, state.split?.editor].forEach((one) => one?.schedule()),
    run: (command) => import("./menubar.js").then((menus) => menus.runCommand(command)),
  });
  startServers({
    tabs: () => state.tabs,
    paint: () => {
      state.editor.schedule();
      state.split?.editor.schedule();
      problemsChanged();
    },
  });
  startIntel({
    editor: () => state.editor,
    tab: () => tabOf(state.active),
    tabs: () => state.tabs,
    lineOf: (path, line) => {
      const s = state.tabs.find((tab) => tab.file === path && !tab.commit && tab.session && !tab.session.window)?.session;
      return s ? s.doc.line(line - s.base) : null;
    },
    openAt: (path, line, col) => openAt(path, line, col),
    touched: (tab) => {
      changed(tab);
      drawTabs();
    },
    refresh: async () => {
      await loadChanges();
      await drawTree();
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
    goTo: (line) => jumpTo(line),
    cursorLine,
    openCommit,
    showSnapshot,
    revertSnapshot,
    refresh: async () => {
      state.children.clear();
      await loadChanges();
      await drawTree();
      drawTimeline(fileOf(state.active), true);
      drawLocalHistory(fileOf(state.active), true);
      drawTodo();
    },
    collapse: () => {
      state.expanded = new Set([""]);
      drawTree();
    },
    panesChanged: () => {
      drawOutline(tabOf(state.active)?.session ?? null);
      drawTimeline(state.active ? fileOf(state.active) : null);
      drawLocalHistory(state.active ? fileOf(state.active) : null);
      drawCommit(state.changes);
      drawProblems(problemFiles());
      drawGit(state.branch);
      drawTodo();
    },
    openAt: (path, line, col) => openAt(path, line, col),
  });
  keepListKeys(document.getElementById("panes"), document.getElementById("file-filter"));
  // The peek goes with Escape, a press outside it, a scroll of the text, or another file shown.
  window.addEventListener(
    "keydown",
    (event) => {
      if (event.key === "Escape" && state.peek) {
        event.preventDefault();
        event.stopPropagation();
        closePeek();
        state.editor.focus();
      }
    },
    true
  );
  document.addEventListener("mousedown", (event) => state.peek && !state.peek.contains(event.target) && !event.target.closest?.(".ed-change") && closePeek());
  state.editor.scroller.addEventListener("scroll", () => state.peek && performance.now() - state.peekAt > PEEK_SETTLES && closePeek());
  menuOn(document.getElementById("files"), fileItems);
  menuOn(document.getElementById("tabs"), tabItems);
  menuOn(document.getElementById("editor"), editorItems);
  window.addEventListener("focus", async () => {
    state.heads.clear();
    markChanges(tabOf(state.active));
    await loadChanges();
    drawTree();
  });
  document.getElementById("defs-side").hidden = true;
  document.getElementById("editor").hidden = true;
  drawEmpty(true);
  loadBridge();
  keepBridge(() => inBridge(state.active) && drawDefs());
  await loadChanges();
  await drawTree();
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
    { label: "Split Right", disabled: !tab.session || Boolean(tab.commit), run: () => (show(tab.path), splitEditor("right")) },
    { label: "Split Down", disabled: !tab.session || Boolean(tab.commit), run: () => (show(tab.path), splitEditor("down")) },
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
  const served = Boolean(tabOf(state.active)?.served);
  return [
    { label: "Go to Definition", keys: "F12", disabled: !served, run: () => goToDefinition(editor.head()) },
    { label: "Find Usages", keys: "Shift+F12", run: findUsages },
    { label: "Rename Symbol…", keys: "F2", disabled: !served, run: renameSymbol },
    { label: "Quick Fix…", keys: "Ctrl+.", disabled: !served, run: quickFix },
    {
      label: "Refactor",
      items: [
        { label: "Extract Variable", keys: "Ctrl+Alt+V", run: () => extractVariable(editor, fileOf(state.active)) },
        { label: "Extract Constant", keys: "Ctrl+Alt+C", run: () => extractConstant(editor) },
        { label: "Extract Function", keys: "Ctrl+Alt+M", run: () => extractFunction(editor, tabOfSession(editor.s)) },
        { label: "Inline Variable", keys: "Ctrl+Alt+N", run: () => inlineVariable(editor) },
      ],
    },
    "-",
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

// Gives the editor the keys of the menus' commands for it, each as `{ keys, run }`, now or as it is
// made.
export function bindEditorKeys(list) {
  state.editorKeys = list;
  state.editor?.addKeys(list);
}

// What the menu bar does to the editor: the editor where a file of text is open in it, saving one
// file or all of them, and closing one tab or all of them.
export function editing() {
  return {
    editor: state.split?.focused ? state.split.editor : state.editor?.s ? state.editor : null,
    active: state.active,
    split: splitEditor,
    unsplit,
    // The tree read again after a file, a folder or a repository was made in it.
    treeChanged: async () => {
      state.children.clear();
      await loadChanges();
      await drawTree();
    },
    // The folder of the file open, as the start of a path for a file or folder to make.
    folderHere: () => {
      const file = fileOf(state.active);
      return file?.includes("/") ? file.slice(0, file.lastIndexOf("/") + 1) : "";
    },
    splitShown: () => Boolean(state.split),
    changed: state.tabs.some(dirty),
    activeChanged: Boolean(tabOf(state.active) && dirty(tabOf(state.active))),
    open: state.tabs.length > 0,
    save: saveActive,
    format: () => formatTab(tabOf(state.active)),
    runFile: () => runTab(tabOf(state.active)),
    definition: () => state.editor?.s && goToDefinition(state.editor.head()),
    usages: findUsages,
    rename: renameSymbol,
    extractVariable: () => state.editor?.s && extractVariable(editing().editor ?? state.editor, fileOf(state.active)),
    extractConstant: () => state.editor?.s && extractConstant(editing().editor ?? state.editor),
    extractFunction: () => state.editor?.s && extractFunction(editing().editor ?? state.editor, tabOfSession((editing().editor ?? state.editor).s)),
    inlineVariable: () => state.editor?.s && inlineVariable(editing().editor ?? state.editor),
    quickFix,
    parameterInfo: () => parameterInfo(),
    quickDoc,
    validate: () => validateTab(tabOf(state.active)),
    saveAll: async ({ auto = false } = {}) => {
      const shown = state.active;
      for (const tab of state.tabs.filter(dirty)) {
        await tab.reading;
        if (!auto && saving("format")) {
          await formatTab(tab, { saving: true });
        }
        tidy(tab);
        const writing = tab.session.doc.id;
        const lines = tab.session.doc.lines.slice();
        const eol = tab.session.doc.eol;
        const written = tab.session.doc.text();
        await invoke("file_write", { path: tab.path, text: written });
        tab.saved = writing;
        tab.savedLines = lines;
        tab.savedEol = eol;
        tab.comparedAt = null;
        tab.closing = false;
        forgetBackup(tab.path);
        if (inBridge(tab.path)) {
          loadBridge().then(drawDefs);
        }
      }
      state.active = shown;
      drawTabs();
      drawLocalHistory(fileOf(state.active), true);
      await loadChanges();
      drawTree();
    },
    close: () => tabOf(state.active) && closeTab(tabOf(state.active)),
    closeAll: () => [...state.tabs].forEach(closeTab),
    brackets: () => Boolean(state.editor?.bracketsOn),
    setBrackets: (on) => state.editor?.setBrackets(on),
    sticky: () => Boolean(state.editor?.stickyOn),
    setSticky: (on) => state.editor?.setSticky(on),
    nextChange: () => stepChange(1, true),
    previousChange: () => stepChange(-1, true),
    hasChanges: () => Boolean(state.editor?.s?.changes?.hunks.length),
    columnMode: () => Boolean(state.editor?.columnMode),
    setColumnMode: (on) => state.editor?.setColumnMode(on),
    back: () => step(-1),
    forward: () => step(1),
    lastEditor,
    lineCount: () => (state.editor?.s ? state.editor.s.base + state.editor.s.doc.count : null),
    symbols: () => {
      const s = state.editor?.s;
      return s ? symbolsOf(s.language?.id, s.doc).map((symbol) => ({ ...symbol, line: s.base + symbol.line, shownLine: s.base + symbol.line + 1 })) : null;
    },
    goLine: (line, col) => {
      const s = state.editor?.s;
      if (s) {
        jumpTo(line - s.base, col);
      }
    },
    find: () => {
      togglePane(true);
      const filter = document.getElementById("file-filter");
      filter.focus();
      filter.select();
    },
  };
}

// Forgets the folders read so far and the tabs, for a tree opened in place of this one. What the tabs
// held stays kept with the tree they were open in.
export function forgetTree() {
  stopDebug();
  keepBackups();
  state.tabs.forEach(stopServing);
  state.restored = false;
  state.heads.clear();
  state.tabs = [];
  state.used = [];
  state.back = [];
  state.forward = [];
  state.active = null;
  show(null);
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

