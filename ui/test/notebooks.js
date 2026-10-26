// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the notebook tests share: a notebook of given cells written into the tree and opened in the editor, and its
// cells as a notebook's file writes them.

import { invoke } from "../gui/src-js/bridge.js";
import { editing, openFile } from "../gui/src-js/edit.js";
import { state } from "../gui/src-js/notebook.js";
import { showView } from "../gui/src-js/views.js";

const { ui } = window.__harness;

export const code = (source, more = {}) => ({ cell_type: "code", source, metadata: {}, outputs: [], execution_count: null, ...more });
export const markdown = (source) => ({ cell_type: "markdown", source, metadata: {} });

// The notebook notebook_<name>.ipynb of `cells` written into the tree and opened in the editor, drawn: its model as
// the window holds it.
export async function openedNotebook(name, cells, metadata = { kernelspec: { name: "python3", display_name: "Python 3", language: "python" } }) {
  const path = `notebook_${name}.ipynb`;
  await invoke("file_write", { path, text: JSON.stringify({ nbformat: 4, nbformat_minor: 5, metadata, cells }, null, 1) });
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path && state.books.get(path) && document.querySelectorAll("#notebook .nb-cell").length === cells.length, 10000, `${path} drawn as a notebook`);
  return state.books.get(path);
}

// The node of the notebook's cell at `index`.
export const cellNodeAt = (book, index) => book.cells[index].node;
