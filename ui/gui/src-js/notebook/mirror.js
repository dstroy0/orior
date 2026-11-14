// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's cells as branches on the window's side, each cell's session the page's mirror of its own, colored in its
// type's language as a file opened is: the cells the file gives as it opens, and a cell the reader adds.

import { invoke } from "../bridge.js";
import { mirror } from "../edit/mirror.js";
import { state } from "../notebook.js";

// The notebooks whose cells are branches on the window's side, each under a key of its own.
let books = 0;

// The language a cell of `type` is colored in on the window's side, by its id.
const languageIdOf = (book, type) => (type === "code" ? book.language : state.hooks.languageOf("cell.md"))?.id ?? "";

// Each cell a branch as the notebook's file gives it, colored in its type's language, and its session the mirror of it.
export async function mirrorCells(book) {
  books += 1;
  book.key = `${book.path}#book${books}`;
  book.made = 0;
  const cells = await invoke("branch_cells", { key: book.key, path: book.path, code: languageIdOf(book, "code"), markdown: languageIdOf(book, "markdown") }).catch(() => []);
  book.cells.forEach((cell, at) => {
    const one = cells?.[at];
    if (one?.key) {
      mirror(cell.session, one.key, one);
    }
  });
}

// A cell the reader adds a branch of no text, colored in its type's language, and its session the mirror of it; what is
// written in it before that is answered goes after it.
export function mirrorNewCell(book, cell) {
  book.made += 1;
  const key = `${book.key}#made${book.made}`;
  const session = cell.session;
  const made = invoke("branch_new", { key, id: languageIdOf(book, cell.type) }).then((one) => one && session.highlight.put(0, one.colors ?? [], one.names));
  mirror(session, key, null, null, made);
}
