// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Bookmarks: lines a reader marks to come back to. Go, Toggle Bookmark (Ctrl+F11) sets or clears one
// on the cursor's line, drawn as a ribbon in the gutter, and Go, Bookmarks (Ctrl+Shift+F11) lists
// them all in the quick open, each with its line's text, a press going there. They are retained between
// visits, by tree.

import { openPalette } from "./palette.js";

const RETAINED = "orior.bookmarks";

const state = { hooks: null, marks: new Map() };

const treeKey = () => document.getElementById("tree-path")?.textContent ?? "";

function retain() {
  let all = {};
  try {
    all = JSON.parse(localStorage.getItem(RETAINED) ?? "{}") ?? {};
  } catch {
    all = {};
  }
  const mine = {};
  for (const [path, lines] of state.marks) {
    if (lines.size) {
      mine[path] = [...lines].sort((a, b) => a - b);
    }
  }
  all[treeKey()] = mine;
  localStorage.setItem(RETAINED, JSON.stringify(all));
}

// Reads the bookmarks retained for the tree that is open.
export function loadBookmarks() {
  state.marks = new Map();
  try {
    const mine = JSON.parse(localStorage.getItem(RETAINED) ?? "{}")?.[treeKey()] ?? {};
    for (const [path, lines] of Object.entries(mine)) {
      state.marks.set(path, new Set(lines));
    }
  } catch {
    state.marks = new Map();
  }
  state.hooks?.repaint();
}

// The bookmarked lines of a file, or null where it has none.
export function bookmarksOf(path) {
  const lines = state.marks.get(path);
  return lines?.size ? lines : null;
}

export function toggleBookmark() {
  const at = state.hooks.here();
  if (!at) {
    return;
  }
  if (!state.marks.has(at.path)) {
    state.marks.set(at.path, new Set());
  }
  const lines = state.marks.get(at.path);
  if (lines.has(at.line)) {
    lines.delete(at.line);
  } else {
    lines.add(at.line);
  }
  retain();
  state.hooks.repaint();
}

// Lists every bookmark in the quick open, the file's name and line, then the line's text.
export function showBookmarks() {
  openPalette("", {
    custom: {
      placeholder: "Bookmarks: type to filter, Enter to go there",
      empty: "No bookmarks. Go, Toggle Bookmark (Ctrl+F11) sets one on the cursor's line.",
      items: [...state.marks]
        .sort(([a], [b]) => a.localeCompare(b))
        .flatMap(([path, lines]) =>
          [...lines]
            .sort((a, b) => a - b)
            .map((line) => {
              const label = `${path.split("/").pop()}:${line + 1}`;
              const text = (state.hooks.lineOf(path, line) ?? "").trim();
              return { text: `${label} ${text}`, label, detail: text.slice(0, 120), run: () => state.hooks.openAt(path, line, 0) };
            }),
        ),
    },
  });
}

// `hooks` gives the cursor's file and line, a line of an open file, opens a file at a place, and
// draws the editor again.
export function startBookmarks(hooks) {
  state.hooks = hooks;
  loadBookmarks();
}
