// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The patterns that keep the explorer and the search each from files of the tree, one to a line, set
// in Preferences. Each list is kept for the reader and handed to orior's Rust, which reads a pattern
// as a .gitignore line: `*` any letters of a name, `**` any folders, a trailing `/` folders only, and
// a pattern starting with `!` keeping only what it names.

import { invoke } from "./bridge.js";

export const PATTERN_PARTS = [
  { part: "explorer", label: "Hidden from the explorer" },
  { part: "search", label: "Hidden from the search" },
];

const keyOf = (part) => `orior.patterns.${part}`;
const listeners = [];

export function patternsOf(part) {
  return localStorage.getItem(keyOf(part)) ?? "";
}

const tell = (part) => invoke("patterns_set", { part, lines: patternsOf(part).split("\n") }).catch(() => false);

// Hands every part's patterns to the Rust, as the window starts.
export function tellPatterns() {
  return Promise.all(PATTERN_PARTS.map(({ part }) => tell(part)));
}

// Keeps a part's patterns and hands them over, then tells each listener the part whose patterns
// changed.
export async function setPatterns(part, text) {
  localStorage.setItem(keyOf(part), text);
  if (await tell(part)) {
    listeners.forEach((listener) => listener(part));
  }
}

export function onPatterns(listener) {
  listeners.push(listener);
}
