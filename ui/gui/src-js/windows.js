// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// More than one window. Each window is a program of its own on one tree, with its own tabs,
// terminals, servers and debugger. File, New Window opens another on the same tree, and Open Folder
// in New Window one on another; a tab dragged out of the window, or its menu's Open in New Window,
// opens its file in a new one; View, Move Window to Next Display moves the window to the next of the
// machine's displays.
//
// A tool window opened in a window of its own, the terminal, the explorer or the jobs, is a new
// window that shows that tool alone, and a file opened from it opens in the window it came from.
// The windows tell each other what to do through the browser storage they share: a message names
// the window it is for, and the others pass it over.

import { invoke } from "./bridge.js";

const TELL = "orior.tell";

// This window's name among the others.
export const windowId = globalThis.crypto?.randomUUID?.() ?? `${Date.now()}-${Math.random()}`;

const heard = new Map();

window.addEventListener("storage", (event) => {
  if (event.key !== TELL || !event.newValue) {
    return;
  }
  try {
    const told = JSON.parse(event.newValue);
    if (told.to === windowId) {
      heard.get(told.kind)?.(told.body);
    }
  } catch {
    // A message this window cannot read is for no window it knows.
  }
});

// What a message of `kind` for this window does.
export function onTold(kind, run) {
  heard.set(kind, run);
}

// Tells window `to` a message of `kind`.
export function tell(to, kind, body) {
  localStorage.setItem(TELL, JSON.stringify({ to, kind, body, at: Date.now() }));
  localStorage.removeItem(TELL);
}

// Opens a new window on the tree at `root`, or this window's, to run the menus' command `words`
// names there.
export function openWindow(words = [], root = null) {
  return invoke("window_open", { root, words });
}

export const showWindow = () => invoke("window_act", { act: "focus" }).catch(() => false);
