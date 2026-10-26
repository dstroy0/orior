// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Sections: a document's headings by their marks, none inside a skipped block, and the folds of its headings and
// blocks.

import { headingsOf, sectionFolds } from "../../../gui/src-js/editor/sections.js";

const { test, assert } = window.__harness;

const docOf = (text) => {
  const lines = text.split("\n");
  return { count: lines.length, line: (at) => lines[at] };
};
const MARKDOWN = { headings: /^(#{1,6})\s+(.*)$/, skips: [[/^```/, /^```/]], blocks: [[/^:::/, /^:::$/]] };

test("each heading is read with its line, its level by its marks and its name, none inside a fenced block", () => {
  const doc = docOf("# One\ntext\n## Two\n```\n# not a heading\n```\n### Three ");
  assert.deepEqual(headingsOf(doc, MARKDOWN), [
    { line: 0, level: 1, name: "One" },
    { line: 2, level: 2, name: "Two" },
    { line: 6, level: 3, name: "Three" },
  ]);
  assert.deepEqual(headingsOf(doc, {}), [], "a language with no headings has none");
});

test("a heading's fold runs to the line before the next heading of its level or above, its blank lines at the end left out", () => {
  const doc = docOf("# One\na\n## Two\nb\n\n# Three\nc\n");
  const folds = sectionFolds(doc, MARKDOWN);
  assert.equal(folds.get(0), 3, "One holds Two, and stops before Three without the blank line");
  assert.equal(folds.get(2), 3);
  assert.equal(folds.get(5), 6, "the last runs to the end, its blank line left out");
});

test("a block folds from its opening line to its closing one, the inner of two closed first", () => {
  const doc = docOf("::: outer\n::: inner\nx\n:::\ny\n:::");
  const folds = sectionFolds(doc, MARKDOWN);
  assert.equal(folds.get(1), 3);
  assert.equal(folds.get(0), 5);
});
