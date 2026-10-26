// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { Session } from "../editor/session.js";
import { Editor } from "../editor/view.js";
import { openPaged } from "../paged.js";
import { serve } from "../servers.js";
import { coloredLines } from "../screen.js";
import { isNotebook, openNotebook } from "../notebook.js";
import { say } from "../statusbar.js";
import { element, show, state } from "../edit.js";
import { hold } from "./held.js";
import { backupKey, keepRecent } from "./places.js";
import { actingPath, closeTab, dirty, drawTabs, mainTabs, markSaved, tabName, tabOf } from "./tabs.js";

// A file too large to read whole opens paged (paged.js): the CLI holds it, and the page holds a
// window of its lines about the view, slid as the view moves.

export const placeKey = (path) => `orior.place.${path}`;

function placeOf(path) {
  try {
    return JSON.parse(localStorage.getItem(placeKey(path)) ?? "null");
  } catch {
    return null;
  }
}

export function keepPlace() {
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

// Loads a file's tab, or says on the status bar why its file could not be read, as for a file gone
// from the disk since its tab was kept. Answers whether the tab is there.
async function loaded(path) {
  try {
    await load(path);
  } catch (error) {
    say(String(error), { failed: true });
  }
  return Boolean(tabOf(path));
}

// Opens the file at `path` in a tab and shows it. Where `preview`, a tab it opens is a preview,
// its name in italics, until it is edited or its tab pressed twice, and the next preview opened
// takes its place; a file opened any other way is a preview no longer.
export async function openFile(path, { preview = false } = {}) {
  if (state.elsewhere) {
    state.elsewhere(path, null, 0);
    return;
  }
  const fresh = !tabOf(path);
  if (!(await loaded(path))) {
    return;
  }
  const opened = tabOf(path);
  if (preview && fresh) {
    state.tabs.filter((one) => one.preview && one !== opened && !dirty(one)).forEach((one) => closeTab(one));
    opened.preview = true;
  } else if (!preview) {
    opened.preview = false;
  }
  if (state.split?.focused && splittable(tabOf(path))) {
    // A file opened in the split alone is not one of the first side's tabs.
    if (fresh) {
      tabOf(path).inMain = false;
    }
    showInSplit(path);
  } else {
    show(path);
  }
}

// Open in Next Split: a file opened on the side not last pressed in, a split to the right made
// where there is none.
export async function openInSplit(path) {
  const fresh = !tabOf(path);
  if (!(await loaded(path))) {
    return;
  }
  if (state.split?.focused || !splittable(tabOf(path))) {
    show(path);
    return;
  }
  if (!state.split) {
    makeSplit("right");
  }
  if (fresh) {
    tabOf(path).inMain = false;
  }
  showInSplit(path);
}

// Opens a file's tab without showing it, with the text kept for it where it had changes not saved.
export async function load(path, as = null) {
  if (!tabOf(path) && (as === "notebook" || (as === null && isNotebook(path)))) {
    const tab = { path, file: path, size: 0, closing: false };
    tab.notebook = await openNotebook(path);
    state.tabs.push(tab);
    return tab;
  }
  if (!tabOf(path)) {
    const opened = await invoke("file_read", { path });
    const tab = { path, file: path, size: opened.size, closing: false };
    const place = placeOf(path);
    if (opened.windowed) {
      tab.session = await openPaged(path, place?.line ?? 0, state.known.languageOf(path));
      markSaved(tab);
      settleAt(tab.session, place);
    } else if (opened.text !== null && opened.text !== undefined) {
      const kept = localStorage.getItem(backupKey(path));
      // A text with color codes in it, as a log or a run's output keeps, shows in its colors with the
      // codes taken out, read-only, until its lock is pressed.
      const colored = kept === null && opened.text.includes("\x1b[") ? coloredLines(opened.text.split(/\r?\n/)) : null;
      tab.session = new Session(colored ? colored.map((one) => one.text).join("\n") : (kept ?? opened.text), state.known.languageOf(path));
      if (colored) {
        tab.session.colored = colored.map((one) => one.html);
        tab.session.readOnly = true;
        tab.readOnly = true;
        tab.colored = opened.text;
      }
      // Kept text that differs from the file's is a change not saved, and the tab says so.
      markSaved(tab, opened.text);
      if (kept !== null && kept !== opened.text) {
        tab.saved = -1;
      }
      settleAt(tab.session, place);
      hold(tab, !colored && (kept === null || kept === opened.text));
    } else {
      tab.bytes = new Uint8Array(opened.bytes);
    }
    state.tabs.push(tab);
    serve(tab).then(() => tab.served && state.editor?.s === tab.session && state.editor.schedule());
    // The width the file's formatter keeps its lines to, drawn as a line down the editor.
    if (tab.session) {
      invoke("format_margin", { path: tab.path, language: tab.session.language?.id ?? "" })
        .then((found) => {
          tab.session.margin = found?.width ?? null;
          tab.session.marginBy = found?.by ?? null;
          state.editor?.schedule();
        })
        .catch(() => {});
      // The indentation the .editorconfig files over the file set, over what its lines say and
      // under what a mode line of its own sets.
      invoke("indent_for", { path: tab.path })
        .then((set) => {
          const s = tab.session;
          for (const key of ["tabs", "size"]) {
            if (set[key] !== null && s.indentSet[key] === undefined) {
              s.indent[key] = set[key];
            }
          }
          s.foldings += 1;
          if (state.editor?.s === s) {
            state.editor.statusSaid = null;
            state.editor.size();
            state.editor.schedule();
          }
        })
        .catch(() => {});
    }
  }
}

// Opens a file as commit `commit` left it, read only, in a tab of its own.
export async function openCommit(path, commit) {
  const key = `${path}@${commit.id}`;
  if (!tabOf(key)) {
    const text = await invoke("file_at", { path, id: commit.id }).catch((error) => {
      say(String(error), { failed: true });
      return null;
    });
    if (text === null) {
      return;
    }
    const tab = { path: key, file: path, commit: commit.id, readOnly: true, size: text.length, closing: false };
    tab.session = new Session(text, state.known.languageOf(path), { readOnly: true });
    tab.saved = tab.session.doc.id;
    state.tabs.push(tab);
  }
  show(key);
}

// Split: a second side beside the first or under it, with tabs of its own. Any file can be in
// either side or both, a file in both showing the same text with the selections, folds and place
// each its own. The menus act on the side last pressed in, and a file opened opens there. The split
// closes with Unsplit or its own close button, its files going back to the first side, or as its
// last tab closes.

// A file of the tree, open and read whole, can be shown in the split.
export const splittable = (tab) => Boolean(tab?.session && !tab.session.window && !tab.commit);

export function makeSplit(direction) {
  const desk = document.querySelector("#mode-edit .desk");
  const host = element("div", { className: "editor split-editor" });
  const close = element("button", { className: "split-close", type: "button", title: "Unsplit" }, "×");
  close.setAttribute("aria-label", "Unsplit");
  close.addEventListener("click", unsplit);
  const strip = element("div", { className: "tabs split-tabs", role: "tablist" });
  strip.setAttribute("aria-label", "The split's tabs");
  const node = element("section", { className: "split" }, element("div", { className: "split-head" }, strip, close), host);
  node.setAttribute("aria-label", "Split");
  desk.append(node);
  desk.dataset.split = direction;
  // Its status line stands in the status bar beside the first one's, the one shown being that of the
  // editor last pressed in.
  const editor = new Editor(host, { ...state.editorHooks, statusHost: document.getElementById("statusbar"), onChange: (s) => state.editorHooks.onChange(s.of ?? s) });
  state.editor.status.after(editor.status);
  state.lendTo(editor);
  state.split = { node, editor, strip, direction, paths: [], active: null, twins: new Map(), focused: true };
  editor.input.addEventListener("focus", () => splitFocused(true));
  splitFocused(true);
}

// Split Right and Split Down: the file shown on the first side shown in a split beside it or under
// it as well.
export function splitEditor(direction) {
  const tab = tabOf(state.active);
  if (!splittable(tab)) {
    say("Split shows a file of the tree that is open and read whole.");
    return;
  }
  unsplit();
  makeSplit(direction);
  showInSplit(tab.path);
}

// Shows the file at `path`, open already, in the split, in a tab of the split's own.
export function showInSplit(path) {
  const split = state.split;
  const tab = tabOf(path);
  if (!split || !splittable(tab)) {
    return;
  }
  if (!split.paths.includes(path)) {
    split.paths.push(path);
  }
  if (!split.twins.has(path)) {
    split.twins.set(path, tab.session.twin());
  }
  split.active = path;
  split.editor.show(split.twins.get(path));
  split.editor.focus();
  splitFocused(true);
  keepRecent(tab.file);
  drawTabs();
}

export function drawSplitTabs() {
  const split = state.split;
  if (!split) {
    return;
  }
  split.strip.replaceChildren(
    ...split.paths.map((path) => {
      const tab = tabOf(path);
      const close = element("span", { className: "close", textContent: tab?.closing && tab.inMain === false ? "×?" : "×", title: path });
      const button = element("button", { className: "tab", type: "button", title: path, role: "tab" });
      button.setAttribute("aria-selected", String(path === split.active));
      button.append(tab && dirty(tab) ? element("span", { className: "dirty", textContent: "●" }) : "", tab ? tabName(tab) : path, close);
      button.addEventListener("click", (event) => (event.target === close ? closeSplitTab(path) : showInSplit(path)));
      button.addEventListener("auxclick", (event) => event.button === 1 && closeSplitTab(path));
      return button;
    })
  );
}

// Takes the file at `path` out of the split, which shows its file used last or, with none left,
// closes.
export function leaveSplit(path) {
  const split = state.split;
  if (!split?.paths.includes(path)) {
    return;
  }
  split.paths = split.paths.filter((one) => one !== path);
  split.twins.get(path)?.drop();
  split.twins.delete(path);
  if (!split.paths.length) {
    unsplit();
  } else if (split.active === path) {
    showInSplit(split.used?.find((one) => split.paths.includes(one)) ?? split.paths.at(-1));
  } else {
    drawSplitTabs();
  }
}

// Closes a tab of the split. A file open on neither side after it closes, as its tab on the first
// side would, asking first where it has changes not saved.
export function closeSplitTab(path) {
  const tab = tabOf(path);
  if (tab?.inMain === false) {
    if (dirty(tab) && !tab.closing) {
      tab.closing = true;
      drawSplitTabs();
      return;
    }
    leaveSplit(path);
    tab.inMain = true;
    closeTab(tab);
    return;
  }
  leaveSplit(path);
}

// View, Move to Next Split: the file shown on the side last pressed in moves to the other side, a
// split to the right made where there is none, and the side it left shows its file used last.
export function moveToNextSplit() {
  const path = actingPath();
  const tab = tabOf(path);
  if (!splittable(tab)) {
    say("A file of the tree, open and read whole, moves to the next split.");
    return;
  }
  if (state.split?.focused) {
    leaveSplit(path);
    show(path);
    return;
  }
  if (!state.split) {
    makeSplit("right");
  }
  tab.inMain = false;
  state.moving = true;
  show(state.used.find((one) => one !== path && tabOf(one)?.inMain !== false) ?? mainTabs().at(-1)?.path ?? null);
  state.moving = false;
  showInSplit(path);
}

// Marks which editor of a split the menus act on, and shows its status line.
export function splitFocused(focused) {
  if (!state.split) {
    return;
  }
  state.split.focused = focused;
  state.split.editor.status.hidden = !focused;
  state.editor.status.hidden = focused;
  if (focused && state.split.active) {
    state.split.used = [state.split.active, ...(state.split.used ?? []).filter((one) => one !== state.split.active)];
  }
}

// Closes the split, its files going back to the first side.
export function unsplit() {
  if (!state.split) {
    return;
  }
  const { node, editor, twins, paths, active } = state.split;
  state.split = null;
  editor.show(null);
  twins.forEach((session) => session.drop());
  for (const path of paths) {
    const tab = tabOf(path);
    if (tab) {
      tab.inMain = true;
    }
  }
  node.remove();
  editor.status.remove();
  state.editor.status.hidden = false;
  delete document.querySelector("#mode-edit .desk").dataset.split;
  if (!state.active && active && tabOf(active)) {
    show(active);
  }
  state.editor.schedule();
  state.editor.focus();
  drawTabs();
}

