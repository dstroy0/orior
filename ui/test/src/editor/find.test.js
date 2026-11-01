// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find: every search runs on the window's side, over the branch a text is stored as or the pages a paged file is, and
// finds what the lines hold as the find bar's flags read them, a pattern among them; a pattern that does not read marks
// the bar; a small text is a branch searched there as a large one is; and a paged file is searched whole, a match
// outside its window brought into it.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openAt, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { press } from "../../menus.js";

const { test, assert, ui } = window.__harness;

// The most matches a search gives, its lines each searched whole until they are found.
const MOST = 20000;

// The matches the lines `lines` hold for `query` as the find bar's flags read it, each [line, from col, to col]: a
// pattern or the query as it is, with case or without, whole words or not, a match of no letters passed over, the
// lines read whole until MOST are found.
function expectedOf(lines, query, options) {
  let source = options.regex ? query : query.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  if (options.word) {
    source = `(?<![\\p{L}\\p{N}_$])(?:${source})(?![\\p{L}\\p{N}_$])`;
  }
  const pattern = new RegExp(source, `g${options.case ? "" : "i"}u`);
  const found = [];
  for (let line = 0; line < lines.length && found.length < MOST; line += 1) {
    for (const match of lines[line].matchAll(pattern)) {
      if (match[0].length) {
        found.push([line, match.index, match.index + match[0].length]);
      }
    }
  }
  return found;
}

const shown = (find) => find.matches.map(({ from, to }) => [from.line, from.col, to.col]);

// Sets the find bar and searches, answering whether the search was asked of the window's side.
async function searched(find, query, options) {
  Object.assign(find.options, { regex: false, case: false, word: false }, options);
  find.query.value = query;
  find.refresh();
  const asked = Boolean(find.asking);
  await ui.waitFor(() => !find.asking, 10000);
  return asked;
}

// The window's side's own answer for `query` with the find bar's flags `options`, or null where it refuses.
function sideAnswer(key, query, options) {
  const sought = { query, regex: false, case: false, word: false, ...options };
  return invoke("document_find", { key, sought, most: MOST }).catch(() => null);
}

test("a search of a large branch runs on the window's side and finds what the lines hold, a pattern among them", async () => {
  // under the size a file opens paged at, so the text is a branch stored whole on the window's side and searched there
  const lines = Array.from({ length: 15000 }, (_, line) => `Line_${line} = line(${line}) # LINE ${line % 11 ? "ſtar" : "star"} $line`);
  lines[7500] = "naïve Line_x = é and 😀 line";
  await invoke("file_write", { path: "found.txt", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("found.txt");
  await ui.waitFor(() => editing().active === "found.txt" && editing().editor?.s?.doc.mirrored?.ready, 40000);
  const view = editing().editor;
  const find = view.find;
  const key = view.doc.mirrored.key;
  find.open(false);
  try {
    const runs = [
      ["line", {}],
      ["Line", { case: true }],
      ["line", { word: true }],
      ["star", {}],
      ["😀 line", { case: true }],
      ["Line_\\d+7 =", { regex: true, case: true }],
      ["\\p{L}+_7\\b", { regex: true }],
      ["é", {}],
    ];
    for (const [query, options] of runs) {
      const said = `${query} ${JSON.stringify(options)}`;
      assert.ok(await searched(find, query, options), `${said}: asked of the window's side`);
      const expected = expectedOf(lines, query, options);
      assert.ok(expected.length > 0, `${said}: the lines hold it`);
      assert.deepEqual(await sideAnswer(key, query, options), expected, `${said}: the window's side finds what the lines hold`);
      assert.deepEqual(shown(find), expected, `${said}: and the bar shows it`);
      assert.equal(find.bar.dataset.bad, "false", `${said}: the bar not marked`);
    }
    await searched(find, "(", { regex: true });
    assert.equal(find.bar.dataset.bad, "true", "a pattern that does not read marks the bar");
    assert.deepEqual(shown(find), [], "and finds nothing");
  } finally {
    find.close();
  }
});

test("a small text is a branch searched on the window's side as a large one is", async () => {
  await invoke("file_write", { path: "small_found.txt", text: "one line\ntwo lines\n" });
  showView("edit");
  await openFile("small_found.txt");
  await ui.waitFor(() => editing().active === "small_found.txt" && editing().editor?.s?.doc.mirrored?.ready, 5000);
  const find = editing().editor.find;
  find.open(false);
  try {
    assert.equal(await searched(find, "line", {}), true);
    assert.equal(find.matches.length, 2);
  } finally {
    find.close();
  }
});

test("a paged file is searched whole on the window's side, and a match outside its window brought into it", async () => {
  const path = "found_paged.py";
  const far = 240000;
  const lines = Array.from({ length: 250000 }, (_, line) => `row_${line} = ${line}`);
  lines[far] = "far_away = 1";
  await invoke("file_write", { path, text: `${lines.join("\n")}\n` });
  showView("edit");
  await openAt(path, 0, 0);
  try {
    await ui.waitFor(() => editing().active === path && editing().editor?.s?.window?.paged, 20000, "the file opened paged");
    const view = editing().editor;
    const find = view.find;
    assert.ok(view.s.base + view.s.doc.count <= far, "the window stands before the match");
    find.open(false);
    try {
      assert.ok(await searched(find, "far_\\w+", { regex: true }), "asked of the window's side");
      assert.deepEqual(shown(find), [[far, 0, 8]], "the match in the file's lines, past the window");
      find.step(1);
      await ui.waitFor(() => view.s.base <= far && far < view.s.base + view.s.doc.count, 10000, "the window brought to the match");
      await ui.waitFor(() => view.primary().head.line + view.s.base === far, 5000, "the match selected");
      const sel = view.primary();
      assert.deepEqual([sel.anchor.line + view.s.base, sel.anchor.col, sel.head.col], [far, 0, 8]);
    } finally {
      find.close();
    }
  } finally {
    if (editing().active === path) {
      await press("close-editor");
    }
  }
});
