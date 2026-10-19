// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find in Files as the reader drives it: text typed in the Search pane finds each line that holds it,
// case and whole words turn on and off beside the field, and a line found opens its file at the match.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing } from "../../gui/src-js/edit.js";
import { focusSearch } from "../../gui/src-js/search.js";
import { runCommand } from "../../gui/src-js/menubar.js";

const { test, assert, ui } = window.__harness;

// The lines found, as path:line:col, in the files written here.
const lines = () => [...document.querySelectorAll("#search-hits .search-hit")].map((row) => row.title).filter((title) => title.startsWith("sought/")).sort();

// Waits for the lines found to come to `expected`, the search running a rest after each change, and
// holds them to it, saying where they do not what the field holds and what the tree's search gives.
async function found(expected, message) {
  await ui.waitFor(() => JSON.stringify(lines()) === JSON.stringify(expected), 10000).catch(() => {});
  if (JSON.stringify(lines()) === JSON.stringify(expected)) {
    return;
  }
  const query = document.getElementById("search-query");
  const asked = await invoke("tree_search", { query: query.value, how: { case: false, word: false, regex: false }, set: null }).catch((error) => String(error));
  assert.deepEqual(lines(), expected, `${message}; the field holds ${JSON.stringify(query.value)}, focused ${document.activeElement === query}, the pane says ${JSON.stringify(document.getElementById("search-said").textContent)}, and the tree's search gives ${JSON.stringify(asked).slice(0, 400)}`);
}

test("text typed is found on each line that holds it, with case and whole words as the buttons beside the field set them", async () => {
  await invoke("folder_create", { path: "sought" }).catch(() => {});
  await invoke("file_write", { path: "sought/a.txt", text: "Needle in a line\nno match here\nneedles and NEEDLE\n" });
  await invoke("file_write", { path: "sought/b.py", text: `${"x = 1\n".repeat(3000)}needle_count = 2  # needle\n` });
  runCommand("search");
  await ui.waitFor(() => document.activeElement?.id === "search-query", 5000);
  await ui.key("Ctrl+A");
  await ui.type("needle");
  await found(["sought/a.txt:1:1", "sought/a.txt:3:1", "sought/b.py:3001:1"], "without case, the first place on each line");
  const option = (name) => document.querySelector(`#search-options [data-option="${name}"]`);
  try {
    await ui.click(option("case"));
    await ui.waitFor(() => option("case").getAttribute("aria-checked") === "true", 2000);
    await found(["sought/a.txt:3:1", "sought/b.py:3001:1"], "with case");
    await ui.click(option("word"));
    await ui.waitFor(() => option("word").getAttribute("aria-checked") === "true", 2000);
    await found(["sought/b.py:3001:21"], "with case and whole words, the word after the #");
    await ui.click([...document.querySelectorAll("#search-hits .search-hit")].find((row) => row.title === "sought/b.py:3001:21"));
    await ui.waitFor(() => editing().active === "sought/b.py", 5000);
    const head = editing().editor.primary().head;
    assert.deepEqual([head.line, head.col], [3000, 20], "the file opens at the match");
  } finally {
    focusSearch("", { case: false, word: false });
  }
});
