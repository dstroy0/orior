// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { drawOpenEditors } from "../explorer.js";
import { releasePaged } from "../paged.js";
import { stopServing } from "../servers.js";
import { moved } from "../intel.js";
import { showMenu } from "../menu.js";
import { closeNotebook, notebookDirty } from "../notebook.js";
import { tabLabelOf, tell } from "../scripts.js";
import { say } from "../statusbar.js";
import { openWindow } from "../windows.js";
import { element, show, state } from "../edit.js";
import { letGo } from "./held.js";
import { act } from "./menus.js";
import { drawSplitTabs, leaveSplit, openFile } from "./open.js";
import { forgetBackup, keepBackups, keepSession, readKept, treeKey } from "./places.js";

// The tabs.

export function tabOf(path) {
  return state.tabs.find((tab) => tab.path === path);
}

// The tab whose file a session shows, a split's session among them, and not a commit's.
export function tabOfSession(s) {
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
export function markSaved(tab, text) {
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
export function dirty(tab) {
  if (tab.notebook) {
    return notebookDirty(tab.notebook);
  }
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
export const fileOf = (path) => tabOf(path)?.file ?? path;

// A tab's name: the file's, and for a file as a commit left it, the commit's short id after.
export function tabName(tab) {
  const name = tab.file.split("/").pop();
  return tab.commit ? `${name} @ ${tab.commit.slice(0, 7)}` : name;
}

// The tabs of the first side: those not moved to the split alone.
export const mainTabs = () => state.tabs.filter((tab) => tab.inMain !== false);

// The file the reader acts on: the split's where its editor was pressed in last, else the first
// side's.
export const actingPath = () => (state.split?.focused ? state.split.active : state.active);

export function drawTabs() {
  keepSession();
  drawSplitTabs();
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
  const marks = markedFiles();
  const tabButton = (tab) => {
    const name = tabName(tab);
    const close = element("span", { className: "close", textContent: tab.closing ? "×?" : "×", title: tab.path });
    const button = element("button", { className: `tab${groupOf(tab.path) ? " grouped" : ""}${tab.preview ? " preview" : ""}`, type: "button", title: tab.path, role: "tab" });
    const label = tab.commit ? "" : tabLabelOf(tab.file);
    button.setAttribute("aria-selected", String(tab.path === state.active));
    const mark = tab.commit ? -1 : marks.indexOf(tab.file);
    button.append(mark >= 0 ? element("span", { className: "tab-mark", textContent: String(mark + 1), title: `Ctrl+${mark + 1}` }) : "", dirty(tab) ? element("span", { className: "dirty", textContent: "●" }) : "", name, label ? element("span", { className: "tab-label", textContent: label }) : "", close);
    button.addEventListener("click", (event) => {
      if (event.target === close) {
        closeTab(tab);
      } else {
        show(tab.path);
      }
    });
    button.addEventListener("auxclick", (event) => event.button === 1 && closeTab(tab));
    button.addEventListener("dblclick", () => {
      if (tab.preview) {
        tab.preview = false;
        drawTabs();
      }
    });
    dragOut(button, tab);
    return button;
  };
  const drawn = [];
  // Down the side, the marked files stand first, each by its key.
  if (sideTabs() && marks.length) {
    drawn.push(element("div", { className: "tab-section", textContent: "Marked" }));
    marks.forEach((path, at) => {
      const row = element("button", { className: "tab tab-marked", type: "button", title: `${path} (Ctrl+${at + 1})` }, element("span", { className: "tab-mark", textContent: String(at + 1) }), path.split("/").pop());
      row.setAttribute("aria-selected", String(fileOf(state.active) === path));
      row.addEventListener("click", () => goMark(at + 1));
      drawn.push(row);
    });
    drawn.push(element("div", { className: "tab-section", textContent: "Open" }));
  }
  // Each group under its header, which folds it; a folded group still shows its tab that is shown.
  for (const group of state.groups) {
    const members = group.paths.map(tabOf).filter((tab) => tab && tab.inMain !== false);
    if (!members.length) {
      continue;
    }
    const header = element(
      "button",
      { className: `tab-group${group.folded ? " folded" : ""}`, type: "button", title: group.folded ? `Unfold ${group.name}` : `Fold ${group.name}` },
      element("span", { className: "tab-group-fold", textContent: group.folded ? "▸" : "▾" }),
      element("span", { className: "tab-group-name", textContent: group.name }),
      element("span", { className: "tab-group-count", textContent: String(members.length) }),
    );
    header.setAttribute("aria-expanded", String(!group.folded));
    header.addEventListener("click", () => {
      group.folded = !group.folded;
      drawTabs();
    });
    header.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      event.stopPropagation();
      showMenu(event.clientX, event.clientY, groupMenu(group));
    });
    drawn.push(header, ...members.filter((tab) => !group.folded || tab.path === state.active).map(tabButton));
  }
  drawn.push(...mainTabs().filter((tab) => !groupOf(tab.path)).map(tabButton));
  bar.replaceChildren(...drawn);
  rowOverflowing(bar);
  // the tab shown stands in sight in a strip of more tabs than it holds
  bar.querySelector('.tab[aria-selected="true"]')?.scrollIntoView({ block: "nearest", inline: "nearest" });
}

// The row marked while it holds more tabs than it shows, which hangs its scrollbar below the tabs.
export function rowOverflowing(bar = document.getElementById("tabs")) {
  bar.toggleAttribute("data-overflowing", !sideTabs() && bar.scrollWidth > bar.clientWidth + 1);
}

// Tabs down the side: the first side's tabs in a column beside its editor in place of a row above
// it, set in View and kept.
const SIDE_TABS = "orior.tabs.side";

export const sideTabs = () => localStorage.getItem(SIDE_TABS) === "true";

export function setSideTabs(on = !sideTabs()) {
  localStorage.setItem(SIDE_TABS, String(on));
  document.querySelector("#mode-edit .desk").dataset.tabs = on ? "side" : "row";
  drawTabs();
}

// Groups of tabs: a tab's menu puts it in a group named by the reader, and the group's header folds
// it, renames it, lets its tabs go or closes them.

export const groupOf = (path) => state.groups.find((group) => group.paths.includes(path)) ?? null;

export async function addToGroup(tab, given = null) {
  const { askFor } = await import("../menubar.js");
  const name = (given ?? (await askFor("The group's name", groupOf(tab.path)?.name ?? state.groups.at(-1)?.name ?? "")))?.trim();
  if (!name) {
    return;
  }
  leaveGroup(tab.path);
  const group = state.groups.find((one) => one.name === name);
  if (group) {
    group.paths.push(tab.path);
  } else {
    state.groups.push({ name, paths: [tab.path], folded: false });
  }
  drawTabs();
}

export function leaveGroup(path) {
  for (const group of state.groups) {
    group.paths = group.paths.filter((one) => one !== path);
  }
  state.groups = state.groups.filter((group) => group.paths.length);
}

function groupMenu(group) {
  return [
    act("group-fold", [group.name, group.folded ? "off" : "on"], { label: group.folded ? "Unfold" : "Fold" }),
    act("group-rename", [group.name], { label: "Rename Group…" }),
    act("group-ungroup", [group.name], { label: "Ungroup" }),
    "-",
    act("group-close", [group.name], { label: "Close the Group's Tabs" }),
  ];
}

// Marked files: a short list of the tree's files, nine at most, each opened by Ctrl and its number,
// kept for the tree.

const marksKey = () => `orior.marks.${treeKey()}`;

export function markedFiles() {
  const kept = readKept(marksKey(), []);
  return Array.isArray(kept) ? kept.filter((path) => typeof path === "string").slice(0, 9) : [];
}

export function keepMarks(list) {
  localStorage.setItem(marksKey(), JSON.stringify(list.slice(0, 9)));
  drawTabs();
}

// Go, Marked Files, Mark File: the file shown marked for the next key free, or unmarked where it was.
export function toggleMark(path = actingPath() ? fileOf(actingPath()) : null) {
  if (!path || tabOf(path)?.commit) {
    return;
  }
  const list = markedFiles();
  if (list.includes(path)) {
    keepMarks(list.filter((one) => one !== path));
    say(`${path} is no longer marked.`);
    return;
  }
  if (list.length >= 9) {
    say("Nine files are marked: Marked Files lets one go first.", { failed: true });
    return;
  }
  keepMarks([...list, path]);
  say(`${path} is marked: Ctrl+${list.length + 1} opens it.`);
}

// Ctrl and a number: the file marked for it, opened.
export async function goMark(n) {
  const path = markedFiles()[n - 1];
  if (!path) {
    say(`No file is marked for Ctrl+${n}.`);
    return;
  }
  await openFile(path);
}

// Go, Marked Files: each marked file, opened by a press, and its menu to let it go or move it.
export function markItems() {
  const list = markedFiles();
  if (!list.length) {
    return [{ label: "No file is marked: Mark File marks the one shown", disabled: true }];
  }
  return list.map((path, at) => ({
    label: `${at + 1}  ${path}`,
    items: [
      { label: "Open", command: at < 9 ? `mark-${at + 1}` : "file-open", args: at < 9 ? [] : [path], run: () => goMark(at + 1) },
      act("mark-up", [path], { label: "Move Up", disabled: at === 0 }),
      act("mark-let-go", [path], { label: "Let Go" }),
    ],
  }));
}

// A tab dragged and let go outside the window opens its file in a new window, and leaves this one.
function dragOut(button, tab) {
  button.addEventListener("pointerdown", (event) => {
    if (event.button !== 0 || tab.commit) {
      return;
    }
    const from = { x: event.clientX, y: event.clientY };
    let dragging = false;
    const move = (moved) => {
      if (!dragging && Math.hypot(moved.clientX - from.x, moved.clientY - from.y) > 8) {
        dragging = true;
        // The pointer let go outside the window is heard where the tab holds it.
        try {
          button.setPointerCapture(event.pointerId);
        } catch {
          // A pointer that is no longer down has nothing to hold.
        }
        button.classList.add("dragged");
      }
    };
    const up = (released) => {
      button.removeEventListener("pointermove", move);
      button.classList.remove("dragged");
      const outside = released.clientX < 0 || released.clientY < 0 || released.clientX > window.innerWidth || released.clientY > window.innerHeight;
      if (dragging && outside) {
        tabToNewWindow(tab);
      }
    };
    button.addEventListener("pointermove", move);
    button.addEventListener("pointerup", up, { once: true });
  });
}

// Opens a tab's file in a new window, with its changes not saved, and takes the tab out of this one.
export async function tabToNewWindow(tab) {
  keepBackups();
  try {
    await openWindow(["file", "open-file", tab.file]);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  // The text kept for its changes stays for the new window to read.
  tab.leaving = true;
  tab.closing = true;
  closeTab(tab);
}

// A tab with changes not yet saved asks for a second click before it closes. A file the split also
// shows leaves the first side and stays open there.
export function closeTab(tab) {
  if (state.split?.paths.includes(tab.path) && tab.inMain !== false) {
    tab.inMain = false;
    if (state.active === tab.path) {
      show(state.used.find((path) => path !== tab.path && tabOf(path)?.inMain !== false) ?? mainTabs().at(-1)?.path ?? null);
    } else {
      drawTabs();
    }
    return;
  }
  if (dirty(tab) && !tab.closing) {
    tab.closing = true;
    drawTabs();
    return;
  }
  state.tabs = state.tabs.filter((one) => one !== tab);
  state.used = state.used.filter((path) => path !== tab.path);
  leaveGroup(tab.path);
  leaveSplit(tab.path);
  stopServing(tab);
  tell("close", { path: tab.file });
  letGo(tab.session?.doc);
  releasePaged(tab.session);
  if (tab.notebook) {
    closeNotebook(tab.notebook);
  }
  if (!tab.leaving) {
    forgetBackup(tab.path);
  }
  if (state.active === tab.path) {
    state.active = state.used.find((path) => tabOf(path)?.inMain !== false) ?? mainTabs().at(-1)?.path ?? null;
    show(state.active);
  } else {
    drawTabs();
  }
}

