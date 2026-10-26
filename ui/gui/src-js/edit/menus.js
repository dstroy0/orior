// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, viewOpen } from "../bridge.js";
import { Session } from "../editor/session.js";
import { Editor } from "../editor/view.js";
import { serve, spansOf, stopServing } from "../servers.js";
import { findUsages, moved, quickFix, renameSymbol } from "../intel.js";
import { extractConstant, extractVariable, inlineVariable } from "../refactor.js";
import { extractFunction } from "../extract.js";
import { clipText, copyText } from "../menu.js";
import { terminalAt } from "../terminal.js";
import { isNotebook, notebookEditor } from "../notebook.js";
import { say } from "../statusbar.js";
import { extOf, show, state } from "../edit.js";
import { compareFiles } from "./compare.js";
import { drawCrumbs, goToDefinition } from "./crumbs.js";
import { load, openFile, openInSplit, splitEditor, splittable } from "./open.js";
import { backupKey, forgetBackup } from "./places.js";
import { holdMemory } from "./refactor.js";
import { saveActive } from "./save.js";
import { addToGroup, closeTab, dirty, drawTabs, fileOf, groupOf, keepMarks, leaveGroup, markedFiles, tabOf, tabOfSession, tabToNewWindow, toggleMark } from "./tabs.js";
import { childrenOf, chooseNames, drawTree, loadChanges, NAME_ENCODINGS, namesChosen, removeFromWindow, treeRowShown } from "./tree.js";

// The menus.

// Files a page window draws, which open in one from the tree's menu.
const SHOWN_IN_WINDOW = new Set(["html", "htm", "svg", "png", "jpg", "jpeg"]);

// A row of the tree's menu: open or close a folder, open a file in the editor or a page in a window
// of its own, cut or copy it, paste files into its folder, copy where it is, or open the terminal in
// its folder. Off the rows, files paste into the tree's top.
// The tree's file menu and a tab's menu as commands, each by the name commands.json gives it, run
// from the menu or from the command line with the words it gives: a path of the tree, a mount's name
// for Remove from Window, and what each names besides.
const rowOfPath = (path) => treeRowShown(path);
const parentOf = (path) => (path.includes("/") ? path.slice(0, path.lastIndexOf("/")) : "");

async function isFolder(path) {
  const row = rowOfPath(path);
  if (row) {
    return row.classList.contains("dir");
  }
  return (await childrenOf(parentOf(path)).catch(() => [])).some((entry) => entry.path === path && entry.dir);
}

const folderOfPath = async (path) => ((await isFolder(path)) ? path : parentOf(path));
const tabNamed = (path) => tabOf(path) ?? state.tabs.find((tab) => tab.file === path) ?? null;

export const popupCommands = {
  "file-open": async ([path]) => {
    if (!(await isFolder(path))) {
      await openFile(path);
      return;
    }
    if (state.expanded.has(path)) {
      state.expanded.delete(path);
    } else {
      state.expanded.add(path);
    }
    await drawTree();
  },
  "file-remove": ([name]) => removeFromWindow(name.replace(/^@/, "")),
  "file-names": ([path, label = ""]) => chooseNames(path, label),
  "file-open-window": ([path]) => viewOpen(path),
  "file-cut": ([path]) => copyFiles(path, true),
  "file-copy": ([path]) => copyFiles(path, false),
  "file-paste": async ([path = ""]) => pasteFiles(path ? await folderOfPath(path) : ""),
  "file-copy-path": ([path]) => copyText(path),
  "file-terminal": async ([path = ""]) => terminalAt(path ? await folderOfPath(path) : ""),
  "file-select-compare": ([path]) => (state.compareFrom = path),
  "file-compare": ([path, other = state.compareFrom]) => other && compareFiles(other, path),
  "tab-close": ([path]) => tabNamed(path) && closeTab(tabNamed(path)),
  "tab-close-others": ([path]) => state.tabs.filter((one) => one !== tabNamed(path)).forEach(closeTab),
  "tab-split-right": ([path]) => (show(tabNamed(path)?.path ?? path), splitEditor("right")),
  "tab-split-down": ([path]) => (show(tabNamed(path)?.path ?? path), splitEditor("down")),
  "tab-new-window": ([path]) => tabNamed(path) && tabToNewWindow(tabNamed(path)),
  "tab-group": ([path, name]) => tabNamed(path) && addToGroup(tabNamed(path), name),
  "tab-ungroup": ([path]) => (leaveGroup(tabNamed(path)?.path ?? path), drawTabs()),
  "tab-mark": ([path]) => toggleMark(tabNamed(path)?.file ?? path),
  "tab-open-as": ([path, as]) => tabNamed(path) && reopenAs(tabNamed(path), as === "notebook" ? "notebook" : "text"),
  "group-fold": ([name, on]) => withGroup(name, (group) => ((group.folded = on === "on" ? true : on === "off" ? false : !group.folded), drawTabs())),
  "group-rename": ([name, given]) =>
    withGroup(name, async (group) => {
      const { askFor } = await import("../menubar.js");
      const to = (given ?? (await askFor("The group's name", group.name)))?.trim();
      if (to) {
        group.name = to;
        drawTabs();
      }
    }),
  "group-ungroup": ([name]) => withGroup(name, (group) => ((state.groups = state.groups.filter((one) => one !== group)), drawTabs())),
  "group-close": ([name]) => withGroup(name, (group) => group.paths.map(tabOf).filter(Boolean).forEach(closeTab)),
  "mark-up": ([path]) => {
    const list = markedFiles();
    const at = list.indexOf(path);
    if (at > 0) {
      keepMarks(list.map((one, index) => (index === at - 1 ? path : index === at ? list[at - 1] : one)));
    }
  },
  "mark-let-go": ([path]) => keepMarks(markedFiles().filter((one) => one !== path)),
  "servers-stop-idle": () => holdMemory(Number.MAX_SAFE_INTEGER),
};

// Runs `act` with the tab group named `name`, or says there is none.
function withGroup(name, act) {
  const group = state.groups.find((one) => one.name === name);
  return group ? act(group) : say(`No group of tabs is named ${name}.`, { failed: true });
}

// An item that runs `command` with `args`, as the command line runs it, and says both.
export const act = (command, args, item) => ({ ...item, command, args, run: () => popupCommands[command](args) });

export function fileItems(event) {
  const row = event.target.closest(".node");
  if (!row) {
    return event.target.closest("#files") ? [act("file-paste", [], { label: "Paste", keys: "Ctrl+V", disabled: !state.clipFiles })] : null;
  }
  const path = row.dataset.key;
  const folder = row.classList.contains("dir");
  const open = act("file-open", [path], { label: folder && state.expanded.has(path) ? "Close" : "Open" });
  const mounted = row.classList.contains("mount") ? path.slice(1) : null;
  const names = namesChosen()[path] ?? "";
  return [
    open,
    ...(mounted ? [act("file-remove", [mounted], { label: "Remove from Window" })] : []),
    ...(row.dataset.archive ? [{ label: "Names' Encoding", items: NAME_ENCODINGS.map(([label, shown]) => act("file-names", [path, label], { label: shown, checked: names === label })) }] : []),
    ...(folder ? [] : [{ label: "Open in Next Split", command: "open-in-split", args: [path], run: () => openInSplit(path) }]),
    ...(!folder && SHOWN_IN_WINDOW.has(extOf(path)) ? [act("file-open-window", [path], { label: "Open in a window" })] : []),
    "-",
    act("file-cut", [path], { label: "Cut", keys: "Ctrl+X" }),
    act("file-copy", [path], { label: "Copy", keys: "Ctrl+C" }),
    act("file-paste", [folderOfRow(row)], { label: "Paste", keys: "Ctrl+V", disabled: !state.clipFiles }),
    "-",
    act("file-copy-path", [path], { label: "Copy path" }),
    act("file-terminal", [folderOfRow(row)], { label: "Open in terminal" }),
    ...(folder
      ? []
      : [
          "-",
          act("file-select-compare", [path], { label: "Select for Compare" }),
          act("file-compare", [path], { label: state.compareFrom ? `Compare with ${state.compareFrom.split("/").pop()}` : "Compare with Selected", disabled: !state.compareFrom || state.compareFrom === path }),
        ]),
  ];
}

// The folder a row of the tree stands for, or the one its file is in.
export function folderOfRow(row) {
  const path = row.dataset.key;
  return row.classList.contains("dir") ? path : path.includes("/") ? path.slice(0, path.lastIndexOf("/")) : "";
}

// Files on the system clipboard, cut or copied in the tree here or in another window of orior, or in
// the system's file manager: how many there are, read again as the window or the tree takes the
// focus, Paste being open while there are any.
export async function readClipFiles() {
  state.clipFiles = await invoke("clip_files").catch(() => 0);
}

export async function copyFiles(path, cut) {
  try {
    await invoke("files_copy", { paths: [path], cut });
  } catch (error) {
    say(String(error));
  }
  await readClipFiles();
}

// Pastes the files on the clipboard into a folder of the tree, moving those that were cut, and shows
// the last of them in the tree.
export async function pasteFiles(into) {
  let pasted = [];
  try {
    pasted = await invoke("files_paste", { into });
  } catch (error) {
    say(String(error));
  }
  for (const { from, to } of pasted) {
    if (from !== null && from !== to) {
      moveTabs(from, to);
    }
  }
  await readClipFiles();
  state.expanded.add(into);
  state.children.clear();
  await loadChanges();
  await drawTree();
  const last = pasted.at(-1)?.to;
  if (last !== undefined) {
    treeRowShown(last)?.focus();
  }
}

// The tabs of a file moved, or of the files in a folder moved, show each file where it went: the
// server is told the file closed where it was and opened where it is, and the text, the place and
// any changes not saved go with it.
function moveTabs(from, to) {
  const moved = state.tabs.filter((tab) => !tab.commit && (tab.path === from || tab.path.startsWith(`${from}/`)));
  for (const tab of moved) {
    const was = tab.path;
    const now = to + was.slice(from.length);
    stopServing(tab);
    const kept = localStorage.getItem(backupKey(was));
    forgetBackup(was);
    if (kept !== null) {
      localStorage.setItem(backupKey(now), kept);
    }
    tab.path = now;
    tab.file = now;
    state.used = state.used.map((path) => (path === was ? now : path));
    for (const group of state.groups) {
      group.paths = group.paths.map((path) => (path === was ? now : path));
    }
    if (markedFiles().includes(was)) {
      keepMarks(markedFiles().map((path) => (path === was ? now : path)));
    }
    if (state.active === was) {
      state.active = now;
    }
    serve(tab).then(() => tab.served && state.editor?.s === tab.session && state.editor.schedule());
  }
  if (moved.length) {
    drawTabs();
    drawCrumbs();
  }
}

// A tab's menu, from the tab bar or from Open Editors: close it, the others or all of them, a tab with changes asking first as its ×
// does, or copy where its file is.
export function tabItems(event) {
  return tabMenu(tabOf(event.target.closest(".tab")?.title));
}

export function tabMenu(tab) {
  if (!tab) {
    return null;
  }
  const others = state.tabs.filter((one) => one !== tab);
  return [
    act("tab-close", [tab.path], { label: "Close" }),
    act("tab-close-others", [tab.path], { label: "Close others", disabled: !others.length }),
    { label: "Close all", command: "close-all", args: [], run: () => [...state.tabs].forEach(closeTab) },
    "-",
    act("tab-split-right", [tab.path], { label: "Split Right", disabled: !tab.session || Boolean(tab.commit) }),
    act("tab-split-down", [tab.path], { label: "Split Down", disabled: !tab.session || Boolean(tab.commit) }),
    { label: "Open in Next Split", command: "open-in-split", args: [tab.path], disabled: !splittable(tab), run: () => openInSplit(tab.path) },
    act("tab-new-window", [tab.path], { label: "Open in New Window", disabled: Boolean(tab.commit) }),
    "-",
    act("tab-group", [tab.path], { label: "Add to Group…" }),
    act("tab-ungroup", [tab.path], { label: "Remove from Group", disabled: !groupOf(tab.path) }),
    act("tab-mark", [tab.file], { label: markedFiles().includes(tab.file) ? "Unmark File" : "Mark File", disabled: Boolean(tab.commit) }),
    "-",
    act("file-copy-path", [tab.file], { label: "Copy path" }),
    ...(tab.notebook || (tab.session && (isNotebook(tab.path) || /^\s*(#|\/\/|%|--) %%/m.test(tab.session.doc.text())))
      ? ["-", act("tab-open-as", [tab.path, tab.notebook ? "text" : "notebook"], { label: tab.notebook ? "Open as Text" : "Open as Notebook", disabled: Boolean(tab.commit) })]
      : []),
  ];
}

// Opens a tab's file again as a notebook of cells, or as its text, in the tab's place; a tab with
// changes not saved is saved first.
async function reopenAs(tab, as) {
  if (dirty(tab)) {
    say("Save the file before it opens another way", { failed: true });
    return;
  }
  const at = state.tabs.indexOf(tab);
  closeTab(tab);
  await load(tab.path, as);
  const opened = tabOf(tab.path);
  if (opened) {
    state.tabs.splice(state.tabs.indexOf(opened), 1);
    state.tabs.splice(Math.max(0, at), 0, opened);
  }
  show(tab.path);
}

// The editor's menu: the clipboard, choosing all, find, go to a line, and save, each the same as its
// keys. Cut and copy with nothing chosen take the cursor's whole line, as the keys do.
export function editorItems() {
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
    { command: "definition", args: [], label: "Go to Definition", keys: "F12", disabled: !served, run: () => goToDefinition(editor.head()) },
    { command: "usages", args: [], label: "Find Usages", keys: "Shift+F12", run: findUsages },
    { command: "rename", args: [], label: "Rename Symbol…", keys: "F2", disabled: !served, run: renameSymbol },
    { command: "quick-fix", args: [], label: "Quick Fix…", keys: "Ctrl+.", disabled: !served && !tabOf(state.active)?.inspected, run: quickFix },
    {
      label: "Refactor",
      items: [
        { command: "extract-variable", args: [], label: "Extract Variable", keys: "Ctrl+Alt+V", run: () => extractVariable(editor, fileOf(state.active)) },
        { command: "extract-constant", args: [], label: "Extract Constant", keys: "Ctrl+Alt+C", run: () => extractConstant(editor) },
        { command: "extract-function", args: [], label: "Extract Function", keys: "Ctrl+Alt+M", run: () => extractFunction(editor, tabOfSession(editor.s)) },
        { command: "inline-variable", args: [], label: "Inline Variable", keys: "Ctrl+Alt+N", run: () => inlineVariable(editor) },
      ],
    },
    "-",
    { command: "cut", args: [], label: "Cut", keys: "Ctrl+X", run: held("cut") },
    { command: "copy", args: [], label: "Copy", keys: "Ctrl+C", run: held("copy") },
    { command: "paste", args: [], label: "Paste", keys: "Ctrl+V", run: pasted },
    "-",
    { command: "select-all", args: [], label: "Select all", keys: "Ctrl+A", run: () => editor.selectAll() },
    { command: "find", args: [], label: "Find", keys: "Ctrl+F", run: () => editor.find.open(false) },
    { command: "replace", args: [], label: "Replace", keys: "Ctrl+H", run: () => editor.find.open(true) },
    { command: "line", args: [], label: "Go to line", keys: "Ctrl+G", run: () => editor.goto.open() },
    "-",
    { command: "save", args: [], label: "Save", keys: "Ctrl+S", disabled: !dirty(tabOf(state.active)), run: saveActive },
  ];
}

// Gives the editor the keys of the menus' commands for it, each as `{ keys, run }`, now or as it is
// made.
export function bindEditorKeys(list) {
  state.editorKeys = list;
  state.editor?.addKeys(list);
}

// Vim's keys in the editor and its split, set on or off in Preferences and kept under orior.vim.
const VIM_KEY = "orior.vim";

export function vimKeys() {
  return localStorage.getItem(VIM_KEY) === "true";
}

export function setVimKeys(on) {
  localStorage.setItem(VIM_KEY, String(on));
  state.editor?.setVim(on);
  state.split?.editor.setVim(on);
}

// The line down the editor at the width the file's formatter keeps lines to, which names the
// formatter and the width at its top: off until View, Formatter's Line Width turns it on, and kept
// under orior.margin.
const MARGIN_KEY = "orior.margin";

export function marginLine() {
  return localStorage.getItem(MARGIN_KEY) === "true";
}

export function setMarginLine(on = !marginLine()) {
  localStorage.setItem(MARGIN_KEY, String(on));
  state.editor?.setMargin(on);
  state.split?.editor.setMargin(on);
}

// The editor and its split as the settings in force give them, once a file changes them: each
// editor's own settings, Vim's keys, the formatter's line, each kind of file's theme, and the
// breadcrumbs.
export function takeSettings() {
  for (const editor of [state.editor, state.split?.editor].filter(Boolean)) {
    editor.takeSettings();
    editor.setVim(vimKeys());
    editor.setMargin(marginLine());
    editor.drawTheme();
  }
  drawCrumbs();
}

// Binds the reader's keys in the editor and its split, in place of those bound before, each `run`
// told the editor its keys were pressed in.
export function bindReaderKeys(list) {
  state.boundKeys = list;
  state.editor?.bindKeys(list);
  state.split?.editor.bindKeys(list);
  notebookEditor()?.bindKeys(list);
}

// What the menu bar does to the editor: the editor where a file of text is open in it, saving one
// file or all of them, and closing one tab or all of them.
// A session of the file at `path` for a view over the editor: a twin of its tab's where a tab holds
// it whole, live and taking edits as the tab does, and the file as it was read, taking none, where
// none does. A path outside the tree is read where it is.
export async function viewSession(path) {
  const tab = tabOf(path);
  if (tab?.session && !tab.session.window && !tab.notebook) {
    return tab.session.twin();
  }
  const outside = /^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(path);
  const text = outside ? await invoke("file_read_any", { path }) : (await invoke("file_read", { path })).text;
  return new Session(text ?? "", state.known.languageOf(path), { readOnly: true });
}

// An editor in `host` for a view over the editor, its edits told as a tab's are.
export function viewEditor(host) {
  const { onEdit, pasteAs, spansOf, onShown, onChange } = state.editorHooks;
  return new Editor(host, { onEdit, pasteAs, spansOf, onShown, onChange: (s) => onChange(s.of ?? s), statusHost: document.createElement("div") });
}
