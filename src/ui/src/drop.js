// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Files dropped on the window from outside it. Each is copied into the tree behind the window
// (files.rs): into the folder of the tree's pane it is dropped on, the folder of a file it is dropped
// on, or the tree's top anywhere else. The folder it would go into is lit while it is held over the
// window. Each file being copied has a bar on the status bar of how far along it is, the window
// works on meanwhile, and nothing dropped is opened until it is asked for.

import { invoke, listen } from "./bridge.js";
import { write } from "./status.js";
import { say } from "./statusbar.js";

const state = { changed: () => {}, lit: null };

// The folder of the tree a drop at (`x`, `y`), in the page's pixels, goes into: the folder row there,
// the folder of the file row there, or the tree's top, "".
export function folderAt(x, y) {
  const row = document.elementFromPoint(x, y)?.closest("#files .node[data-key]");
  if (!row) {
    return "";
  }
  const key = row.dataset.key;
  if (row.classList.contains("dir") && row.dataset.archive !== "true") {
    return key;
  }
  return key.includes("/") ? key.slice(0, key.lastIndexOf("/")) : "";
}

// A point the window's events give, in the system's pixels, in the page's.
const pageOf = (position) => ({ x: (position?.x ?? -1) / devicePixelRatio, y: (position?.y ?? -1) / devicePixelRatio });

// Lights the row of the folder `into`, or none.
function light(into) {
  const row = into === null || into === "" ? null : document.querySelector(`#files .node[data-key="${CSS.escape(into)}"]`);
  if (row !== state.lit) {
    state.lit?.classList.remove("drop-into");
    row?.classList.add("drop-into");
    state.lit = row;
  }
}

// Copies the files and folders at `paths`, outside the tree, into its folder `into`.
export async function bring(paths, into) {
  light(null);
  if (paths.length) {
    await invoke("files_bring", { into, paths });
  }
}

const listOf = (names) => (names.length > 1 ? `${names.slice(0, -1).join(", ")} and ${names.at(-1)}` : names[0]);

// `changed` is called once a copy has ended, for the tree's pane to show what came in.
export function startDrop({ changed }) {
  state.changed = changed;
  listen("bring-progress", ({ payload }) => write("reads", [payload.path, payload.done >= payload.size ? null : { read: payload.done, size: payload.size }]));
  listen("bring-done", ({ payload }) => {
    if (payload.error) {
      say(payload.error, { failed: true });
    } else {
      say(`Copied ${listOf(payload.brought.map((one) => one.to))} into the tree`);
    }
    state.changed();
  });
  listen("tauri://drag-over", ({ payload }) => {
    const { x, y } = pageOf(payload.position);
    light(folderAt(x, y));
  });
  listen("tauri://drag-leave", () => light(null));
  listen("tauri://drag-drop", ({ payload }) => {
    const { x, y } = pageOf(payload.position);
    bring(payload.paths ?? [], folderAt(x, y)).catch((error) => say(String(error), { failed: true }));
  });
}
