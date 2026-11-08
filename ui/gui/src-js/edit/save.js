// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { lineChanges } from "../editor/diff.js";
import { inBridge, loadBridge } from "../bridge_panel.js";
import { drawLocalHistory } from "../explorer.js";
import { toolFor } from "../plugins.js";
import { inWindow, jumpPaged, savePaged } from "../paged.js";
import { changed } from "../servers.js";
import { runInTerminal } from "../terminal.js";
import { markNotebookSaved, notebookFile } from "../notebook.js";
import { writeWithCare } from "../rights.js";
import { say } from "../statusbar.js";
import { state } from "../edit.js";
import { problemsChanged, tidy } from "./crumbs.js";
import { drawDefs } from "./defs.js";
import { forgetHeads, markChanges } from "./mirror.js";
import { openFile, placeKey } from "./open.js";
import { forgetBackup, markPlace } from "./places.js";
import { actingPath, dirty, drawTabs, markSaved, tabOf } from "./tabs.js";
import { drawTree, loadChanges } from "./tree.js";

export async function saveActive(tab = tabOf(actingPath())) {
  if (tab?.notebook) {
    await invoke("notebook_write", { path: tab.path, book: notebookFile(tab.notebook) });
    markNotebookSaved(tab.notebook);
    tab.closing = false;
    drawTabs();
    await loadChanges();
    drawTree();
    return;
  }
  if (!tab?.session || tab.readOnly) {
    return;
  }
  // A paged file is written on the tree's side, with every edit made to it.
  if (tab.session.window?.paged) {
    const writing = tab.session.doc.id;
    await savePaged(tab.session);
    tab.saved = writing;
    tab.closing = false;
    drawTabs();
    await loadChanges();
    drawTree();
    return;
  }
  if (saving("format")) {
    await formatTab(tab, { saving: true });
  }
  tidy(tab);
  const writing = tab.session.doc.id;
  const lines = tab.session.doc.lines.slice();
  const eol = tab.session.doc.eol;
  const written = tab.session.doc.text();
  await writeWithCare(tab.path, written);
  tab.saved = writing;
  tab.savedLines = lines;
  tab.savedEol = eol;
  tab.comparedAt = null;
  tab.closing = false;
  forgetBackup(tab.path);
  drawTabs();
  if (inBridge(tab.path)) {
    loadBridge().then(drawDefs);
  }
  drawLocalHistory(tab.file, true);
  await loadChanges();
  drawTree();
}

// Opens a file with the cursor on a line and column of it, each counted from the file's first. A
// line a file still being read has not reached yet is waited for; a file not open yet opens around
// the line. Where `preview`, a tab it opens is a preview, as openFile makes one.
export async function openAt(path, line, col = 0, { preview = false } = {}) {
  if (state.elsewhere) {
    state.elsewhere(path, line, col);
    return;
  }
  if (!tabOf(path)) {
    localStorage.setItem(placeKey(path), JSON.stringify({ line, col }));
  }
  markPlace();
  state.moving = true;
  try {
    await openFile(path, { preview });
  } finally {
    state.moving = false;
  }
  const tab = tabOf(path);
  const s = tab?.session;
  if (!s) {
    return;
  }
  if (s.window?.paged && !inWindow(s, line)) {
    await jumpPaged(s, line);
  }
  if (state.split?.focused && state.split.active === path) {
    state.split.editor.goTo(line - s.base, col);
  } else if (state.active === path) {
    state.editor.goTo(line - s.base, col);
  }
}

// Saving: on its own a moment after typing rests where Auto Save is on, and each file saved with
// the spaces and tabs at its lines' ends taken off and a line end after its last line where those
// are on. A Markdown file keeps its lines' ends, where two spaces break a line. Where Format on Save
// is on, a file is formatted first, on every save but Auto Save's, which comes while the reader is
// still at work in it.

export const AUTO_SAVE_REST = 1000;
const SAVING = {
  "auto-save": ["orior.autosave", false],
  trim: ["orior.trim", false],
  "final-newline": ["orior.final-newline", false],
  format: ["orior.format-on-save", false],
};

export function saving(name) {
  const [key, fallback] = SAVING[name];
  const kept = localStorage.getItem(key);
  return kept === null ? fallback : kept === "true";
}

export function setSaving(name, on = !saving(name)) {
  localStorage.setItem(SAVING[name][0], String(on));
}

// Formatting: Edit, Format Document, and saving where Format on Save is on. The tab's text goes to the
// formatter its language has, as format.rs in the command line's crate runs it, and comes back as
// one edit for each run of lines that changed, which one undo takes back. A text changed while the
// formatter worked is left as it is. A save says nothing of a language no formatter knows.

let formattable = null;

// The edit for a run of `doc`'s lines [from, to) written again as `lines`.
function linesEdit(doc, from, to, lines) {
  if (to < doc.count) {
    return { from: { line: from, col: 0 }, to: { line: to, col: 0 }, text: lines.map((line) => `${line}\n`).join("") };
  }
  if (lines.length) {
    return from < doc.count ? { from: { line: from, col: 0 }, to: doc.end(), text: lines.join("\n") } : { from: doc.end(), to: doc.end(), text: `\n${lines.join("\n")}` };
  }
  return from === 0 ? { from: { line: 0, col: 0 }, to: doc.end(), text: "" } : { from: { line: from - 1, col: doc.line(from - 1).length }, to: doc.end(), text: "" };
}

export async function formatTab(tab, { saving: onSave = false } = {}) {
  const s = tab?.session;
  if (!s || s.readOnly || s.window) {
    return;
  }
  formattable ??= new Set(await invoke("format_languages").catch(() => []));
  const language = s.language?.id ?? "plaintext";
  if (!formattable.has(language)) {
    if (!onSave) {
      say(`No formatter formats ${s.language?.name ?? "plain text"}: File, Toolchains lists those there are.`);
    }
    return;
  }
  const doc = s.doc;
  const version = doc.id;
  const before = doc.lines.join("\n");
  let formatted;
  try {
    formatted = await invoke("format_text", { path: tab.path, language, text: before });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (doc.id !== version) {
    say("The file changed while it was formatted, and was left as it is.");
    return;
  }
  const lines = formatted.replace(/\r\n?/g, "\n").split("\n");
  if (lines.join("\n") === before) {
    if (!onSave) {
      say("Already formatted.");
    }
    return;
  }
  const places = replaceLines(s, lines, "format");
  if (!onSave) {
    say(`Formatted ${places} ${places === 1 ? "place" : "places"}.`);
  }
}

// Puts `lines` in place of a session's own as one change of `type`, which undo takes back, only the
// lines that differ replaced. Says in how many places the text changed.
function replaceLines(s, lines, type) {
  const doc = s.doc;
  const changes = lineChanges(doc.lines, lines);
  const edits = changes ? changes.hunks.map((hunk) => linesEdit(doc, hunk.then[0], hunk.then[1], lines.slice(hunk.now[0], hunk.now[1]))) : [{ from: { line: 0, col: 0 }, to: doc.end(), text: lines.join("\n") }];
  if (state.editor?.s === s) {
    state.editor.change(edits, type);
  } else {
    doc.change(edits, type, s.selections);
    s.selections = s.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
    doc.settle(s.selections);
  }
  return changes ? changes.hunks.length : 1;
}

// Changes made on the disk outside the window, as the watcher tells of them a batch at a time: each
// folder whose entries changed is read again, a tab with no changes of its own takes its file's new
// text, and git's view of the tree is read again where it may have changed. Batches are taken one
// after another.
export function treeChanged({ payload: changed }) {
  state.watched = state.watched.then(async () => {
    if (changed.lost) {
      state.children.clear();
    }
    // Only a folder the tree has read is shown, and only one shown draws the tree again.
    const shown = changed.folders.filter((folder) => state.children.delete(folder));
    const files = changed.lost ? state.tabs.map((tab) => tab.path) : changed.files;
    for (const tab of files.map(tabOf).filter((tab) => tab && !tab.commit)) {
      await reloadTab(tab);
    }
    if (changed.git) {
      forgetHeads();
      state.repos = null;
      markChanges(tabOf(state.active));
      await loadChanges();
    }
    if (shown.length || changed.git || changed.lost) {
      await drawTree();
    }
  });
}

// A tab's file as the disk holds it now, taken in where the tab has no changes of its own: the lines
// that differ replaced as one change, which undo takes back, and the tab marked saved at it. A tab
// with changes of its own keeps them, and the status bar says the file changed; a file read as a
// window, or not read as text, is left as it is.
export async function reloadTab(tab) {
  const s = tab.session;
  if (!s || s.window || tab.readOnly) {
    return;
  }
  let opened;
  try {
    opened = await invoke("file_read", { path: tab.path });
  } catch {
    return;
  }
  if (typeof opened.text !== "string") {
    return;
  }
  const lines = opened.text.split(/\r?\n/);
  if (lines.length === s.doc.lines.length && lines.every((line, at) => line === s.doc.lines[at])) {
    return;
  }
  if (dirty(tab)) {
    say(`${tab.file} changed on the disk, and its tab keeps the changes made here.`);
    return;
  }
  replaceLines(s, lines, "reload");
  markSaved(tab, opened.text);
  changed(tab);
  drawTabs();
}

// Run, Run File: the tab's file saved where it has changes, then run in the terminal with the
// toolchain its language has, by the line run_file.rs in the command line's crate gives.
export async function runTab(tab) {
  const s = tab?.session;
  if (!s || s.window) {
    return;
  }
  if (dirty(tab) && !tab.readOnly) {
    state.active = tab.path;
    await saveActive();
  }
  try {
    const run = await invoke("run_file_line", { path: tab.path, language: s.language?.id ?? "plaintext" });
    runInTerminal(run.line);
    say(`Running ${tab.path.split("/").pop()} with ${run.tool}.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Validate: the tab's file saved where it has changes, then checked by the tool plugin for its
// language, as validate.rs in the command line's crate checks it. Its findings are drawn under the
// text as a language server's diagnostics are, until the text changes, and its verdict is said.
export async function validateTab(tab) {
  const s = tab?.session;
  if (!s || s.window) {
    return;
  }
  const language = s.language?.id ?? "plaintext";
  const tool = toolFor(language);
  const name = tab.file.split("/").pop();
  if (!tool) {
    say(`No tool plugin validates ${s.language?.name ?? "plain text"}: File, Plugins lists them.`);
    return;
  }
  if (dirty(tab) && !tab.readOnly) {
    state.active = tab.path;
    await saveActive();
  }
  say(`Validating ${name} with ${tool.name}…`);
  try {
    const report = await invoke("validate_file", { path: tab.file, language });
    s.diagnostics = report.findings.map((finding) => ({
      from: { line: finding.line, col: finding.col },
      to: { line: finding.end_line, col: finding.end_col },
      severity: finding.severity,
      message: finding.lifted ? `${finding.message}

${finding.lifted}` : finding.message,
      source: finding.kind,
    }));
    tab.validated = true;
    problemsChanged();
    state.editor.schedule();
    say(`${name} ${report.passes ? "passes" : "fails"}: ${report.verdict}`, { failed: !report.passes });
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Verify Disassembly: the tab's C module saved where it has changes, then the command line's
// loop over it run in the terminal, by the line the tree's side gives: the tree built with link-time
// optimization, the module's functions read by the disassembler and by Ghidra, and the two readings
// kept the same. A file that is no C module is said to be none.
export async function verifyDisassemblyTab(tab) {
  const s = tab?.session;
  if (!s || s.window) {
    return;
  }
  const name = tab.file.split("/").pop();
  if (s.language?.id !== "c") {
    say(`Verify Disassembly reads a C module: ${name} is ${s.language?.name ?? "plain text"}.`);
    return;
  }
  if (dirty(tab) && !tab.readOnly) {
    state.active = tab.path;
    await saveActive();
  }
  try {
    const line = await invoke("disasm_verify_line", { path: tab.path });
    runInTerminal(line);
    say(`Verifying ${name}'s disassembly in the terminal.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

