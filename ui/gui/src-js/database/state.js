// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Database tool window's state and parts, and the connection a file runs on, which each of its parts reads.

// The rows asked for at a time past the end, the height of a row of the grid, and the fewest rows
// read beyond those in view each way.
export const PAGE = 200;
export const ROW = 22;
export const SPARE = 40;

export const CHOSEN = "orior.database.chosen";
export const FILES = "orior.database.files";

export const state = {
  hooks: null,
  connections: [],
  open: new Map(),
  // The schema's levels read, by their path, and those opened.
  levels: new Map(),
  opened: new Set(),
  reading: 0,
  // The result shown: the connection it came from, its columns, the rows held here by their index,
  // how many the server has read and whether it has read them all.
  result: null,
  running: null,
};
export const parts = {};

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export const chosen = () => localStorage.getItem(CHOSEN) ?? "";
export const connectionOf = (name) => state.connections.find((one) => one.connection.name === name)?.connection ?? null;
export const keyOf = (path) => path.join("\u0000");

export function filesKept() {
  try {
    return JSON.parse(localStorage.getItem(FILES) ?? "{}") ?? {};
  } catch {
    return {};
  }
}

// The connection a file runs on: the one chosen for it, or the window's.
export function connectionForFile(path) {
  return filesKept()[path] || chosen();
}
