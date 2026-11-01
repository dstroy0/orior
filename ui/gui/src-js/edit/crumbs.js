// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { drawProblems, iconOf, paneOpen, shownGroup } from "../explorer.js";
import { symbolsOf } from "../outline.js";
import { checkTree, definition, knownProblems, treeChecking } from "../servers.js";
import { showMenu } from "../menu.js";
import { say } from "../statusbar.js";
import { element, state } from "../edit.js";
import { openFile } from "./open.js";
import { jumpTo } from "./places.js";
import { openAt, saving } from "./save.js";
import { tabName, tabOf } from "./tabs.js";
import { childrenOf } from "./tree.js";

// Every file's diagnostics, a file at a time, for the Problems pane: the open files' first, as the
// tabs stand, then the rest of the tree's by path.
export function problemFiles() {
  const open = state.tabs.filter((tab) => !tab.commit && tab.session);
  const shown = new Set(open.map((tab) => tab.file));
  const rest = [...knownProblems()].filter(([path]) => !shown.has(path)).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0));
  return open.filter((tab) => tab.session.diagnostics?.length).map((tab) => ({ path: tab.file, items: tab.session.diagnostics, open: true })).concat(rest.map(([path, items]) => ({ path, items, open: false })));
}

// Draws the Problems pane again, and tells the tool strip how many errors and warnings there are; at
// most once each PROBLEMS_REST, or each CHECKING_REST while the tree's check brings a file's
// diagnostics at a time.
const PROBLEMS_REST = 120;
const CHECKING_REST = 400;
let problemsWaiting = 0;
export function problemsChanged() {
  if (!problemsWaiting) {
    const checking = treeChecking();
    problemsWaiting = window.setTimeout(drawAllProblems, checking && checking.done < checking.total ? CHECKING_REST : PROBLEMS_REST);
  }
}

// Starts the tree's check where Problems shows.
export function checkShown() {
  if (paneOpen("problems") && shownGroup() === "problems") {
    checkTree();
  }
}

function drawAllProblems() {
  problemsWaiting = 0;
  const files = problemFiles();
  drawProblems(files, treeChecking());
  const all = files.flatMap((file) => file.items);
  window.dispatchEvent(new CustomEvent("problems-changed", { detail: { errors: all.filter((one) => one.severity === 1).length, warnings: all.filter((one) => one.severity === 2).length } }));
}

// Go to Definition, and a click with Ctrl pressed: where the language server says the symbol at `p`
// is defined. A file under the tree opens there; one outside it, as a system header is, is named.
export async function goToDefinition(p) {
  const tab = tabOf(state.active);
  if (!tab?.served) {
    say(`No language server serves ${tab?.session?.language?.name ?? "plain text"}: File, Toolchains lists those there are.`);
    return;
  }
  const [found] = await definition(tab, p);
  if (!found) {
    say("No definition found.");
  } else if (/^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(found.path)) {
    say(`Defined at ${found.path}:${found.line + 1}`);
  } else {
    await openAt(found.path, found.line, found.col);
  }
}

// Takes the ends of a tab's lines off and puts a line end after its last line, as the settings say,
// as one edit that undo takes back.
export function tidy(tab) {
  const s = tab.session;
  if (!s || s.readOnly || s.window) {
    return;
  }
  const doc = s.doc;
  const edits = [];
  if (saving("trim") && !/\.(?:md|markdown)$/i.test(tab.file)) {
    for (let line = 0; line < doc.count; line += 1) {
      const text = doc.line(line);
      const kept = text.replace(/[ \t]+$/, "").length;
      if (kept !== text.length) {
        edits.push({ from: { line, col: kept }, to: { line, col: text.length }, text: "" });
      }
    }
  }
  const last = doc.count - 1;
  if (saving("final-newline") && doc.line(last) !== "") {
    const end = { line: last, col: doc.line(last).length };
    edits.push({ from: end, to: end, text: "\n" });
  }
  if (!edits.length) {
    return;
  }
  if (state.editor.s === s) {
    state.editor.change(edits, "tidy");
    return;
  }
  doc.change(edits, "tidy", s.selections);
  s.selections = s.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
  doc.settle(s.selections);
}

// The breadcrumbs.

const CRUMBS_KEY = "orior.crumbs";

// How long a text's symbols are kept for the breadcrumbs after an edit before they are read again,
// in milliseconds.
const CRUMB_SYMBOLS_KEPT = 500;

export function crumbsShown() {
  return localStorage.getItem(CRUMBS_KEY) !== "false";
}

export function setCrumbs(on = !crumbsShown()) {
  localStorage.setItem(CRUMBS_KEY, String(on));
  drawCrumbs();
}

function crumbSymbols(s) {
  const now = performance.now();
  if (!s.crumbs || (s.crumbs.id !== s.doc.id && now - s.crumbs.at > CRUMB_SYMBOLS_KEPT)) {
    s.crumbs = { id: s.doc.id, at: now, symbols: symbolsOf(s.language?.id, s.doc) };
  }
  return s.crumbs.symbols;
}

// The symbols the cursor's line is inside: the last at each depth that starts at or above it, where
// the region it opens reaches the line or it stands on the line itself.
function symbolsAround(s, line) {
  const stack = [];
  for (const symbol of crumbSymbols(s)) {
    if (symbol.line > line) {
      break;
    }
    stack.length = Math.min(stack.length, symbol.depth);
    stack.push(symbol);
  }
  return stack.filter((symbol) => symbol.line === line || s.endOf(symbol.line) >= line);
}

// The symbols beside one in the crumbs: those `parent` holds at `depth`, or the file's outermost
// where it is null.
function symbolsBeside(s, parent, depth) {
  const chain = [];
  const beside = [];
  for (const one of crumbSymbols(s)) {
    chain.length = Math.min(chain.length, one.depth);
    if (one.depth === depth && (chain.at(-1)?.line ?? -1) === (parent?.line ?? -1)) {
      beside.push(one);
    }
    chain.push(one);
  }
  return beside;
}

// A folder's entries as a list from the crumbs: a file opens at a press, and a folder opens its own
// entries beside it.
async function entryItems(dir) {
  return (await childrenOf(dir)).map((entry) => (entry.dir ? { label: entry.name, items: () => entryItems(entry.path) } : { label: entry.name, command: "file-open", args: [entry.path], run: () => openFile(entry.path) }));
}

// Opens the list of the files and folders in `dir` from a crumb, the one named `name` taking the keys.
async function listBeside(crumb, dir, name) {
  openFromCrumb(crumb, await entryItems(dir), name);
}

// Opens a list over a crumb of the status bar, the item labeled `label` taking the keys.
function openFromCrumb(crumb, items, label) {
  const box = crumb.getBoundingClientRect();
  const menu = showMenu(box.left, box.top, items, { anchor: crumb });
  const own = [...menu.querySelectorAll(".menu-item")].find((one) => one.textContent === label);
  own?.focus();
  own?.scrollIntoView({ block: "nearest" });
}

// The breadcrumbs of the file shown: its folders, it, and the symbols the cursor is in. A press on a
// folder or the file opens the list of what stands beside it, and on a symbol the symbols beside it.
export function drawCrumbs() {
  const bar = document.getElementById("crumbs");
  const tab = tabOf(state.active);
  bar.hidden = !tab || !crumbsShown();
  if (bar.hidden) {
    return;
  }
  const parts = tab.file.split("/");
  const crumb = (text, run, className = "") => {
    const button = element("button", { type: "button", className: `crumb ${className}`.trim(), textContent: text });
    button.addEventListener("click", run);
    return button;
  };
  const nodes = [];
  // Each folder's and the file's crumb opens the list of the folder that holds it, with it chosen.
  parts.forEach((name, at) => {
    const part = crumb(at === parts.length - 1 ? tabName(tab) : name, () => listBeside(part, parts.slice(0, at).join("/"), name), at === parts.length - 1 ? "file" : "");
    nodes.push(part);
  });
  nodes.at(-1).prepend(iconOf(tab.file.split("/").pop(), Boolean(state.known?.typeOf(tab.file))));
  const s = tab.session;
  if (s && state.editor?.s === s) {
    const around = symbolsAround(s, state.editor.head().line);
    around.forEach((symbol, at) => {
      const part = crumb(symbol.name, () => openFromCrumb(part, symbolsBeside(s, around[at - 1] ?? null, symbol.depth).map((one) => ({ label: one.name, run: () => jumpTo(one.line) })), symbol.name), "symbol");
      nodes.push(part);
    });
  }
  const key = nodes.map((node) => node.textContent).join("/");
  if (bar.dataset.key === key) {
    return;
  }
  bar.dataset.key = key;
  bar.replaceChildren(...nodes.flatMap((node, at) => (at ? [element("span", { className: "crumb-gap", textContent: "›", ariaHidden: "true" }), node] : [node])));
}

