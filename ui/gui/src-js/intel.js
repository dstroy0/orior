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

import { asked, invoke, listen } from "./bridge.js";
import { cmp, wordAt } from "./editor/document.js";
import { iconOf, showPane } from "./explorer.js";
import { SIGNATURE_REST, closeSignature, parameterInfo, signatureOpen } from "./intel/signature.js";
import { element, here, served, state } from "./intel/state.js";
import { showMenu } from "./menu.js";
import { focusSearch } from "./search.js";
import { flush } from "./servers.js";
import { togglePane } from "./sides.js";
import { say } from "./statusbar.js";
import { runInTerminal, terminalAt } from "./terminal.js";

export { callHierarchy } from "./intel/calls.js";
export { closeSignature, parameterInfo, typed } from "./intel/signature.js";

const EMPTY_USAGES = "Find Usages (Shift+F12) on a symbol lists each place it is used.";

// The fix mark: a bulb, drawn in the color of the line's worst diagnostic.
const BULB = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 1.6a4.6 4.6 0 0 0-2.7 8.3c.5.4.8 1 .8 1.6v.5h3.8v-.5c0-.6.3-1.2.8-1.6A4.6 4.6 0 0 0 8 1.6Z" fill="currentColor" fill-opacity=".22" stroke="currentColor" stroke-width="1.2"/><path d="M6.2 13.6h3.6M6.7 15h2.6" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"/></svg>';

// The places the symbol at the cursor is defined at, used at, or called from, as `kind` asks,
// "definition", "usages" or "callers": each its file, its line and column from 0, and the text of the
// line where the server gives it, with the name asked about. Null after saying why there is nothing
// to ask.
export async function placesHere(kind) {
  const found = served();
  if (!found) {
    return null;
  }
  const word = wordAt(found.s.doc, found.head);
  const name = word?.text ?? "the symbol";
  await flush(found.tab);
  if (kind === "definition") {
    const places = (await asked("lsp_definition", found.at).catch(() => [])) ?? [];
    return { name, places: places.map((one) => ({ path: one.path, line: one.line, col: one.col, text: "" })) };
  }
  if (kind === "usages") {
    const usages = (await asked("lsp_references", found.at, 61000)) ?? [];
    return { name, places: usages.map((one) => ({ path: one.path, line: one.from.line, col: one.from.col, text: one.text })) };
  }
  const root = await asked("calls_root", found.at);
  if (!root) {
    return { name, places: [] };
  }
  const calls = (await asked("calls_of", { path: found.tab.file, item: root.item, incoming: true })) ?? [];
  return {
    name: root.name,
    places: calls.flatMap((call) => (call.at.length ? call.at : [call.from]).map((at) => ({ path: call.path, line: at.line, col: at.col, text: call.site, caller: call.name }))),
  };
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
  const turn = ++state.asked;
  state.name = word?.text ?? "the symbol";
  togglePane(true, { take: false });
  showPane("usages");
  document.getElementById("usages-said").textContent = `Finding usages of ${state.name}…`;
  document.getElementById("usages-hits").replaceChildren();
  let usages;
  try {
    await flush(found.tab);
    usages = (await asked("lsp_references", found.at, 61000)) ?? [];
  } catch (error) {
    if (turn === state.asked) {
      document.getElementById("usages-said").textContent = String(error);
    }
    return;
  }
  if (turn !== state.asked) {
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
    const files = await asked("lsp_rename", { ...box.at, name }, 61000);
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
    span = await asked("lsp_renamable", found.at);
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
    actions = await asked("lsp_actions", { path: tab.file, from: { line: s.base + start.line, col: start.col }, to: { line: s.base + end.line, col: end.col } });
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
    actions.map((action) => ({ label: action.title, disabled: Boolean(action.disabled), command: "quick-fix-apply", args: [action.title], run: () => applyAction(found, action) })),
  );
}

// Applies a quick fix or refactoring: its edits, or, for a fix that updates a snapshot, its line run in
// the terminal, in its build's folder.
async function applyAction({ editor, tab }, action) {
  if (action.raw?.orior_run) {
    terminalAt(action.raw.orior_run.folder);
    runInTerminal(action.raw.orior_run.line);
    return;
  }
  try {
    const files = await asked("lsp_act", { path: tab.file, raw: action.raw });
    await applyEdits(files);
  } catch (error) {
    say(String(error), { failed: true });
  }
  editor.focus();
}

// The quick fixes' list as a command, by the name commands.json gives it: the fix or refactoring of
// that title at the cursor or the selection, asked of the server again and applied.
export const intelCommands = {
  "quick-fix-apply": async (words) => {
    const title = words.join(" ");
    const found = here()?.tab.inspected && !here().tab.served ? here() : served();
    if (!found) {
      return;
    }
    const { s, tab, editor } = found;
    const sel = editor.primary();
    const [start, end] = cmp(sel.anchor, sel.head) <= 0 ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
    await flush(tab);
    const actions = await asked("lsp_actions", { path: tab.file, from: { line: s.base + start.line, col: start.col }, to: { line: s.base + end.line, col: end.col } }).catch(() => []);
    const action = actions.find((one) => one.title === title);
    return action ? applyAction(found, action) : say(`No quick fix here is titled ${title}.`, { failed: true });
  },
};


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
