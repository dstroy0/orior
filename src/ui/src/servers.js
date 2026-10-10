// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Language servers, as servers.rs in the command line's crate runs them: a tab whose language has
// one in the toolchain manifest is handed to it when it opens, told of each change a moment after
// typing rests, and let go when it closes. Its hovers, completions and definitions come from the
// server, and the server's diagnostics are drawn under the text they are about. A tab of a language
// orior's own inspections read is handed over, told of its changes and let go the same way where no
// server serves it, and hears the inspections' findings.

import { invoke, listen } from "./bridge.js";

const CHANGE_REST = 300;

let tabsOf = () => [];
let painted = () => {};

// Inlay hints: the types and the parameter names a server writes in a served tab's text, asked for
// the lines shown and HINTS_AROUND on each side, once typing and scrolling rest, and again after the
// text changes or the view moves past them.
const HINTS_AROUND = 60;
const HINTS_REST = 250;

export function hintsShown(tab, from, to) {
  const s = tab?.session;
  if (!tab.served || !s || s.window) {
    return;
  }
  const have = s.hints;
  if (have && !have.stale && have.from <= from && have.to >= Math.min(to, s.doc.count - 1)) {
    return;
  }
  window.clearTimeout(tab.hinting);
  tab.hinting = window.setTimeout(async () => {
    await flush(tab);
    const low = Math.max(0, from - HINTS_AROUND);
    const high = Math.min(s.doc.count - 1, to + HINTS_AROUND);
    const asked = (tab.hintsAsked = (tab.hintsAsked ?? 0) + 1);
    const version = s.doc.id;
    const found = await invoke("lsp_hints", { path: tab.file, from: s.base + low, to: s.base + high }).catch(() => null);
    if (!found || asked !== tab.hintsAsked || s.doc.id !== version) {
      return;
    }
    const byLine = new Map();
    for (const hint of found) {
      const line = hint.line - s.base;
      if (line < 0 || line >= s.doc.count) {
        continue;
      }
      const width = [...hint.label].length + (hint.left ? 1 : 0) + (hint.right ? 1 : 0);
      if (!byLine.has(line)) {
        byLine.set(line, []);
      }
      byLine.get(line).push({ col: hint.col, label: hint.label, kind: hint.kind, left: hint.left, right: hint.right, width });
    }
    for (const hints of byLine.values()) {
      hints.sort((a, b) => a.col - b.col || (a.kind === 2) - (b.kind === 2));
    }
    s.hints = { byLine, from: low, to: high, stale: false };
    s.view?.schedule();
  }, HINTS_REST);
}

// Every file's diagnostics as its server last gave them, open in a tab or not; how far the tree's
// check has gone; and whether it has started in the tree open.
const known = new Map();
let checking = null;
let checked = false;

// The languages orior's own inspections read.
let inspected = new Set();

// `tabs` gives the open tabs, and `paint` draws the editor again after diagnostics arrive.
export function startServers({ tabs, paint }) {
  tabsOf = tabs;
  painted = paint;
  invoke("inspect_languages")
    .then((languages) => {
      inspected = new Set(languages);
    })
    .catch(() => {});
  listen("lsp-diagnostics", (event) => {
    const { path, items } = event.payload;
    if (items.length) {
      known.set(path, items);
    } else {
      known.delete(path);
    }
    for (const tab of tabsOf()) {
      if ((tab.served || tab.serving || tab.inspected) && tab.file === path) {
        tab.session.diagnostics = items;
      }
    }
    painted();
  });
  // A server that has read more of the tree has every tab's hints asked for again.
  listen("lsp-hints", () => {
    for (const tab of tabsOf()) {
      if (tab.session?.hints) {
        tab.session.hints.stale = true;
      }
    }
    painted();
  });
  listen("tree-check", (event) => {
    checking = event.payload;
    painted();
  });
}

// The tree's check, started once in the tree open: every file of the tree no tab holds is handed to
// its language's server, as servers.rs checks the tree, and is checked again as it changes.
export function checkTree() {
  if (!checked) {
    checked = true;
    invoke("problems_check").catch(() => {
      checked = false;
    });
  }
}

// Every file's diagnostics the servers have given, by its path.
export function knownProblems() {
  return known;
}

// How far the tree's check has gone, `{ done, total }`, or null before it starts.
export function treeChecking() {
  return checking;
}

// Forgets the diagnostics and the check of the tree open, for another tree opened in its place.
export function forgetProblems() {
  known.clear();
  checking = null;
  checked = false;
}

// Hands a tab to its language's server where there is one. A file read a window at a time, or
// as a commit left it, is not handed over.
export async function serve(tab) {
  const s = tab.session;
  if (!s || s.window || tab.commit || tab.served !== undefined) {
    return;
  }
  tab.served = false;
  const language = s.language?.id;
  if (!language) {
    return;
  }
  // The diagnostics a server already holds for the file can come before it answers.
  tab.serving = true;
  const took = await invoke("lsp_open", { path: tab.file, language, text: s.doc.text() }).catch(() => false);
  tab.serving = false;
  if (took && tabsOf().includes(tab)) {
    tab.served = true;
    s.diagnostics ??= [];
    wrap(tab);
    // A question asked now, its answer let go, has the server read the file through before the
    // reader asks one: the first Go to Definition answers as quickly as the next.
    invoke("lsp_hover", { path: tab.file, line: 0, col: 0 }).catch(() => {});
  } else if (took) {
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  } else if (inspected.has(language) && tabsOf().includes(tab)) {
    tab.inspected = true;
    s.diagnostics ??= [];
  }
}

// Gives a served tab's language the server's hover and completion, over the language's own. A
// language set again, as a plugin read again sets it, is given them again.
export function wrap(tab) {
  const s = tab.session;
  if (!tab.served || !s?.language || s.language.served) {
    return;
  }
  const own = s.language;
  const at = (p) => ({ path: tab.file, line: s.base + p.line, col: p.col });
  s.language = Object.assign(Object.create(own), {
    served: true,
    async hover(doc, p) {
      const parts = [];
      await flush(tab);
      const said = await invoke("lsp_hover", at(p)).catch(() => null);
      if (said) {
        parts.push(said);
      } else if (own.hover) {
        const found = await own.hover(doc, p);
        parts.push(...(found?.parts ?? []));
      }
      if (!parts.length) {
        return null;
      }
      const text = doc.line(p.line);
      let from = p.col;
      let to = p.col;
      while (from > 0 && /\w/.test(text[from - 1])) {
        from -= 1;
      }
      while (to < text.length && /\w/.test(text[to])) {
        to += 1;
      }
      return { from: { line: p.line, col: from }, to: { line: p.line, col: to }, parts };
    },
    async complete(doc, p) {
      await flush(tab);
      const items = await invoke("lsp_complete", at(p)).catch(() => []);
      return items.length ? items : (own.complete?.(doc, p) ?? []);
    },
  });
}

// Tells the server of a change to a served tab, or the inspections of one to an inspected tab, once
// typing has rested.
export function changed(tab) {
  if (!tab?.served && !tab?.inspected) {
    return;
  }
  window.clearTimeout(tab.telling);
  tab.untold = true;
  tab.telling = window.setTimeout(() => flush(tab), CHANGE_REST);
}

// Tells the server of a change to a served tab now, where one is waiting for typing to rest, so
// that what is asked next is asked of the text as it stands.
export async function flush(tab) {
  window.clearTimeout(tab?.telling);
  if ((tab?.served || tab?.inspected) && tab.untold) {
    tab.untold = false;
    await invoke("lsp_change", { path: tab.file, text: tab.session.doc.text() }).catch(() => {});
  }
}

export function stopServing(tab) {
  if (tab?.served || tab?.inspected) {
    tab.served = false;
    tab.inspected = false;
    tab.untold = false;
    window.clearTimeout(tab.telling);
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  }
}

// The server's hover at a place in a served tab, as Markdown, or null. Given `text`, the server is
// asked of that text in place of the tab's, and told the tab's own again after. A server still
// reading a change, which answers that the content was modified or answers nothing, is asked again a
// moment later, up to ASKS times for the one and a few for the other.
const ASKS = 20;
export async function hoverAt(tab, p, text = null) {
  if (!tab?.served) {
    return null;
  }
  await flush(tab);
  if (text !== null) {
    await invoke("lsp_change", { path: tab.file, text }).catch(() => {});
  }
  try {
    let empty = 0;
    for (let asked = 0; asked < ASKS; asked += 1) {
      try {
        const said = await invoke("lsp_hover", { path: tab.file, line: tab.session.base + p.line, col: p.col });
        if (said) {
          return said;
        }
        empty += 1;
        if (empty > 3) {
          return null;
        }
      } catch (error) {
        if (!/modified/i.test(String(error))) {
          return null;
        }
      }
      await new Promise((resolve) => window.setTimeout(resolve, 150));
    }
    return null;
  } finally {
    if (text !== null) {
      tab.untold = true;
      await flush(tab);
    }
  }
}

// Where the symbol at a place in a served tab is defined, each { path, line, col } counted from
// the file's first: a path under the tree from its top folder, and any other whole.
export async function definition(tab, p) {
  if (!tab?.served) {
    return [];
  }
  await flush(tab);
  return invoke("lsp_definition", { path: tab.file, line: tab.session.base + p.line, col: p.col }).catch(() => []);
}
