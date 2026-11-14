// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { changed, stopTelling } from "../servers.js";
import { changeSignature } from "../signature.js";
import { moveDeclaration } from "../move.js";
import { shapeSearch } from "../shapes.js";
import { say } from "../statusbar.js";
import { state } from "../edit.js";
import { problemsChanged } from "./crumbs.js";
import { load } from "./open.js";
import { openAt } from "./save.js";
import { drawTabs, tabOf, tabOfSession } from "./tabs.js";

// Refactorings across files: each file's edits written into the tab that holds it, a tab opened for
// a file none holds, every file's as one step of one type, `group:` and a number, which ties them:
// Undo or Redo in any of them takes the step back or brings it again in all of them.
let groups = 0;

async function applyGroup(files) {
  groups += 1;
  const type = `group:${groups}`;
  for (const file of files.filter((one) => one.edits.length)) {
    if (!tabOf(file.path)) {
      await load(file.path).catch(() => {});
    }
    const tab = tabOf(file.path);
    const s = tab?.session;
    if (!s || s.window || s.readOnly) {
      say(`${file.path} is not open whole, or cannot be written, and was not changed.`, { failed: true });
      continue;
    }
    const edits = file.edits.map(({ from, to, text }) => ({ from: { line: from.line - s.base, col: from.col }, to: { line: to.line - s.base, col: to.col }, text }));
    if (state.editor?.s === s) {
      state.editor.change(edits, type);
    } else {
      s.doc.change(edits, type, s.selections);
      s.selections = s.selections.map((sel) => ({ anchor: s.doc.clamp(sel.anchor), head: s.doc.clamp(sel.head), goal: null }));
      s.doc.settle(s.selections);
    }
    s.doc.seal();
    changed(tab);
  }
  drawTabs();
}

// Takes back, or brings again, the step of `type` in every tab but the one whose editor already does.
export function undoGroup(type, back, own) {
  for (const tab of state.tabs) {
    const doc = tab.session?.doc;
    if (!doc || doc === own) {
      continue;
    }
    const step = back ? doc.steps.get(doc.id) : doc.steps.get(doc.next.get(doc.id));
    if (step?.type !== type) {
      continue;
    }
    doc.writer = tab.session;
    const found = back ? doc.undo() : doc.redo();
    doc.writer = null;
    if (found?.selections) {
      tab.session.selections = found.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
    }
    changed(tab);
  }
  drawTabs();
}

// Change Signature on the file in the editor last pressed in.
export function changeSignatureHere(sheet) {
  const editor = state.split?.focused ? state.split.editor : state.editor;
  const tab = editor?.s ? tabOfSession(editor.s) : null;
  if (!tab) {
    return null;
  }
  return changeSignature({
    editor,
    path: tab.file,
    sheet,
    search: async (name) => [...new Set((await invoke("tree_search", { query: name, how: { case: true, word: true, regex: false } }).catch(() => [])).map((hit) => hit.path))],
    textOf: async (path) => tabOf(path)?.session?.doc.text() ?? (await invoke("file_read", { path }).catch(() => null))?.text ?? null,
    apply: applyGroup,
  });
}

// Write Docstring: a docstring drawn up for the function the cursor is in, from its parameters, what
// it returns and what it raises, in the form chosen for Python and as JSDoc for JavaScript, the cursor
// left on its summary line; and Sort Methods by Name, the methods of the class the cursor is in sorted
// by name. Each is one change undo takes back.
const DOCSTRING_FORM = "orior.docstring-form";

export function docstringForm() {
  return localStorage.getItem(DOCSTRING_FORM) ?? "google";
}

export function setDocstringForm(form) {
  if (!["google", "numpy", "rest", "plain"].includes(form)) {
    say(`${form} is no docstring form: google, numpy, rest or plain.`, { failed: true });
    return;
  }
  localStorage.setItem(DOCSTRING_FORM, form);
  say(`Docstrings are drawn up in the ${form} form.`);
}

function codeHere() {
  const editor = state.split?.focused ? state.split.editor : state.editor;
  const s = editor?.s;
  if (!s || s.window || s.readOnly || !s.language) {
    say("This reads the code of a file open whole in a language.");
    return null;
  }
  return { editor, s, line: editor.primary().head.line };
}

export async function writeDocstring() {
  const found = codeHere();
  if (!found) {
    return;
  }
  const { editor, s, line } = found;
  try {
    const [edit, summary] = await invoke("code_docstring", { language: s.language.id, text: s.doc.text(), line, form: docstringForm() });
    editor.change([edit], "docstring");
    s.doc.seal();
    editor.select([{ anchor: summary, head: summary, goal: null }]);
    editor.focus();
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Fill Paragraph: the paragraph of documentation the cursor is in filled to the documentation's
// margin, or the code's where none is set apart, as fill.rs in the command line's crate fills it.
export async function fillParagraph() {
  const found = codeHere();
  if (!found) {
    return;
  }
  const { editor, s, line } = found;
  const width = editor.docMargin ?? s.margin ?? 79;
  try {
    const edit = await invoke("fill_paragraph", { text: s.doc.text(), line, width });
    editor.change([edit], "fill");
    s.doc.seal();
  } catch (error) {
    say(String(error), { failed: true });
  }
}

export async function sortMethods() {
  const found = codeHere();
  if (!found) {
    return;
  }
  const { editor, s, line } = found;
  try {
    const edit = await invoke("code_sort", { language: s.language.id, text: s.doc.text(), line });
    editor.change([edit], "sort");
    s.doc.seal();
    say("The class's methods are sorted by name.");
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Search Structurally on the file in the editor last pressed in, and the tree's files in its language.
export function shapeSearchHere(sheet) {
  const editor = state.split?.focused ? state.split.editor : state.editor;
  const tab = editor?.s ? tabOfSession(editor.s) : null;
  const language = editor?.s?.language;
  if (!tab || !language) {
    say("Search Structurally reads the code of a file open in a language.");
    return;
  }
  shapeSearch({ sheet, language: language.id, languageName: language.name ?? language.id, path: tab.file, open: (path, line, col) => openAt(path, line, col), apply: applyGroup });
}

// Move on the file in the editor last pressed in.
export function moveHere(ask) {
  const editor = state.split?.focused ? state.split.editor : state.editor;
  const tab = editor?.s ? tabOfSession(editor.s) : null;
  if (!tab) {
    return null;
  }
  return moveDeclaration({
    editor,
    path: tab.file,
    ask,
    textOf: async (path) => tabOf(path)?.session?.doc.text() ?? (await invoke("file_read", { path }).catch(() => null))?.text ?? null,
    make: (path) => invoke("file_create", { path }),
    apply: applyGroup,
  });
}

// Holds the app's memory to `budget` bytes: the servers no open tab needs stop, and where that is not
// enough, the one that holds the max and does not serve the tab displayed. The tabs a stopped server had
// open lose their diagnostics and are passed to a server again as each shows.
export async function holdMemory(budget) {
  const open = [...new Set(state.tabs.map((tab) => tab.session?.language?.id).filter(Boolean))];
  const displayed = tabOf(state.active)?.session?.language?.id ?? null;
  const answer = await invoke("servers_unneeded_stop", { open, displayed, budget }).catch(() => null);
  if (!answer?.stopped.length) {
    return;
  }
  const root = document.getElementById("tree-path").textContent;
  const plain = (path) => path.replace(/\\/g, "/").toLowerCase();
  const files = new Set(answer.files.map(plain));
  for (const tab of state.tabs) {
    if (tab.served && files.has(plain(`${root}/${tab.file}`))) {
      Object.assign(tab, { served: undefined, untold: false, unserved: true });
      stopTelling(tab);
      if (tab.session) {
        tab.session.diagnostics = null;
      }
    }
  }
  problemsChanged();
  say(`${answer.stopped.join(", ")} stopped to hold memory to its budget; ${answer.files.length ? "its files are passed to it again as each shows" : "no open tab needed it"}.`);
}

