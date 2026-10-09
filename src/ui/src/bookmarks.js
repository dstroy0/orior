// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Bookmarks: lines a reader marks to come back to. Go, Toggle Bookmark (Ctrl+F11) sets or clears one
// on the cursor's line, drawn as a ribbon in the gutter, and Go, Bookmarks (Ctrl+Shift+F11) lists
// them all in the quick open, each with its line's text, a press going there. They are kept between
// visits, by tree.

import { fuzzy } from "./fuzzy.js";
import { openPalette } from "./palette.js";

const KEPT = "orior.bookmarks";

const state = { hooks: null, marks: new Map() };

const treeKey = () => document.getElementById("tree-path")?.textContent ?? "";

function keep() {
  let all = {};
  try {
    all = JSON.parse(localStorage.getItem(KEPT) ?? "{}") ?? {};
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
  localStorage.setItem(KEPT, JSON.stringify(all));
}

// Reads the bookmarks kept for the tree that is open.
export function loadBookmarks() {
  state.marks = new Map();
  try {
    const mine = JSON.parse(localStorage.getItem(KEPT) ?? "{}")?.[treeKey()] ?? {};
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
  keep();
  state.hooks.repaint();
}

// Lists every bookmark in the quick open, the file's name and line, then the line's text.
export function showBookmarks() {
  openPalette("", {
    custom: {
      placeholder: "Bookmarks: type to filter, Enter to go there",
      empty: "No bookmarks. Go, Toggle Bookmark (Ctrl+F11) sets one on the cursor's line.",
      rows: (query) => {
        const rows = [];
        for (const [path, lines] of [...state.marks].sort(([a], [b]) => a.localeCompare(b))) {
          for (const line of [...lines].sort((a, b) => a - b)) {
            const label = `${path.split("/").pop()}:${line + 1}`;
            const text = (state.hooks.lineOf(path, line) ?? "").trim();
            const found = fuzzy(query, `${label} ${text}`);
            if (!found) {
              continue;
            }
            rows.push({
              label,
              hits: found.hits.filter((at) => at < label.length),
              detail: text.slice(0, 120),
              score: query ? found.score : 0,
              run: () => state.hooks.openAt(path, line, 0),
            });
          }
        }
        return query ? rows.sort((a, b) => b.score - a.score) : rows;
      },
    },
  });
}

// `hooks` gives the cursor's file and line, a line of an open file, opens a file at a place, and
// draws the editor again.
export function startBookmarks(hooks) {
  state.hooks = hooks;
  loadBookmarks();
}
