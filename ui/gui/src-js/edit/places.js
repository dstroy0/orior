// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { BACKUP_MAX, PLACES, RECENT, show, state } from "../edit.js";
import { load, makeSplit, openFile, showInSplit, splittable } from "./open.js";
import { dirty, mainTabs, tabOf } from "./tabs.js";

// Places: where the cursor is, retained before each jump for Back and Forward.

function here() {
  const tab = tabOf(state.active);
  if (!tab?.session) {
    return tab ? { path: tab.path, line: 0, col: 0 } : null;
  }
  const head = tab.session.selections[tab.session.primary].head;
  return { path: tab.path, line: tab.session.base + head.line, col: head.col };
}

// Retains the place the cursor is at for Back, unless it is the last one retained or next to it.
export function markPlace() {
  const place = here();
  const last = state.back.at(-1);
  if (!place || (last && last.path === place.path && Math.abs(last.line - place.line) < 2)) {
    return;
  }
  state.back.push(place);
  state.back.splice(0, state.back.length - PLACES);
  state.forward = [];
}

// Goes to a line of the file displayed, retention the place it leaves for Back.
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

// Steps back, by -1, or forward, by 1, through the places retained.
export async function step(by) {
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

// Last Editor: each press while Ctrl is pressed steps one tab further back in when each was displayed, and
// letting Ctrl go retains the tab it stopped on as the one displayed last.
export function lastEditor() {
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

export function endCycle() {
  if (!state.cycle) {
    return;
  }
  state.cycle = null;
  if (state.active) {
    state.used = [state.active, ...state.used.filter((one) => one !== state.active)];
  }
}

// What each tree retains: its tabs and the one displayed, the files opened last, and the text of each file
// with changes not saved.

export const treeKey = () => document.getElementById("tree-path").textContent;
const sessionKey = () => `orior.session.${treeKey()}`;
const recentKey = () => `orior.recent.${treeKey()}`;
export const backupKey = (path) => `orior.backup.${treeKey()}\n${path}`;

export function readKept(key, fallback) {
  try {
    return JSON.parse(localStorage.getItem(key) ?? "null") ?? fallback;
  } catch {
    return fallback;
  }
}

export function retainSession() {
  if (!state.restored) {
    return;
  }
  const tabs = state.tabs.filter((tab) => !tab.commit).map((tab) => tab.path);
  const main = mainTabs().filter((tab) => !tab.commit).map((tab) => tab.path);
  const split = state.split ? { paths: state.split.paths, active: state.split.active, direction: state.split.direction } : null;
  const groups = state.groups.map(({ name, paths, folded }) => ({ name, paths, folded }));
  localStorage.setItem(sessionKey(), JSON.stringify({ tabs, main, active: tabOf(state.active)?.commit ? null : state.active, split, groups }));
}

export function retainRecent(path) {
  localStorage.setItem(recentKey(), JSON.stringify([path, ...readKept(recentKey(), []).filter((one) => one !== path)].slice(0, RECENT)));
}

// The files of the tree opened last, the last first.
export function recentFiles() {
  return readKept(recentKey(), []);
}

export function forgetBackup(path) {
  localStorage.removeItem(backupKey(path));
}

// Retains the text of each tab with changes not saved, and lets go of each without.
export function retainBackups() {
  for (const tab of state.tabs) {
    if (!tab.session || tab.readOnly || tab.session.window) {
      continue;
    }
    if (!dirty(tab)) {
      forgetBackup(tab.path);
      continue;
    }
    const text = tab.session.doc.text();
    if (text.length > BACKUP_MAX) {
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
  state.restoring = true;
  try {
    await putSessionBack();
  } finally {
    state.restoring = false;
  }
}

async function putSessionBack() {
  const retained = readKept(sessionKey(), { tabs: [], active: null });
  for (const path of retained.tabs ?? []) {
    await load(path).catch(() => {});
  }
  // The groups of tabs as they were.
  state.groups = (Array.isArray(retained.groups) ? retained.groups : [])
    .filter((group) => typeof group?.name === "string" && Array.isArray(group.paths))
    .map((group) => ({ name: group.name, paths: group.paths.filter((path) => tabOf(path)), folded: Boolean(group.folded) }))
    .filter((group) => group.paths.length);
  state.restored = true;
  // The split as it was, with its tabs, and the tabs of the first side.
  const split = (retained.split?.paths ?? (retained.split?.path ? [retained.split.path] : [])).filter((path) => splittable(tabOf(path)));
  if (split.length && ["right", "down"].includes(retained.split.direction)) {
    makeSplit(retained.split.direction);
    for (const path of split) {
      showInSplit(path);
    }
    showInSplit(split.includes(retained.split.active) ? retained.split.active : split.at(-1));
    for (const tab of state.tabs) {
      if (retained.main && !retained.main.includes(tab.path) && split.includes(tab.path)) {
        tab.inMain = false;
      }
    }
  }
  const displayed = tabOf(retained.active)?.inMain !== false && tabOf(retained.active) ? retained.active : mainTabs().at(-1)?.path;
  if (displayed) {
    state.moving = true;
    show(displayed);
    state.moving = false;
  } else {
    retainSession();
  }
}

