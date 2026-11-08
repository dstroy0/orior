// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { Session } from "../editor/session.js";
import { Editor } from "../editor/view.js";
import { drawOutline, drawUndo, lightOutline } from "../explorer.js";
import { follow as followPaged, readPaged } from "../paged.js";
import { changed, hintsShown, pairOf, parseShown, spansOf } from "../servers.js";
import { moved, typed } from "../intel.js";
import { breakpointMenu, breakpointsOf, pausedLineOf, toggleBreakpoint, valueAt } from "../debug.js";
import { bookmarksOf, startBookmarks } from "../bookmarks.js";
import { startRuns } from "../runs.js";
import { startTrace } from "../trace.js";
import { coverageOf, startTests, testMenu, testsOf } from "../tests.js";
import { showMenu } from "../menu.js";
import { cellFileOf, notebookEditor, startNotebooks } from "../notebook.js";
import { kindColors: typeColors } from "../themes.js";
import { onFonts } from "../fonts.js";
import { writeWithCare } from "../rights.js";
import { marksOf, startScripts, syncPlugins, tell } from "../scripts.js";
import { say } from "../statusbar.js";
import { cursorListeners, editing, editListeners, onCursorMoved, onEdit, onShown, pasteListeners, show, state } from "../edit.js";
import { drawCrumbs, goToDefinition, problemsChanged } from "./crumbs.js";
import { light } from "./defs.js";
import { markChanges, mirror, openLineCommit } from "./mirror.js";
import { marginLine, vimKeys } from "./menus.js";
import { keepPlace, splitFocused } from "./open.js";
import { closePeek, hunkAt, showPeek } from "./peek.js";
import { endCycle, keepBackups } from "./places.js";
import { undoGroup } from "./refactor.js";
import { AUTO_SAVE_REST, openAt, saving } from "./save.js";
import { drawTabs, markSaved, tabOf, tabOfSession } from "./tabs.js";

export const cursorLine = () => (state.editor?.s ? state.editor.head().line : 0);

const fileOfSession = (s) => state.tabs.find((tab) => tab.session === (s?.of ?? s) && !tab.commit)?.file ?? null;

export function hookEditor() {
  let lighting = 0;
  let keeping = 0;
  let outlining = 0;
  let backing = 0;
  let marking = 0;
  let autoSaving = 0;
  window.addEventListener("keyup", (event) => event.key === "Control" && endCycle());
  window.addEventListener("blur", endCycle);
  onFonts(() => state.editor?.restyle());
  state.editorHooks = {
    onChangeMark: (line) => (state.peek && hunkAt(state.editor.s, line) && state.peek.dataset.line === String(line) ? closePeek() : showPeek(line)),
    onHistory: openLineCommit,
    onGroup: undoGroup,
    onShown: (s, from, to) => {
      s.highlight.shown(from, to, s.view?.reach?.() ?? Infinity);
      const tab = tabOfSession(s.of ?? s);
      if (tab) {
        hintsShown(tab, from, to);
        parseShown(tab, from, to);
      }
    },
    spansOf: (s, from, to) => {
      const tab = tabOfSession(s.of ?? s);
      return tab ? spansOf(tab, from, to) : Promise.resolve([]);
    },
    pairOf: (s, head) => {
      const tab = tabOfSession(s.of ?? s);
      return tab ? pairOf(tab, head) : Promise.resolve(null);
    },
    onEdit: (s) => {
      // The tab's server hears of the change once the frame that draws it is drawn, and not once the
      // keys rest: its marks come with no wait of the window's own (servers.js).
      const tab = state.tabs.find((one) => one.session === (s.of ?? s));
      if (tab) {
        changed(tab);
      }
      editListeners.forEach((listener) => listener(s.of ?? s));
    },
    pasteAs: (s, text) => {
      const path = tabOfSession(s.of ?? s)?.file ?? null;
      for (const listener of pasteListeners) {
        const given = listener(path, text);
        if (given) {
          return given;
        }
      }
      return null;
    },
    onCursor: () => {
      moved();
      cursorListeners.forEach((listener) => listener());
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
        tab.preview = false;
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
      drawUndo(tabOf(state.active)?.session ?? null);
    },
  };
}

export function startEditor() {
  state.editor = new Editor(document.getElementById("editor"), { ...state.editorHooks, statusHost: document.getElementById("statusbar") });
  // A paged file's window moves with the view, looked at once a frame while the view scrolls.
  let following = 0;
  state.editor.scroller.addEventListener("scroll", () => {
    if (!following && state.editor.s?.window?.paged) {
      following = requestAnimationFrame(() => {
        following = 0;
        followPaged(state.editor.s, state.editor);
      });
    }
  });
  startNotebooks({
    languageOf: (path) => state.known.languageOf(path),
    changed: () => drawTabs(),
    menu: (items, event) => showMenu(event.clientX, event.clientY, items),
  });
  notebookEditor().bindKeys(state.boundKeys ?? []);
  // A Python cell's breakpoints, its line stopped on and the values under the pointer, by the file
  // the cell is debugged as.
  const cells = notebookEditor();
  cells.breakpointsOf = (s) => {
    const file = cellFileOf(s);
    return file ? breakpointsOf(file) : null;
  };
  cells.pausedOf = (s) => {
    const file = cellFileOf(s);
    return file ? pausedLineOf(file) : null;
  };
  cells.valueAt = (s, p) => {
    const file = cellFileOf(s);
    return file ? valueAt(file, s.doc, { line: p.line, col: p.col }) : null;
  };
  cells.onBreakpoint = (line) => {
    const file = cellFileOf(cells.s);
    if (file) {
      toggleBreakpoint(file, line);
    }
  };
  cells.onBreakpointMenu = (line, event) => {
    const file = cellFileOf(cells.s);
    if (file) {
      breakpointMenu(file, line, event.clientX, event.clientY);
    }
  };
  state.editor.input.addEventListener("focus", () => splitFocused(false));
  state.editor.onDefinition = (p) => goToDefinition(p);
  state.editor.themeOf = (s) => typeColors(s?.language?.id);
  state.editor.addKeys(state.editorKeys);
  state.editor.bindKeys(state.boundKeys);
  state.editor.setVim(vimKeys());
  state.editor.setMargin(marginLine());
  // The debugger's marks in the gutter, by the file a session shows, a split's the same as the
  // session it was made from. A file as a commit left it has none.
  state.editor.breakpointsOf = (s) => {
    const file = fileOfSession(s);
    return file ? breakpointsOf(file) : null;
  };
  state.editor.bookmarksOf = (s) => {
    const file = fileOfSession(s);
    return file ? bookmarksOf(file) : null;
  };
  // The breakpoint strip's menu, and the value of the name under the pointer while a program is
  // stopped.
  state.editor.onBreakpointMenu = (line, event) => {
    const file = fileOfSession(state.editor.s);
    if (file) {
      breakpointMenu(file, line, event.clientX, event.clientY);
    }
  };
  state.editor.valueAt = (s, p) => {
    const file = fileOfSession(s);
    return file ? valueAt(file, s.doc, { line: p.line, col: p.col }) : null;
  };
  // Each test's mark in the gutter, and the lines the last run with coverage ran, by the file a
  // session shows; a press on a mark opens the test's menu.
  state.editor.testsOf = (s) => {
    const file = fileOfSession(s);
    return file ? testsOf(file) : null;
  };
  state.editor.coverageOf = (s) => {
    const file = fileOfSession(s);
    return file ? coverageOf(file) : null;
  };
  state.editor.onTestMark = (line, event) => {
    const file = fileOfSession(state.editor.s);
    if (file) {
      testMenu(file, line, event.clientX, event.clientY);
    }
  };
  startRuns();
  startTrace();
  startTests({
    openAt: (path, line, col) => openAt(path, line, col),
    saveAll: () => editing().saveAll({ auto: true }),
    repaint: () => [state.editor, state.split?.editor].forEach((one) => one?.schedule()),
  });
  state.editor.pluginMarksOf = (s) => {
    const file = fileOfSession(s);
    return file ? marksOf(file) : null;
  };
  startScripts({
    repaint: () => [state.editor, state.split?.editor].forEach((one) => one?.schedule()),
    tabsChanged: () => drawTabs(),
    active: () => (state.active ? tabOf(state.active)?.file ?? null : null),
    editor: () => state.editor,
    textOf: async (path) => {
      const s = tabOf(path)?.session;
      if (s?.window?.paged) {
        throw new Error(`${path} is stored as pages: orior.editor.lines reads a part of it`);
      }
      return s?.doc.text() ?? (await invoke("file_read", { path })).text;
    },
    // A part of a file's lines: a paged file's from the tree's side, and any other's as its tab holds
    // it or its file does.
    linesOf: async (path, from, to) => {
      const s = tabOf(path)?.session;
      if (s?.window?.paged) {
        return readPaged(s, from, to);
      }
      const lines = s ? s.doc.lines : ((await invoke("file_read", { path })).text ?? "").split(/\r?\n/);
      return lines.slice(from - (s?.base ?? 0), to - (s?.base ?? 0));
    },
    write: (one, path, text) => writeWithCare(path, text, { who: one.name }).then(() => null),
    open: (path, line, col) => openAt(path, line, col),
  });
  syncPlugins();
  onShown((path) => path && tell("open", { path }));
  onEdit((s) => {
    const tab = tabOfSession(s);
    if (tab && !tab.commit) {
      window.clearTimeout(state.telling);
      state.telling = window.setTimeout(() => tell("edit", { path: tab.file }), 150);
    }
  });
  onCursorMoved(() => {
    const tab = tabOf(state.active);
    const head = state.editor?.s ? state.editor.head() : null;
    if (tab?.session && head && !tab.commit) {
      tell("cursor", { path: tab.file, line: tab.session.base + head.line, col: head.col });
    }
  });
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
    // A text shown in its colors goes back to its codes as written, to be edited.
    if (tab.colored !== undefined) {
      const raw = tab.colored;
      delete tab.colored;
      tab.readOnly = false;
      tab.session = new Session(raw, state.known.languageOf(tab.path));
      // The window's side holds the file's text as the tab opened it; it colors it again, the names first.
      mirror(tab.session, tab.key);
      invoke("branch_language", { key: tab.key, id: tab.session.language?.id ?? "" }).catch(() => {});
      markSaved(tab, raw);
      show(tab.path);
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
    editor.bindKeys(state.boundKeys);
    editor.setVim(vimKeys());
    editor.setMargin(marginLine());
    for (const name of ["breakpointsOf", "bookmarksOf", "testsOf", "coverageOf", "pausedOf", "onLock", "valueAt", "themeOf"]) {
      editor[name] = state.editor[name];
    }
    editor.onBreakpoint = (line) => {
      const file = fileOfSession(editor.s);
      if (file) {
        toggleBreakpoint(file, line);
      }
    };
    editor.onBreakpointMenu = (line, event) => {
      const file = fileOfSession(editor.s);
      if (file) {
        breakpointMenu(file, line, event.clientX, event.clientY);
      }
    };
  };
}
