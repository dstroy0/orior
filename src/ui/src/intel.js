// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a language server knows of the symbol at the cursor, past its hover, completion and
// definition. Find Usages lists each place the symbol is used in the explorer's Usages pane, a row a
// file and under it a row a place. Rename Symbol takes the new name in a field over the old one and
// writes it wherever the symbol is used: into the tabs that hold those files, where undo takes it
// back, and into the files no tab holds. Quick Fix lists what the server offers at the cursor and
// carries out the one chosen. Parameter Info shows the signature of the call the cursor is in with
// the parameter it is at in bold, and opens on its own as ( or , is typed in a call. Quick
// Documentation shows the hover of the symbol at the cursor. Where the cursor rests on a line a
// diagnostic is on, a mark in the gutter beside it, in the diagnostic's color, opens Quick Fix.
//
// A language with no server finds its usages as whole words in the tree's files, and has no
// rename, quick fix or parameter info.

import { invoke, listen } from "./bridge.js";
import { cmp, wordAt } from "./editor/document.js";
import { escapeHtml } from "./editor/view.js";
import { markup } from "./editor/widgets.js";
import { iconOf, showPane } from "./explorer.js";
import { showMenu } from "./menu.js";
import { focusSearch } from "./search.js";
import { flush } from "./servers.js";
import { togglePane } from "./sides.js";
import { say } from "./statusbar.js";
import { runInTerminal, terminalAt } from "./terminal.js";

// How long the cursor rests before the signature it is in is asked for again, in milliseconds.
const SIGNATURE_REST = 120;

const EMPTY_USAGES = "Find Usages (Shift+F12) on a symbol lists each place it is used.";

// The fix mark: a bulb, drawn in the color of the line's worst diagnostic.
const BULB = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 1.6a4.6 4.6 0 0 0-2.7 8.3c.5.4.8 1 .8 1.6v.5h3.8v-.5c0-.6.3-1.2.8-1.6A4.6 4.6 0 0 0 8 1.6Z" fill="currentColor" fill-opacity=".22" stroke="currentColor" stroke-width="1.2"/><path d="M6.2 13.6h3.6M6.7 15h2.6" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"/></svg>';

const state = { hooks: null, usages: [], closed: new Set(), asked: 0, rename: null, signature: null, signatureAsked: 0, signatureWait: 0, calls: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The tab the editor shows, and the place of its cursor counted from the file's first line.
function here() {
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
function served() {
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

// Edits.

// Writes each file's edits: into the tab that holds the file, as one step undo takes back, and to
// the file itself where no tab holds it. Says how many places in how many files changed.
export async function applyEdits(files) {
  const editor = state.hooks.editor();
  const unopened = [];
  let places = 0;
  for (const file of files) {
    if (!file.edits.length) {
      continue;
    }
    places += file.edits.length;
    const tab = state.hooks.tabs().find((one) => one.file === file.path && !one.commit && one.session);
    if (!tab) {
      unopened.push(file);
      continue;
    }
    const s = tab.session;
    if (s.window || s.readOnly) {
      say(`${file.path} is open as a part of the file, or cannot be written, and was not changed.`, { failed: true });
      continue;
    }
    const edits = file.edits.map(({ from, to, text }) => ({ from: { line: from.line - s.base, col: from.col }, to: { line: to.line - s.base, col: to.col }, text }));
    if (editor?.s === s) {
      editor.change(edits, "server");
    } else {
      s.doc.change(edits, "server", s.selections);
      s.selections = s.selections.map((sel) => ({ anchor: s.doc.clamp(sel.anchor), head: s.doc.clamp(sel.head), goal: null }));
      s.doc.settle(s.selections);
    }
    s.doc.seal();
    state.hooks.touched(tab);
  }
  if (unopened.length) {
    try {
      await invoke("edits_write", { files: unopened });
    } catch (error) {
      say(String(error), { failed: true });
    }
    await state.hooks.refresh();
  }
  const count = files.filter((file) => file.edits.length).length;
  return { places, files: count };
}

// Find Usages.

function drawUsages() {
  const said = document.getElementById("usages-said");
  const hits = document.getElementById("usages-hits");
  const groups = new Map();
  for (const usage of state.usages) {
    if (!groups.has(usage.path)) {
      groups.set(usage.path, []);
    }
    groups.get(usage.path).push(usage);
  }
  const count = state.usages.length;
  said.textContent = state.name === undefined ? EMPTY_USAGES : count ? `${count} usage${count === 1 ? "" : "s"} of ${state.name} in ${groups.size} file${groups.size === 1 ? "" : "s"}` : `No usages of ${state.name} found.`;
  const rows = [];
  for (const [path, found] of groups) {
    const cut = path.lastIndexOf("/");
    const open = !state.closed.has(path);
    const head = element("button", { className: "node dir search-file", type: "button", title: path });
    head.dataset.key = `usages:${path}`;
    head.dataset.depth = "0";
    head.setAttribute("aria-expanded", String(open));
    head.append(element("span", { className: "twisty" }), iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(found.length) }));
    head.addEventListener("click", () => {
      if (state.closed.has(path)) {
        state.closed.delete(path);
      } else {
        state.closed.add(path);
      }
      drawUsages();
      hits.querySelector(`[data-key="usages:${CSS.escape(path)}"]`)?.focus();
    });
    rows.push(head);
    if (!open) {
      continue;
    }
    for (const usage of found) {
      const text = state.hooks.lineOf(path, usage.from.line) ?? usage.text;
      const end = usage.to.line === usage.from.line ? usage.to.col : text.length;
      const lead = text.slice(0, usage.from.col).replace(/^\s+/, "");
      const row = element("button", { className: "search-hit", type: "button", title: `${path}:${usage.from.line + 1}:${usage.from.col + 1}` });
      row.dataset.key = `usages:${path}:${usage.from.line}:${usage.from.col}`;
      row.dataset.depth = "1";
      row.append(
        element("i", { className: "guide" }),
        element("span", { className: "line" }, lead.length > 40 ? `…${lead.slice(-30)}` : lead, element("mark", { textContent: text.slice(usage.from.col, end) }), text.slice(end, end + 160)),
      );
      row.addEventListener("click", () => state.hooks.openAt(path, usage.from.line, usage.from.col));
      rows.push(row);
    }
  }
  hits.replaceChildren(...rows);
}

export async function findUsages() {
  const found = here();
  if (!found) {
    return;
  }
  const word = wordAt(found.s.doc, found.head);
  if (!found.tab.served) {
    if (!word) {
      say("No symbol at the cursor.");
      return;
    }
    togglePane(true);
    showPane("search");
    focusSearch(word.text, { word: true, case: true, regex: false });
    say(`No language server serves ${found.s.language?.name ?? "plain text"}: searching the tree for ${word.text} as a whole word.`);
    return;
  }
  const asked = ++state.asked;
  state.name = word?.text ?? "the symbol";
  togglePane(true, { take: false });
  showPane("usages");
  document.getElementById("usages-said").textContent = `Finding usages of ${state.name}…`;
  document.getElementById("usages-hits").replaceChildren();
  let usages;
  try {
    await flush(found.tab);
    usages = await invoke("lsp_references", found.at);
  } catch (error) {
    if (asked === state.asked) {
      document.getElementById("usages-said").textContent = String(error);
    }
    return;
  }
  if (asked !== state.asked) {
    return;
  }
  state.usages = usages;
  state.closed.clear();
  drawUsages();
}

// Rename Symbol.

function closeRename(back = true) {
  const box = state.rename;
  if (!box) {
    return;
  }
  state.rename = null;
  box.field.remove();
  box.hint.remove();
  if (back) {
    state.hooks.editor()?.focus();
  }
}

async function commitRename() {
  const box = state.rename;
  const name = box.field.value.trim();
  closeRename();
  if (!name || name === box.old) {
    return;
  }
  try {
    const files = await invoke("lsp_rename", { ...box.at, name });
    const done = await applyEdits(files);
    say(`Renamed ${box.old} to ${name} in ${done.places} place${done.places === 1 ? "" : "s"} in ${done.files} file${done.files === 1 ? "" : "s"}.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

export async function renameSymbol() {
  const found = served();
  if (!found) {
    return;
  }
  closeRename(false);
  let span;
  try {
    await flush(found.tab);
    span = await invoke("lsp_renamable", found.at);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const { editor, s } = found;
  const from = span ? { line: span.from.line - s.base, col: span.from.col } : wordAt(s.doc, found.head)?.from;
  const to = span ? { line: span.to.line - s.base, col: span.to.col } : wordAt(s.doc, found.head)?.to;
  if (!from || editor.s !== s) {
    say("Nothing at the cursor can be renamed.");
    return;
  }
  const old = span?.name ?? s.doc.slice(from, to);
  const field = element("input", { className: "ed-rename", type: "text", value: old, spellcheck: false });
  field.setAttribute("aria-label", `Rename ${old}`);
  const host = editor.host.getBoundingClientRect();
  const rect = editor.rectOf(from);
  field.style.left = `${rect.left - host.left - 3}px`;
  field.style.top = `${rect.top - host.top - 2}px`;
  field.style.width = `${Math.max(old.length + 6, 14)}ch`;
  field.addEventListener("input", () => (field.style.width = `${Math.max(field.value.length + 6, 14)}ch`));
  field.addEventListener("keydown", (event) => {
    event.stopPropagation();
    if (event.key === "Enter") {
      event.preventDefault();
      commitRename();
    } else if (event.key === "Escape") {
      event.preventDefault();
      closeRename();
    }
  });
  field.addEventListener("blur", () => state.rename?.field === field && closeRename(false));
  const hint = element("div", { className: "ed-rename-hint", textContent: "Enter renames it everywhere it is used. Escape keeps the old name." });
  hint.style.left = field.style.left;
  hint.style.top = `${rect.bottom - host.top + 4}px`;
  editor.host.append(field, hint);
  state.rename = { field, hint, old, at: found.at };
  field.focus();
  field.select();
}

// Quick Fix: the server's quick fixes and refactorings, after the fixes of orior's own inspections,
// which an inspected tab with no server offers alone.

export async function quickFix() {
  const found = here()?.tab.inspected && !here().tab.served ? here() : served();
  if (!found) {
    return;
  }
  const { editor, s, tab } = found;
  const sel = editor.primary();
  const [start, end] = cmp(sel.anchor, sel.head) <= 0 ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
  let actions;
  try {
    await flush(tab);
    actions = await invoke("lsp_actions", { path: tab.file, from: { line: s.base + start.line, col: start.col }, to: { line: s.base + end.line, col: end.col } });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (!actions.length) {
    say("No quick fixes or refactorings here.");
    return;
  }
  const rect = editor.rectOf(found.head);
  showMenu(
    rect.left,
    rect.bottom + 2,
    actions.map((action) => ({
      label: action.title,
      disabled: Boolean(action.disabled),
      run: async () => {
        // A fix that updates a snapshot runs its line in the terminal, in its build's folder.
        if (action.raw?.orior_run) {
          terminalAt(action.raw.orior_run.folder);
          runInTerminal(action.raw.orior_run.line);
          return;
        }
        try {
          const files = await invoke("lsp_act", { path: tab.file, raw: action.raw });
          await applyEdits(files);
        } catch (error) {
          say(String(error), { failed: true });
        }
        editor.focus();
      },
    })),
  );
}

// Call Hierarchy: the functions that call the one at the cursor, and those it calls, each a tree that
// opens a level at a time, as the language server answers or, where none answers for calls, as orior's
// own index of the tree's functions finds them. A press on a function goes to its call, the first
// where it calls more than once; a press on its arrow, or the list's keys, open and close the level
// under it.

const EMPTY_CALLS = "Call Hierarchy (Ctrl+Alt+H) on a function lists what calls it and what it calls.";

export async function callHierarchy() {
  const found = here();
  if (!found) {
    return;
  }
  let root;
  try {
    await flush(found.tab);
    root = await invoke("calls_root", found.at);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  togglePane(true, { take: false });
  showPane("calls");
  if (!root) {
    state.calls = null;
    drawCalls("No function at the cursor.");
    return;
  }
  const calls = { root, from: found.tab.file, open: new Set(["in", "out"]), children: new Map() };
  state.calls = calls;
  drawCalls(`Reading the calls of ${root.name}…`);
  await Promise.all([loadCalls(calls, "in", root, true), loadCalls(calls, "out", root, false)]);
  if (state.calls === calls) {
    drawCalls();
  }
}

// Reads the level of calls under `key` once.
async function loadCalls(calls, key, call, incoming) {
  if (calls.children.has(key)) {
    return;
  }
  const found = await invoke("calls_of", { path: calls.from, item: call.item, incoming }).catch((error) => {
    say(String(error), { failed: true });
    return [];
  });
  calls.children.set(key, found);
}

async function toggleCalls(key, call, incoming) {
  const calls = state.calls;
  if (calls.open.has(key)) {
    calls.open.delete(key);
  } else {
    calls.open.add(key);
    await loadCalls(calls, key, call, incoming);
  }
  if (state.calls === calls) {
    drawCalls();
    document.querySelector(`#calls-tree [data-key="calls:${CSS.escape(key)}"]`)?.focus();
  }
}

function drawCalls(note) {
  const said = document.getElementById("calls-said");
  const tree = document.getElementById("calls-tree");
  const calls = state.calls;
  if (!calls) {
    said.textContent = note ?? EMPTY_CALLS;
    tree.replaceChildren();
    return;
  }
  said.textContent = note ?? `${calls.root.name}, ${calls.root.path}:${calls.root.from.line + 1}`;
  const rows = [];
  const add = (key, call, depth, incoming, label) => {
    const open = calls.open.has(key);
    const children = calls.children.get(key);
    const where = label ? "" : `${call.path}:${call.from.line + 1}`;
    const row = element("button", { className: "node dir search-file call-row", type: "button", title: label ?? `${call.name}${call.detail ? ` — ${call.detail}` : ""}\n${where}` });
    row.dataset.key = `calls:${key}`;
    row.dataset.depth = String(depth);
    row.setAttribute("aria-expanded", String(open));
    const guides = Array.from({ length: depth }, () => element("i", { className: "guide" }));
    const count = label ? (children?.length ?? "") : call.at.length > 1 ? String(call.at.length) : "";
    row.append(...guides, element("span", { className: "twisty" }), element("span", { className: "name", textContent: label ?? call.name }), element("span", { className: "where", textContent: where }), element("span", { className: "count", textContent: String(count) }));
    row.addEventListener("click", (event) => {
      if (label || !event.isTrusted || event.target.closest(".twisty")) {
        toggleCalls(key, call, incoming);
        return;
      }
      const at = call.at[0] ?? call.from;
      state.hooks.openAt(call.at.length ? call.site : call.path, at.line, at.col);
    });
    rows.push(row);
    if (!open) {
      return;
    }
    if (children && !children.length) {
      const none = element("p", { className: "pane-empty call-none", textContent: incoming ? "Nothing calls it." : "It calls nothing." });
      none.style.paddingLeft = `${1.2 + depth * 0.9}rem`;
      rows.push(none);
    }
    for (const child of children ?? []) {
      add(`${key}/${child.path}:${child.from.line}:${child.name}`, child, depth + 1, incoming);
    }
  };
  add("in", calls.root, 0, true, `Calls to ${calls.root.name}`);
  add("out", calls.root, 0, false, `Calls from ${calls.root.name}`);
  tree.replaceChildren(...rows);
}

// Parameter Info.

function signatureBox(editor) {
  if (!state.signature) {
    const box = element("div", { className: "ed-signature", hidden: true });
    box.addEventListener("mousedown", (event) => event.preventDefault());
    editor.host.append(box);
    state.signature = box;
  }
  return state.signature;
}

export function closeSignature() {
  window.clearTimeout(state.signatureWait);
  state.signatureAsked += 1;
  if (state.signature) {
    state.signature.hidden = true;
  }
}

function signatureOpen() {
  return Boolean(state.signature && !state.signature.hidden);
}

// Asks for the signature at the cursor and shows it, or closes it where the cursor is in no call.
export async function parameterInfo({ quiet = false } = {}) {
  const found = quiet ? here() : served();
  if (!found?.tab.served) {
    return;
  }
  const asked = ++state.signatureAsked;
  await flush(found.tab);
  const signature = await invoke("lsp_signature", found.at).catch(() => null);
  if (asked !== state.signatureAsked || state.hooks.editor()?.s !== found.s) {
    return;
  }
  if (!signature) {
    closeSignature();
    if (!quiet) {
      say("The cursor is not in a call.");
    }
    return;
  }
  const { editor } = found;
  const box = signatureBox(editor);
  const [from, to] = signature.params[signature.active ?? -1] ?? [0, 0];
  const label = signature.label;
  const shown = to > from ? `${escapeHtml(label.slice(0, from))}<b>${escapeHtml(label.slice(from, to))}</b>${escapeHtml(label.slice(to))}` : escapeHtml(label);
  box.innerHTML = `<pre><code>${shown}</code></pre>${signature.doc ? markup(signature.doc) : ""}`;
  box.hidden = false;
  const host = editor.host.getBoundingClientRect();
  const rect = editor.rectOf(editor.head());
  const above = rect.top - host.top - box.offsetHeight - 4;
  box.style.top = `${above >= 0 ? above : rect.bottom - host.top + 4}px`;
  box.style.left = `${Math.max(4, Math.min(rect.left - host.left - 12, host.width - box.offsetWidth - 12))}px`;
}

// After the text of a served tab changes: opens Parameter Info where ( or , was just typed, and
// asks again while it is open.
export function typed() {
  const found = here();
  if (!found?.tab.served) {
    return;
  }
  const before = found.s.doc.line(found.head.line)[found.head.col - 1];
  if (before === "(" || before === "," || signatureOpen()) {
    window.clearTimeout(state.signatureWait);
    state.signatureWait = window.setTimeout(() => parameterInfo({ quiet: true }), SIGNATURE_REST);
  }
}

// The fix mark: shown beside the cursor's line in a served tab where a diagnostic is on it.
function placeFix() {
  if (!state.hooks) {
    return;
  }
  const found = here();
  const editor = state.hooks.editor();
  if (!state.fix && editor) {
    state.fix = element("button", { className: "ed-fix", type: "button", title: "Quick Fix (Ctrl+.)", hidden: true });
    state.fix.innerHTML = BULB;
    state.fix.setAttribute("aria-label", "Quick Fix");
    state.fix.addEventListener("mousedown", (event) => {
      event.preventDefault();
      event.stopPropagation();
      quickFix();
    });
    editor.host.append(state.fix);
    editor.scroller.addEventListener("scroll", placeFix);
  }
  if (!state.fix) {
    return;
  }
  const line = found?.head.line;
  const worst = found?.tab.served || found?.tab.inspected ? Math.min(...(found.s.diagnostics ?? []).filter((one) => one.from.line - found.s.base <= line && one.to.line - found.s.base >= line).map((one) => one.severity)) : Infinity;
  if (!Number.isFinite(worst)) {
    state.fix.hidden = true;
    return;
  }
  const host = editor.host.getBoundingClientRect();
  const gutter = editor.gutter.getBoundingClientRect();
  const rect = editor.rectOf({ line, col: 0 });
  const top = rect.top - host.top;
  state.fix.hidden = top < 0 || rect.bottom > host.bottom;
  state.fix.dataset.severity = String(worst);
  state.fix.style.top = `${top + (rect.bottom - rect.top - 16) / 2}px`;
  state.fix.style.left = `${gutter.right - host.left - 19}px`;
}

// After the cursor moves: the fix mark goes to its line, and Parameter Info follows it while open.
export function moved() {
  placeFix();
  if (signatureOpen()) {
    window.clearTimeout(state.signatureWait);
    state.signatureWait = window.setTimeout(() => parameterInfo({ quiet: true }), SIGNATURE_REST);
  }
}

// Quick Documentation.

export async function quickDoc() {
  const found = here();
  if (!found) {
    return;
  }
  const { editor, s, head } = found;
  const shown = await s.language?.hover?.(s.doc, head);
  if (!shown?.parts?.length || editor.s !== s) {
    say("No documentation for the symbol at the cursor.");
    return;
  }
  editor.hover.show(shown);
}

// `hooks` gives the editor, the tab it shows and every tab, the text of a line of an open file,
// opens a file at a place, tells the tabs and the server of a tab changed here, and reads the
// tree's changes again after files are written.
export function startIntel(hooks) {
  state.hooks = hooks;
  document.getElementById("usages-said").textContent = EMPTY_USAGES;
  listen("lsp-edits", (event) => applyEdits(event.payload));
  listen("lsp-diagnostics", () => window.requestAnimationFrame(placeFix));
  window.addEventListener(
    "keydown",
    (event) => {
      if (event.key === "Escape" && signatureOpen()) {
        closeSignature();
      }
    },
    true,
  );
}
