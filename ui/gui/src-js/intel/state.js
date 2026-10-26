// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What every part of the language server's intel reads: its state, and the tab the editor shows with its cursor.

import { say } from "../statusbar.js";

export const state = { hooks: null, usages: [], closed: new Set(), asked: 0, rename: null, signature: null, signatureAsked: 0, signatureWait: 0, calls: null };

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The tab the editor shows, and the place of its cursor counted from the file's first line.
export function here() {
  // An editor can move its cursor before the hooks are given, as it does once as it is made.
  if (!state.hooks) {
    return null;
  }
  const editor = state.hooks.editor();
  const tab = state.hooks.tab();
  const s = editor?.s;
  if (!s || !tab || tab.session !== s) {
    return null;
  }
  const head = editor.head();
  return { editor, tab, s, head, at: { path: tab.file, line: s.base + head.line, col: head.col } };
}

// The tab, where it is served, or null after saying why there is nothing to ask.
export function served() {
  const found = here();
  if (!found) {
    return null;
  }
  if (!found.tab.served) {
    say(`No language server serves ${found.s.language?.name ?? "plain text"}: File, Toolchains lists those there are.`);
    return null;
  }
  return found;
}
