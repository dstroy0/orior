// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find: on a large text held on the window's side, each search runs there and finds what the page's own
// search finds, and one the window's side does not run as the page does is the page's own; on a small
// text, the page searches it itself.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

// The matches as the page's own search gives them, for the find bar as it is set.
const own = (find, doc) => find.scan(doc, find.pattern()).map(({ from, to }) => [from.line, from.col, to.col]);
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

// The window's side's own answer for the find bar as it is set, or null where the search is the page's.
function rustAnswer(find, key) {
  const sought = { query: find.query.value, regex: find.options.regex, case: find.options.case, word: find.options.word, unicode: Boolean(find.pattern()?.unicode) };
  return invoke("document_find", { key, sought, most: 20000 }).catch(() => null);
}

test("a search of a large held text runs on the window's side and finds what the page's own search finds", async () => {
  // past HOLD_FROM lines, so the text is held on the window's side and searched there, and under the size a file opens paged at
  const lines = Array.from({ length: 15000 }, (_, line) => `Line_${line} = line(${line}) # LINE ${line % 11 ? "ſtar" : "star"} $line`);
  lines[7500] = "naïve Line_x = é and 😀 line";
  await invoke("file_write", { path: "found.txt", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("found.txt");
  await ui.waitFor(() => editing().active === "found.txt" && editing().editor?.s?.doc.held?.ready, 40000);
  const view = editing().editor;
  const find = view.find;
  const key = view.doc.held.key;
  find.open(false);
  try {
    const runs = [
      ["line", {}],
      ["Line", { case: true }],
      ["line", { word: true }],
      ["star", {}],
      ["😀 line", { case: true }],
    ];
    for (const [query, options] of runs) {
      const said = `${query} ${JSON.stringify(options)}`;
      assert.ok(await searched(find, query, options), `${said}: asked of the window's side`);
      const expected = own(find, view.doc);
      assert.deepEqual(await rustAnswer(find, key), expected, `${said}: the window's side finds the page's own matches`);
      assert.deepEqual(shown(find), expected, `${said}: and the bar shows them`);
    }
    // Searches that are the page's own: a pattern, which the page's engine matches faster than the
    // window's side's, and text sought outside ASCII without case.
    for (const [query, options] of [["Line_\\d+7 =", { regex: true, case: true }], ["\\p{L}+_7\\b", { regex: true }], ["é", {}]]) {
      await searched(find, query, options);
      assert.equal(await rustAnswer(find, key), null, `${query}: the page's own`);
      assert.deepEqual(shown(find), own(find, view.doc), `${query}: with the page's answer`);
      assert.ok(find.matches.length > 0, `${query}: found`);
    }
  } finally {
    find.close();
  }
});

test("a small text is searched on the page", async () => {
  await invoke("file_write", { path: "small_found.txt", text: "one line\ntwo lines\n" });
  showView("edit");
  await openFile("small_found.txt");
  await ui.waitFor(() => editing().active === "small_found.txt", 5000);
  const find = editing().editor.find;
  find.open(false);
  try {
    assert.equal(await searched(find, "line", {}), false);
    assert.equal(find.matches.length, 2);
  } finally {
    find.close();
  }
});
