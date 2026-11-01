// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, listen } from "../bridge.js";
import { inBridge, keepBridge, loadBridge } from "../bridge_panel.js";
import { drawGit, drawLocalHistory, drawReview, drawTodo, drawOutline, drawProblems, drawTimeline, drawUndo, paneOpen, shownGroup, startExplorer } from "../explorer.js";
import { drawCommit, startCommit } from "../commit.js";
import { changed, startServers, treeChecking } from "../servers.js";
import { startIntel } from "../intel.js";
import { startDebug } from "../debug.js";
import { keepListKeys } from "../lists.js";
import { menuOn } from "../menu.js";
import { onPatterns, tellPatterns } from "../patterns.js";
import { notebookOfCellFile, repaintNotebook, showCellLine } from "../notebook.js";
import { onScheme } from "../scheme.js";
import { iconsKey } from "../themes.js";
import { say } from "../statusbar.js";
import { drawEmpty, editing, show, state } from "../edit.js";
import { compareCommit, openComment, openDiff, openMerge, openTouched, reloadFromDisk, revertSnapshot, showSnapshot, textNow } from "./compare.js";
import { checkShown, problemFiles, problemsChanged } from "./crumbs.js";
import { drawDefs } from "./defs.js";
import { forgetHeads, markChanges } from "./mirror.js";
import { copyFiles, editorItems, fileItems, folderOfRow, pasteFiles, readClipFiles, tabItems, tabMenu } from "./menus.js";
import { openCommit, openFile } from "./open.js";
import { closePeek, PEEK_SETTLES } from "./peek.js";
import { jumpTo } from "./places.js";
import { openAt, saveActive, treeChanged } from "./save.js";
import { cursorLine } from "./start.js";
import { actingPath, closeTab, dirty, drawTabs, fileOf, tabOf } from "./tabs.js";
import { drawTree, loadChanges, repoName, repoOf, repoOpen } from "./tree.js";

export function startPanes() {
  startCommit({
    shown: () => paneOpen("changes") && shownGroup() === "commit",
    saveAll: () => editing().saveAll(),
    reload: (paths) => reloadFromDisk(paths),
    refresh: async () => {
      await loadChanges();
      await drawTree();
    },
    diff: (path, mark) => (mark === "C" ? openMerge(path) : openDiff(path, mark)),
    texts: async (path, mark) => ({ then: mark === "U" || mark === "A" ? "" : ((await invoke("file_head", { path }).catch(() => null)) ?? ""), now: await textNow(path) }),
    open: (path) => openFile(path),
    repo: repoOpen,
    repoOf,
    repoName,
    repos: () => state.repos ?? [],
  });
  startDebug({
    editor: () => state.editor,
    tab: () => tabOf(actingPath()),
    save: async (tab) => {
      if (dirty(tab) && !tab.readOnly) {
        await saveActive(tab);
      }
    },
    open: (path) => openFile(path),
    // A line of a notebook's cell shows in the cell, where the notebook is open.
    openAt: async (path, line, col) => {
      const notebook = notebookOfCellFile(path);
      if (notebook) {
        await openFile(notebook);
        if (showCellLine(path, line, col)) {
          return;
        }
      }
      await openAt(path, line, col);
    },
    repaint: () => {
      [state.editor, state.split?.editor].forEach((one) => one?.schedule());
      repaintNotebook();
    },
    run: (command) => import("../menubar.js").then((menus) => menus.runCommand(command)),
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
  onScheme(() => {
    [state.editor, state.split?.editor].filter(Boolean).forEach((editor) => {
      editor.drawTheme();
      editor.refreshColors();
    });
    // The theme's icons of files, where they changed.
    if (iconsKey() !== state.iconsDrawn) {
      state.iconsDrawn = iconsKey();
      drawTabs();
      drawTree();
    }
  });
  let wait = 0;
  document.getElementById("file-filter").addEventListener("input", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawTree, 180);
  });
  startExplorer({
    show,
    open: (path) => openFile(path),
    sessionOf: (path) => (path ? tabOf(path)?.session ?? null : null),
    activePath: () => (state.active ? tabOf(state.active)?.file ?? null : null),
    close: (path) => tabOf(path) && closeTab(tabOf(path)),
    tabMenu: (path) => tabMenu(tabOf(path)),
    goTo: (line) => jumpTo(line),
    goToState: (id) => (state.split?.focused ? state.split.editor : state.editor)?.goToState(id),
    cursorLine,
    openCommit,
    compareCommit,
    openTouched,
    openComment,
    showChanges: (path) => openDiff(path, state.changes.get(path) ?? "M"),
    repo: repoOpen,
    repoName,
    repos: () => state.repos ?? [],
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
      drawUndo(tabOf(state.active)?.session ?? null);
      drawTimeline(state.active ? fileOf(state.active) : null);
      drawLocalHistory(state.active ? fileOf(state.active) : null);
      drawCommit(state.changes);
      checkShown();
      drawProblems(problemFiles(), treeChecking());
      drawGit(state.branch);
      drawReview();
      drawTodo();
    },
    openAt: (path, line, col) => openAt(path, line, col),
  });
}

export async function bindPage() {
  keepListKeys(document.getElementById("panes"), document.getElementById("file-filter"));
  // A link in a hover, a diagnostic or a document opens in the browser, and the window stays.
  document.addEventListener(
    "click",
    (event) => {
      const link = event.target.closest?.("a[data-link]");
      if (link) {
        event.preventDefault();
        invoke("link_open", { url: link.getAttribute("href") }).catch((error) => say(String(error), { failed: true }));
      }
    },
    true,
  );
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
  const files = document.getElementById("files");
  menuOn(files, fileItems);
  menuOn(document.getElementById("tabs"), tabItems);
  menuOn(document.getElementById("editor"), editorItems);
  // Cut, copy and paste of files by their keys while a row of the tree holds them.
  files.addEventListener("keydown", (event) => {
    const row = event.target.closest(".node");
    const act = { x: () => copyFiles(row.dataset.key, true), c: () => copyFiles(row.dataset.key, false), v: () => pasteFiles(folderOfRow(row)) }[event.key.toLowerCase()];
    if (row && act && (event.ctrlKey || event.metaKey) && !event.altKey && !event.shiftKey) {
      event.preventDefault();
      event.stopPropagation();
      act();
    }
  });
  files.addEventListener("pointerdown", readClipFiles);
  files.addEventListener("focusin", readClipFiles);
  window.addEventListener("focus", async () => {
    readClipFiles();
    forgetHeads();
    state.repos = null;
    markChanges(tabOf(state.active));
    await loadChanges();
    drawTree();
  });
  document.getElementById("defs-side").hidden = true;
  document.getElementById("editor").hidden = true;
  drawEmpty(true);
  loadBridge();
  keepBridge(() => inBridge(state.active) && drawDefs());
  listen("tree-changed", treeChanged);
  // The explorer drawn again as its patterns change.
  onPatterns((part) => {
    if (part === "explorer") {
      state.children.clear();
      drawTree();
    }
  });
  await tellPatterns();
  await loadChanges();
  await drawTree();
}
