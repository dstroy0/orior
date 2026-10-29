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
import { findWith } from "./editor/find.js";
import { inBridge, loadBridge } from "./bridge_panel.js";
import { drawGit, drawLocalHistory, drawOutline, drawTimeline, drawUndo, forgetGraph } from "./explorer.js";
import { symbolsOf } from "./outline.js";
import { registerLanguages } from "./languages.js";
import { loadPlugins, onPlugins } from "./plugins.js";
import { forgetProblems, serve, stopServing, wrap } from "./servers.js";
import { callHierarchy, closeSignature, findUsages, parameterInfo, quickDoc, quickFix, renameSymbol } from "./intel.js";
import { extractConstant, extractVariable, inlineVariable } from "./refactor.js";
import { extractFunction } from "./extract.js";
import { stopDebug } from "./debug.js";
import { drawNotebook, hideNotebook } from "./notebook.js";
import { writeWithCare } from "./rights.js";
import { syncPlugins } from "./scripts.js";
import { togglePane } from "./sides.js";
import { onOverBudget } from "./statusbar.js";
import { findPaged, inWindow, jumpPaged } from "./paged.js";
import { compareWithClipboard, conflicted, openMerge } from "./edit/compare.js";
import { checkShown, drawCrumbs, goToDefinition, tidy } from "./edit/crumbs.js";
import { drawBinary, drawDefs } from "./edit/defs.js";
import { along, forgetHeads, markChanges, sendChanges, setLineHistory } from "./edit/held.js";
import { closeSplitTab, moveToNextSplit, openInSplit, splitEditor, unsplit } from "./edit/open.js";
import { bindPage, startPanes } from "./edit/panes.js";
import { closePeek, stepChange } from "./edit/peek.js";
import { forgetBackup, jumpTo, keepBackups, keepRecent, lastEditor, markPlace, step } from "./edit/places.js";
import { changeSignatureHere, docstringForm, fillParagraph, holdMemory, moveHere, setDocstringForm, shapeSearchHere, sortMethods, writeDocstring } from "./edit/refactor.js";
import { formatTab, runTab, saveActive, saving, validateTab, verifyDisassemblyTab } from "./edit/save.js";
import { hookEditor, startEditor } from "./edit/start.js";
import { actingPath, closeTab, dirty, drawTabs, fileOf, goMark, markItems, rowOverflowing, setSideTabs, sideTabs, tabOf, tabOfSession, toggleMark } from "./edit/tabs.js";
import { drawRepoBranch, drawTree, loadChanges, repoName, repoOf, repoOpen, treeRowShown } from "./edit/tree.js";
export { crumbsShown, setCrumbs } from "./edit/crumbs.js";
export { bindEditorKeys, bindReaderKeys, marginLine, popupCommands, setMarginLine, setVimKeys, takeSettings, viewEditor, viewSession, vimKeys } from "./edit/menus.js";
export { openFile } from "./edit/open.js";
export { jumpTo, recentFiles, restoreSession } from "./edit/places.js";
export { openAt, saving, setSaving } from "./edit/save.js";
export { addToWindow, chooseNames, loadMounts, treeRowShown } from "./edit/tree.js";

export const state = {
  editor: null,
  // The split beside the first side or under it, or null: its editor, its tab strip, its tabs'
  // paths and the one it shows, each file's own session over the file's text, and whether its
  // editor was pressed in last.
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
  repos: null,
  // The places jumped from, and the ones gone back from.
  back: [],
  forward: [],
  moving: false,
  // Whether the tabs the tree had open are being opened again as it starts.
  restoring: false,
  // The tabs by when each was last shown, and the walk Last Editor is on while Ctrl is held.
  used: [],
  cycle: null,
  // How many times the last commit's files were forgotten, for a branch on the window's side to know
  // to read its file as the commit holds it again.
  headsAge: 0,
  // How many files the system clipboard holds, for the tree's Paste.
  clipFiles: 0,
  // The batches of changes on the disk being taken, one after another.
  watched: Promise.resolve(),
  // How many times the tree was drawn, which tells a draw a later one overtook it, and the latest.
  drawing: 0,
  drawn: null,
  // The keys of the menus' commands for the editor.
  editorKeys: [],
  // The keys the reader bound, over the editor's own.
  boundKeys: [],
  // Where a file opened goes instead of this window's editor, as from a window that shows a tool
  // window alone, or null.
  elsewhere: null,
  // The named groups of the first side's tabs, each its name, its tabs' paths and whether it is
  // folded.
  groups: [],
  // The folders and files from other places mounted beside the tree, each its name, its path on the
  // disk and whether it is a folder.
  mounts: [],
};

// How many places Back holds, how many files the quick open lists as opened last, and the largest
// text kept for a file with changes not saved.
export const PLACES = 50;
export const RECENT = 30;
export const BACKUP_MOST = 2 * 1024 * 1024;

// Which git state a folder takes from the files in it: the first of these that any of them is.
export const ROLL_ORDER = ["C", "M", "D", "A", "R", "U"];

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export const extOf = (path) => (path.includes(".") ? path.split(".").pop().toLowerCase() : "");

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
  treeRowShown(path);
}

export function show(path) {
  closePeek();
  closeSignature();
  if (path !== state.active && state.active && !state.moving) {
    markPlace();
  }
  state.active = path;
  const tab = tabOf(path);
  if (tab) {
    tab.inMain = true;
  }
  if (tab && !state.cycle) {
    state.used = [path, ...state.used.filter((one) => one !== path)];
  }
  if ((state.repos?.length ?? 0) > 1) {
    drawRepoBranch();
    drawGit(state.branch);
  }
  // A tab whose server was stopped to hold memory to its budget is handed to one again as it shows.
  if (tab?.session && tab.served === undefined && !tab.serving && tab.unserved) {
    tab.unserved = false;
    serve(tab).then(() => tab.served && state.editor?.s === tab.session && state.editor.schedule());
  }
  if (tab && !tab.commit) {
    keepRecent(tab.file);
  }
  const editorNode = document.getElementById("editor");
  const binaryNode = document.getElementById("binary");
  const notebookNode = document.getElementById("notebook");
  drawEmpty(!tab);
  notebookNode.hidden = !tab?.notebook;
  if (!tab?.notebook) {
    hideNotebook();
  }
  if (tab?.notebook) {
    state.editor.show(null);
    editorNode.hidden = true;
    binaryNode.hidden = true;
    drawNotebook(tab.notebook);
  } else if (tab?.session) {
    editorNode.hidden = false;
    binaryNode.hidden = true;
    state.editor.show(tab.session);
    // A tab shown as the session comes back takes the keys only where nothing else has them: what the
    // reader began to type into at the start keeps them.
    if (!state.restoring || !document.activeElement || document.activeElement === document.body) {
      state.editor.focus();
    }
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
  drawUndo(tab?.session ?? null);
  drawTimeline(tab ? tab.file : null);
  drawLocalHistory(tab?.commit ? null : (tab?.file ?? null));
  shownListeners.forEach((listener) => listener(tab?.file ?? null));
}

// What is told each time the file shown changes, with the file's path, or null where none shows.
const shownListeners = [];

// What is told of each change to an open file's text as it is made, with the file's session, and of
// each move of the cursor once the keys rest.
export const editListeners = [];
export const cursorListeners = [];

export function onEdit(listener) {
  editListeners.push(listener);
}

export function onCursorMoved(listener) {
  cursorListeners.push(listener);
}

// What may give a paste into a file another text: each is asked with the file's path and the text,
// and the first that gives a text, or a promise of one, is pasted in its place.
export const pasteListeners = [];

export function onPasteAs(listener) {
  pasteListeners.push(listener);
}

export function onShown(listener) {
  shownListeners.push(listener);
}

// With no file open the desk shows the name over the lattice, as the run view does with no job
// chosen, and the tab bar goes until a tab is in it.
export function drawEmpty(shown) {
  document.getElementById("tabs").hidden = shown;
  document.getElementById("edit-empty").hidden = !shown;
}

export async function startEdit(defs) {
  document.querySelector("#mode-edit .desk").dataset.tabs = sideTabs() ? "side" : "row";
  new ResizeObserver(() => rowOverflowing()).observe(document.getElementById("tabs"));
  // Find runs on the window's side: over the branch a text is held as, once the changes before it have gone there, and
  // over the pages a paged file is held as, the window brought to a match outside it.
  findWith({
    search: (s, sought, most) => {
      if (s.window?.paged) {
        return findPaged(s, sought, most).catch(() => null);
      }
      const held = s.doc.held;
      if (!held?.ready) {
        return null;
      }
      sendChanges(s.doc);
      return along(held, () => invoke("document_find", { key: held.key, sought, most })).catch(() => null);
    },
    reach: (s, line) => (s.window?.paged && !inWindow(s, line) ? jumpPaged(s, line) : Promise.resolve()),
  });
  onOverBudget(holdMemory);
  // The changes not saved are kept at once as the window closes, is hidden or loses the keys, and not
  // only a moment after typing rests: no window's changes are lost, whichever closes first.
  window.addEventListener("pagehide", keepBackups);
  window.addEventListener("beforeunload", keepBackups);
  window.addEventListener("blur", keepBackups);
  document.addEventListener("visibilitychange", () => document.hidden && keepBackups());
  await loadPlugins();
  state.known = registerLanguages(defs);
  // A plugin read again, turned on or turned off colors every open file anew.
  onPlugins(() => {
    for (const tab of state.tabs) {
      tab.session?.setLanguage(state.known.languageOf(tab.file));
      wrap(tab);
    }
    state.editor?.restyle();
    syncPlugins();
  });
  state.head = defs.head;
  hookEditor();
  startEditor();
  startPanes();
  await bindPage();
}


export function editing() {
  return {
    editor: state.split?.focused ? state.split.editor : state.editor?.s ? state.editor : null,
    active: actingPath(),
    split: splitEditor,
    unsplit,
    moveToNextSplit,
    openInSplit,
    sideTabs,
    setSideTabs,
    toggleMark,
    goMark,
    markItems,
    // The tree read again after a file, a folder or a repository was made in it.
    treeChanged: async () => {
      state.children.clear();
      await loadChanges();
      await drawTree();
    },
    // The folder of the file open, as the start of a path for a file or folder to make.
    folderHere: () => {
      const file = fileOf(actingPath());
      return file?.includes("/") ? file.slice(0, file.lastIndexOf("/") + 1) : "";
    },
    splitShown: () => Boolean(state.split),
    changed: state.tabs.some(dirty),
    activeChanged: Boolean(tabOf(actingPath()) && dirty(tabOf(actingPath()))),
    open: state.tabs.length > 0,
    // The session of the file at `path` where a tab holds it.
    sessionOf: (path) => tabOf(path)?.session ?? null,
    save: () => saveActive(),
    format: () => formatTab(tabOf(actingPath())),
    runFile: () => runTab(tabOf(actingPath())),
    definition: () => state.editor?.s && goToDefinition(state.editor.head()),
    usages: findUsages,
    calls: callHierarchy,
    rename: renameSymbol,
    extractVariable: () => state.editor?.s && extractVariable(editing().editor ?? state.editor, fileOf(actingPath())),
    extractConstant: () => state.editor?.s && extractConstant(editing().editor ?? state.editor),
    extractFunction: () => state.editor?.s && extractFunction(editing().editor ?? state.editor, tabOfSession((editing().editor ?? state.editor).s)),
    inlineVariable: () => state.editor?.s && inlineVariable(editing().editor ?? state.editor),
    quickFix,
    parameterInfo: () => parameterInfo(),
    quickDoc,
    validate: () => validateTab(tabOf(actingPath())),
    verifyDisassembly: () => verifyDisassemblyTab(tabOf(actingPath())),
    saveAll: async ({ auto = false } = {}) => {
      const shown = state.active;
      for (const tab of state.tabs.filter(dirty)) {
        if (!auto && saving("format")) {
          await formatTab(tab, { saving: true });
        }
        tidy(tab);
        const writing = tab.session.doc.id;
        const lines = tab.session.doc.lines.slice();
        const eol = tab.session.doc.eol;
        const written = tab.session.doc.text();
        await writeWithCare(tab.path, written);
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
    close: () => (state.split?.focused ? closeSplitTab(state.split.active) : tabOf(state.active) && closeTab(tabOf(state.active))),
    closeAll: () => [...state.tabs].forEach(closeTab),
    brackets: () => Boolean(state.editor?.bracketsOn),
    setBrackets: (on) => state.editor?.setBrackets(on),
    marks: () => Boolean(state.editor?.marksOn),
    pastEnds: () => Boolean(state.editor?.pastEnds),
    setPastEnds: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setPastEnds(on)),
    compareWithClipboard,
    conflicted,
    openMerge,
    setMarks: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setMarks(on)),
    repo: repoOpen,
    repoOf,
    repoName,
    repos: () => state.repos ?? [],
    languageOf: (path) => state.known?.languageOf(path) ?? null,
    // The file of the tab open, and its text as the tab holds it, or null where none is open.
    fileHere: () => (actingPath() ? tabOf(actingPath())?.file ?? null : null),
    textHere: () => (actingPath() ? tabOf(actingPath())?.session?.doc.text() ?? null : null),
    holdMemory,
    changeSignature: changeSignatureHere,
    moveDeclaration: moveHere,
    shapeSearch: shapeSearchHere,
    writeDocstring,
    sortMethods,
    docstringForm,
    setDocstringForm,
    lineHistory: () => Boolean(state.lineHistory),
    setLineHistory,
    continuation: () => state.editor?.continuation ?? {},
    setContinuation: (continuation) => [state.editor, state.split?.editor].forEach((one) => one?.setContinuation(continuation)),
    operatorNext: () => Boolean(state.editor?.operatorNext),
    setOperatorNext: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setOperatorNext(on)),
    docMargin: () => state.editor?.docMargin ?? null,
    setDocMargin: (columns) => [state.editor, state.split?.editor].forEach((one) => one?.setDocMargin(columns)),
    fillParagraph,
    hints: (kind) => Boolean(state.editor?.hintKinds[kind]),
    setHints: (kind, on) => [state.editor, state.split?.editor].forEach((one) => one?.setHints(kind, on)),
    smoothScroll: () => Boolean(state.editor?.smoothOn),
    wordWrap: () => Boolean(state.editor?.wrapOn),
    setWordWrap: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setWrap(on)),
    setSmoothScroll: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setSmooth(on)),
    flickScroll: () => Boolean(state.editor?.flickOn),
    setFlickScroll: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setFlick(on)),
    commentsHidden: () => Boolean(state.editor?.commentsHidden),
    setCommentsHidden: (on) => [state.editor, state.split?.editor].forEach((one) => one?.setCommentsHidden(on)),
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
// Sends each file opened in this window to `elsewhere`, a function of its path, line and column, or
// with null keeps them here.
export function openElsewhere(elsewhere) {
  state.elsewhere = elsewhere;
}

export function forgetTree() {
  stopDebug();
  keepBackups();
  state.tabs.forEach(stopServing);
  forgetProblems();
  checkShown();
  state.restored = false;
  forgetHeads();
  state.repos = null;
  state.mounts = [];
  forgetGraph();
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

