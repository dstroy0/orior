// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, pick } from "../bridge.js";
import { showCreate, showInit } from "../create.js";
import { say } from "../statusbar.js";
import { addToWindow, editing, openAt as openFileAt, openFile } from "../edit.js";
import { showPane } from "../explorer.js";
import { openPalette } from "../palette.js";
import { focusSearch } from "../search.js";
import { clipText, copyText } from "../menu.js";
import { togglePane } from "../sides.js";
import { showView } from "../views.js";
import { state } from "../menubar.js";
import { sheet } from "./sheets.js";

// Takes the selections' text, or the cursors' whole lines, out of the editor for the clipboard, as
// its keys do, and out of the text where `cut`: put on the clipboard through the window's own, which
// needs no press of the reader's.
export function clipOut(editor, cut) {
  const text = editor.takeOut(cut);
  editor.note({ cut });
  return copyText(text.replace(/\n/g, editor.doc.eol));
}

export async function pasted(editor) {
  const text = await clipText();
  editor.focus();
  editor.paste({ preventDefault() {}, clipboardData: { getData: () => text } });
}

// The run view's search, holding `word`.
export function searchJobs(word = "") {
  showView("run");
  const filter = document.getElementById("job-filter");
  filter.value = word;
  filter.dispatchEvent(new Event("input"));
  filter.focus();
}

// The file a path names, at the line and column after its colons, or the quick open where none is
// named.
export function goToFile(args = []) {
  const found = (args[0] ?? "").match(/^(.*?)(?::(\d+))?(?::(\d+))?$/);
  if (!found[1]) {
    openPalette("");
    return;
  }
  showView("edit");
  openFileAt(found[1], Math.max(0, Number(found[2] || 1) - 1), Math.max(0, Number(found[3] || 1) - 1));
}

// Find in Files: the explorer's Search pane, holding the words given where there are any.
export function findInFiles(args = []) {
  showView("edit");
  togglePane(true);
  showPane("search");
  focusSearch(args.length ? args.join(" ") : undefined);
}

// The bridge's key in Lstar.klq, or the file itself.
export async function goToBridge(args = []) {
  const read = await invoke("bridge_read").catch(() => null);
  const klq = read?.klq;
  if (!klq) {
    return;
  }
  showView("edit");
  openFileAt(klq.path, klq.keys?.[args[0]]?.line ?? 0);
}

// File, Create, File and Folder: the path the command line gave, made at once, or one asked for. A
// file made opens.
export async function create(folder, given) {
  const made = async (path) => {
    await editing().treeChanged();
    if (!folder) {
      showView("edit");
      await openFile(path);
    }
  };
  if (!given) {
    showCreate(sheet, { folder, start: editing().folderHere(), made });
    return;
  }
  try {
    await invoke(folder ? "folder_create" : "file_create", { path: given });
    await made(given);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// File, Create, Repository: the open tree made a repository, or a folder chosen.
export function createRepository(given) {
  showInit(sheet, {
    given,
    made: async (folder, here) => {
      if (here) {
        await editing().treeChanged();
      }
      say(`${folder} is a repository now.`);
    },
  });
}

// File, Open, File: a file of the tree, the one the command line named or one the picker gives.
export async function openAnyFile(given) {
  const chosen = given ?? (await pick("file"));
  if (typeof chosen !== "string") {
    return;
  }
  let path = (await invoke("tree_relative", { path: chosen }).catch(() => null)) ?? (/^[a-zA-Z]:|^\//.test(chosen) ? null : chosen);
  // A file from outside the tree opens beside it, mounted on its own.
  if (!path) {
    try {
      path = `@${await addToWindow(chosen)}`;
    } catch (error) {
      say(String(error), { failed: true });
      return;
    }
  }
  showView("edit");
  await openFile(path);
}

// File, Open, Workspace: a .code-workspace file opened as a window that holds its folders, the first
// of them as the tree and the rest mounted beside it.
export async function openWorkspace(given) {
  const chosen = given ?? (await pick("file"));
  if (typeof chosen !== "string") {
    return;
  }
  let folders;
  try {
    folders = await invoke("workspace_read", { path: chosen });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const [tree, ...rest] = folders;
  if (!tree) {
    say(`${chosen} names no folder.`, { failed: true });
    return;
  }
  await state.openFolder(tree);
  for (const folder of rest) {
    await addToWindow(folder).catch((error) => say(String(error), { failed: true }));
  }
}

