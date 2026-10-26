// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { closeDiff, showDiff } from "../diffview.js";
import { showMerge } from "../mergeview.js";
import { anchor } from "../review.js";
import { clipText } from "../menu.js";
import { isNotebook, showNotebookMerge } from "../notebook.js";
import { say } from "../statusbar.js";
import { state } from "../edit.js";
import { openFile } from "./open.js";
import { forgetBackup } from "./places.js";
import { openAt, reloadTab } from "./save.js";
import { drawTabs, markSaved, tabOf } from "./tabs.js";
import { loadChanges } from "./tree.js";

// A file's changes from the last commit side by side over the editor: its text as the open tab
// holds it, or as the tree does where no tab holds it.
export async function openDiff(path, mark) {
  const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
  const then = mark === "U" || mark === "A" ? null : await invoke("file_head", { path }).catch(() => null);
  let now = "";
  if (tab) {
    now = tab.session.doc.text();
  } else if (mark !== "D") {
    now = (await invoke("file_read", { path }).catch(() => null))?.text ?? "";
  }
  showDiff(document.querySelector("#mode-edit .desk"), path, then, now, { review: path });
}

// Compare: two texts side by side in the changes view, `sides` naming them, the left first, and the
// review comments of `review`, a file of the tree, under the right side's lines.
function compareTexts(name, then, now, sides, review = name) {
  showDiff(document.querySelector("#mode-edit .desk"), name, then, now, { sides, same: "The two are the same.", review });
}

const shortId = (commit) => commit.id.slice(0, 7);

// A commit as a diff's bar names it: its id, and who made it, as a change is answered for where it
// is reviewed, whether authors show elsewhere.
const madeBy = (commit) => `${shortId(commit)}${commit.author ? ` by ${commit.author}` : ""}`;

// A revision of a file beside the file as it stands, or beside another revision, the older left.
export async function compareCommit(path, commit, other = null) {
  try {
    if (!other) {
      compareTexts(path, await invoke("file_at", { path, id: commit.id }), await textNow(path), `${madeBy(commit)}, then as it stands`);
      return;
    }
    const [older, newer] = commit.when <= other.when ? [commit, other] : [other, commit];
    compareTexts(path, await invoke("file_at", { path, id: older.id }), await invoke("file_at", { path, id: newer.id }), `${madeBy(older)}, then ${madeBy(newer)}`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// A file as a commit left it beside the file as the commit's first parent left it, the parent left.
// A file the commit added has nothing on the left, and one it deleted nothing on the right.
export async function openTouched(commit, file) {
  const parent = commit.parents[0];
  const then = file.state === "A" || !parent ? null : await invoke("file_at", { path: file.was ?? file.path, id: parent }).catch(() => null);
  const now = file.state === "D" ? "" : await invoke("file_at", { path: file.path, id: commit.id }).catch(() => "");
  compareTexts(file.path, then, now, `${parent ? parent.slice(0, 7) : "nothing"}, then ${madeBy(commit)}`);
}

// A review comment's file, open at the line the comment stands at in its text as it is.
export async function openComment(comment) {
  await openFile(comment.path);
  const lines = tabOf(state.active)?.session?.doc.lines ?? [];
  await openAt(comment.path, anchor(comment, lines).line);
}

// The file open beside the text on the clipboard.
export async function compareWithClipboard() {
  const tab = tabOf(state.active);
  if (tab?.session) {
    compareTexts(tab.file, await clipText(), tab.session.doc.text(), "the clipboard, then the file");
  }
}

// One file of the tree beside another, the one chosen first on the left.
export async function compareFiles(first, second) {
  compareTexts(`${first} · ${second}`, await textNow(first), await textNow(second), `${first}, then ${second}`, second);
}

// A file a merge left in conflict, in the merge window over the editor: once every conflict is
// settled, the result is written to the file, the file staged as resolved, and an open tab of it
// takes the result.
export async function openMerge(path) {
  if (isNotebook(path)) {
    const merged = await invoke("notebook_merge", { path }).catch((error) => (say(String(error), { failed: true }), null));
    if (merged) {
      showNotebookMerge(document.querySelector("#mode-edit .desk"), path, merged, {
        resolved: async (book) => {
          try {
            await invoke("notebook_write", { path, book });
            await invoke("git_resolve", { path });
            say(`${path} is resolved.`);
          } catch (error) {
            say(String(error), { failed: true });
          }
          await loadChanges();
        },
      });
    }
    return;
  }
  const text = await textNow(path);
  showMerge(document.querySelector("#mode-edit .desk"), path, text, {
    resolved: async (result) => {
      try {
        await invoke("file_write", { path, text: result });
        await invoke("git_resolve", { path });
        say(`${path} is resolved.`);
      } catch (error) {
        say(String(error), { failed: true });
      }
      const tab = tabOf(path);
      if (tab) {
        await reloadTab(tab);
      }
      await loadChanges();
    },
  });
}

// The files a merge left in conflict.
export function conflicted() {
  return [...state.changes].filter(([, mark]) => mark === "C").map(([path]) => path);
}

// Gives each open tab of `paths` its file's text as the tree holds it now, as one step undo takes
// back, after the Commit window rolled the files back.
export async function reloadFromDisk(paths) {
  for (const path of paths) {
    const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
    if (!tab) {
      continue;
    }
    const opened = await invoke("file_read", { path }).catch(() => null);
    if (typeof opened?.text !== "string") {
      continue;
    }
    if (replaceText(tab, opened.text, "rollback")) {
      markSaved(tab);
      forgetBackup(tab.path);
    }
  }
  drawTabs();
}

// Gives a tab `text` in place of all it holds, as one step undo takes back, and says whether that
// changed it.
function replaceText(tab, text, why) {
  const doc = tab.session.doc;
  const fresh = text.replace(/\r\n/g, "\n");
  if (doc.text() === fresh) {
    return false;
  }
  const edit = [{ from: { line: 0, col: 0 }, to: doc.end(), text: fresh }];
  if (state.editor.s === tab.session) {
    state.editor.change(edit, why);
  } else {
    doc.change(edit, why, tab.session.selections);
    tab.session.selections = tab.session.selections.map((sel) => ({ anchor: doc.clamp(sel.anchor), head: doc.clamp(sel.head), goal: null }));
    doc.settle(tab.session.selections);
  }
  return true;
}

// The text of a file as it stands: its open tab's, or the tree's where no tab holds it.
export async function textNow(path) {
  const tab = state.tabs.find((one) => one.file === path && !one.commit && one.session && !one.session.window);
  return tab ? tab.session.doc.text() : ((await invoke("file_read", { path }).catch(() => null))?.text ?? "");
}

// Shows a file's Local History snapshot kept at `at` beside the file as it stands.
export async function showSnapshot(path, at) {
  const then = await invoke("history_read", { path, at });
  const date = new Date(at);
  const when = `${date.toLocaleDateString()} ${date.toLocaleTimeString()}`;
  showDiff(document.querySelector("#mode-edit .desk"), path, then, await textNow(path), { sides: `saved ${when}, then as it stands`, same: `No change from the save at ${when}.` });
}

// Takes a file back to its Local History snapshot kept at `at`, in its tab as one step undo takes
// back, the tab opened for it where none is.
export async function revertSnapshot(path, at) {
  const then = await invoke("history_read", { path, at });
  if (!tabOf(path)) {
    await openFile(path);
  }
  const tab = tabOf(path);
  if (tab?.session && !tab.readOnly) {
    closeDiff();
    replaceText(tab, then, "revert");
    drawTabs();
  }
}

