// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Language servers, as servers.rs in the command line's crate runs them: a tab whose language has
// one in the toolchain manifest is handed to it when it opens, told of each change a moment after
// typing rests, and let go when it closes. Its hovers, completions and definitions come from the
// server, and the server's diagnostics are drawn under the text they are about. A tab of a language
// orior's own inspections read is handed over, told of its changes and let go the same way where no
// server serves it, and hears the inspections' findings.

import { asked, invoke, listen } from "./bridge.js";
import { wordAt } from "./editor/document.js";

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
    const turn = (tab.hintsAsked = (tab.hintsAsked ?? 0) + 1);
    const version = s.doc.id;
    const found = await asked("lsp_hints", { path: tab.file, from: s.base + low, to: s.base + high }).catch(() => null);
    if (!found || turn !== tab.hintsAsked || s.doc.id !== version) {
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

// The parse of a served or an inspected tab, orior's own or its server's, asked for the lines shown
// and PARSE_AROUND on each side once typing and scrolling rest, its spans laid over the colors the
// language's patterns give and its folds standing in for those the indentation gives.
const PARSE_AROUND = 120;
const PARSE_REST = 150;
const PARSE_AGAIN = 2000;
let classNames = null;

export function parseShown(tab, from, to) {
  const s = tab?.session;
  if (!(tab.served || tab.inspected) || !s || s.window || tab.unparsed === s.doc.id || (tab.parseAgain ?? 0) > Date.now()) {
    return;
  }
  const have = s.highlight.parse;
  if (have && !have.stale && have.from <= from && have.to >= Math.min(to, s.doc.count - 1)) {
    return;
  }
  // One parse is asked for at a time: one wanted while another is out is asked for once that answers.
  if (tab.parseOut) {
    tab.parseWanted = [from, to];
    return;
  }
  window.clearTimeout(tab.parsing);
  tab.parsing = window.setTimeout(async () => {
    tab.parseOut = true;
    try {
      await parseNow(tab, from, to);
    } finally {
      tab.parseOut = false;
    }
    const wanted = tab.parseWanted;
    tab.parseWanted = null;
    if (wanted) {
      parseShown(tab, ...wanted);
    }
  }, PARSE_REST);
}

// The parse of `tab`'s lines `from` to `to` and those around them, asked for now and laid over its colors.
async function parseNow(tab, from, to) {
  const s = tab.session;
  await flush(tab);
  classNames ??= await invoke("parse_classes").catch(() => null);
  const version = s.doc.id;
  const told = tab.told;
  const low = Math.max(0, from - PARSE_AROUND);
  const high = Math.min(s.doc.count - 1, to + PARSE_AROUND);
  // A parse that fails, as a server still reading the tree's fails, is asked for again after a moment.
  const found = await asked("parse_colors", { path: tab.file, from: low, to: high }).catch(() => undefined);
  if (found === undefined) {
    tab.parseAgain = Date.now() + PARSE_AGAIN;
    return;
  }
  // A change told while the parse was read may not be in it.
  if (s.doc.id !== version || tab.told !== told || !classNames) {
    return;
  }
  if (!found) {
    tab.unparsed = version;
    return;
  }
  const kept = s.highlight.parse;
  const byLine = kept && !kept.stale ? kept.byLine : new Map();
  found.lines.forEach((spans, index) => byLine.set(found.from + index, spans));
  const span = kept && !kept.stale ? [Math.min(kept.from, found.from), Math.max(kept.to, found.from + found.lines.length - 1)] : [found.from, found.from + found.lines.length - 1];
  s.highlight.parse = { byLine, names: classNames, whole: found.whole, from: span[0], to: span[1], folds: new Map(found.folds), stale: false };
  s.view?.schedule();
}

// The spans of a served or an inspected tab that hold `from` to `to` and are more than it, the least
// first, for Expand Selection.
export async function spansOf(tab, from, to) {
  const s = tab?.session;
  if (!(tab?.served || tab?.inspected) || !s || s.window) {
    return [];
  }
  await flush(tab);
  const found = await asked("parse_spans", { path: tab.file, from: { line: s.base + from.line, col: from.col }, to: { line: s.base + to.line, col: to.col } }).catch(() => []);
  return (found ?? []).map(([start, end]) => [{ line: start.line - s.base, col: start.col }, { line: end.line - s.base, col: end.col }]);
}

// The checkers the reader names, as checkers.rs in the command line's crate runs them, kept by
// their names, a comma between each.
const CHECKERS_KEY = "orior.checkers";

export function checkersNamed() {
  return (localStorage.getItem(CHECKERS_KEY) ?? "").split(",").map((name) => name.trim()).filter(Boolean);
}

// Names the checkers to run as files change. Gives the names no checker answers to.
export async function setCheckers(names) {
  localStorage.setItem(CHECKERS_KEY, names.join(", "));
  return invoke("checkers_set", { names }).catch(() => []);
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
  invoke("checkers_set", { names: checkersNamed() }).catch(() => {});
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
  // A server that colors its files again has every tab's parse from a server asked for again; orior's
  // own parse of a file, whole, does not change with the server's.
  listen("lsp-parse", () => {
    for (const tab of tabsOf()) {
      tab.unparsed = null;
      if (tab.session?.highlight.parse && !tab.session.highlight.parse.whole) {
        tab.session.highlight.parse.stale = true;
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
  // The diagnostics a server already holds for the file can come before it answers. A file orior
  // reads itself, a build file or JSON, is completed from the start, as its server starts or where
  // none serves it.
  tab.serving = true;
  tab.readHere = await invoke("reads_file", { path: tab.file }).catch(() => false);
  if (tab.readHere) {
    s.diagnostics ??= [];
    wrap(tab);
  }
  const sent = s.doc.text();
  const sentDoc = s.doc.id;
  const took = await invoke("lsp_open", { path: tab.file, language, text: sent }).catch(() => false);
  tab.serving = false;
  tab.toldDoc = sentDoc;
  if (took && tabsOf().includes(tab)) {
    tab.served = true;
    s.diagnostics ??= [];
    wrap(tab);
    // A question asked now, its answer let go, has the server read the file through before the
    // reader asks one: the first Go to Definition answers as quickly as the next.
    asked("lsp_hover", { path: tab.file, line: 0, col: 0 }).catch(() => {});
  } else if (took) {
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  } else if ((inspected.has(language) || tab.readHere) && tabsOf().includes(tab)) {
    tab.inspected = true;
    s.diagnostics ??= [];
  }
  // What was typed while the server started is told it now.
  if ((tab.served || tab.inspected) && s.doc.text() !== sent) {
    changed(tab);
  }
}

// Hands every open tab to its language's server again, the servers having ended, as they do with the
// link to the tree's machine.
export function serveAgain() {
  for (const tab of tabsOf()) {
    if (tab.served !== undefined || tab.inspected) {
      stopTelling(tab);
      Object.assign(tab, { served: undefined, inspected: false, untold: false, toldDoc: undefined });
      serve(tab);
    }
  }
}

// Gives a served tab's language the server's hover and completion, over the language's own, and the
// tab of a file orior_gui reads itself, a build file or JSON, the completions orior_gui gives it. A language
// set again, as a plugin read again sets it, is given them again.
// What the tree's index of declarations says of a word: each declaration of that name, its kind and
// where it is, as a hover's parts; none where the word is not declared in the tree.
const TREE_HOVER_MOST = 6;
async function treeHover(word) {
  if (!word) {
    return [];
  }
  const found = await invoke("symbols_find", { query: word.text }).catch(() => null);
  const same = (found?.symbols ?? []).filter((one) => one.name === word.text).slice(0, TREE_HOVER_MOST);
  // the declaration's own line first, as code, then what it is and where
  return same.map((one) => `${one.text ? "```\n" + one.text + "\n```\n\n" : ""}${one.kind} \`${one.path}:${one.line + 1}\``);
}

export function wrap(tab) {
  const s = tab.session;
  if (!(tab.served || tab.readHere) || !s?.language || s.language.served) {
    return;
  }
  const own = s.language;
  const at = (p) => ({ path: tab.file, line: s.base + p.line, col: p.col });
  s.language = Object.assign(Object.create(own), {
    served: true,
    // The tree answers first and at once: the language's own hover, else the declarations of the word
    // under the pointer that the tree's index holds. The server's answer, where a server holds the file,
    // comes later as `more`, and nothing waits on it.
    async hover(doc, p) {
      const later = flush(tab)
        .then(() => asked("lsp_hover", at(p)))
        .then((said) => (said ? [said] : null))
        .catch(() => null);
      const word = wordAt(doc, p);
      const span = word ? { from: word.from, to: word.to } : { from: p, to: p };
      const found = own.hover ? await own.hover(doc, p) : null;
      const parts = found?.parts?.length ? [...found.parts] : await treeHover(word);
      if (parts.length) {
        return { ...span, ...(found?.from ? { from: found.from, to: found.to } : {}), parts, more: later };
      }
      const said = await later;
      return said ? { ...span, parts: said } : null;
    },
    async complete(doc, p) {
      await flush(tab);
      return asked("lsp_complete", at(p)).catch(() => []);
    },
    completeNow(doc, p) {
      return own.complete?.(doc, p) ?? [];
    },
  });
}

// Tells the server of a change to a served tab, or the inspections of one to an inspected tab, as
// soon as the frame that draws it has been drawn, with no wait of the window's own: the letter shows
// first, and the server's marks follow as soon as it has them. Keys typed while a change is on its
// way are told together once it has been taken.
export function changed(tab) {
  if (!tab?.served && !tab?.inspected) {
    return;
  }
  tab.untold = true;
  if (tab.telling) {
    return;
  }
  const telling = (tab.telling = {});
  requestAnimationFrame(() =>
    setTimeout(() => {
      if (tab.telling === telling) {
        tab.telling = null;
        flush(tab);
      }
    }, 0),
  );
}

// Lets go of a change waiting to be told, as the tab stops being served.
export function stopTelling(tab) {
  if (tab) {
    tab.telling = null;
  }
}

// Tells the server of a change to a served tab now, where one is waiting for typing to rest, so
// that what is asked next is asked of the text as it stands.
//
// The server holds the text of the version it was last told, `toldDoc`; a tab whose version has
// moved since is told its text, whether the edit has said it changed yet, and one whose version has
// come back to it is told nothing.
//
// A parse or hints asked before the change was told were read from the text before it: once it is
// told, they are asked for again.
export async function flush(tab) {
  stopTelling(tab);
  const s = tab?.session;
  if (!(tab?.served || tab?.inspected) || !s) {
    return;
  }
  // The change on its way is taken before the next is told, for the server to hear them in order.
  await tab.sending;
  const behind = tab.toldDoc === undefined ? tab.untold : tab.toldDoc !== s.doc.id;
  tab.untold = false;
  if (behind) {
    tab.toldDoc = s.doc.id;
    tab.told = (tab.told ?? 0) + 1;
    tab.sending = invoke("lsp_change", { path: tab.file, text: s.doc.text() }).catch(() => {});
    await tab.sending;
    if (s.highlight.parse) {
      s.highlight.parse.stale = true;
    }
    if (s.hints) {
      s.hints.stale = true;
    }
    s.view?.schedule();
  }
  // A change another flush has on its way is taken before what is asked next.
  await tab.sending;
}

export function stopServing(tab) {
  if (tab?.served || tab?.inspected) {
    tab.served = false;
    tab.inspected = false;
    tab.untold = false;
    stopTelling(tab);
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  }
}

// The server's hover at a place in a served tab, as Markdown, or null. Given `text`, the server is
// asked of that text in place of the tab's, and told the tab's own again after. A server still
// reading a change, which answers that the content was modified or answers nothing, is asked again a
// moment later, up to ASKS times for the one and a few for the other; where `named`, a name is known
// to stand at the place, and an answer of nothing is asked again ASKS times too, as a server still
// reading the tree gives.
const ASKS = 20;
export async function hoverAt(tab, p, text = null, named = false) {
  if (!tab?.served) {
    return null;
  }
  await flush(tab);
  if (text !== null) {
    // The server holds a text no version of the tab's is, until the tab's own is told again.
    tab.toldDoc = null;
    await invoke("lsp_change", { path: tab.file, text }).catch(() => {});
  }
  try {
    let empty = 0;
    for (let asked = 0; asked < ASKS; asked += 1) {
      try {
        const said = await asked("lsp_hover", { path: tab.file, line: tab.session.base + p.line, col: p.col });
        if (said) {
          return said;
        }
        empty += 1;
        if (empty > (named ? ASKS : 3)) {
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
  return asked("lsp_definition", { path: tab.file, line: tab.session.base + p.line, col: p.col }).catch(() => []);
}
