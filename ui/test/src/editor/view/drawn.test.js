// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the editor draws in its gutter: the mark of a line added, changed, or with lines taken out above or below it,
// and Line History's date on the first line of each run a commit left; and in its text, each bracket in the color of
// how deep it stands.

import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("a line added or changed has its bar, and lines taken out a wedge above, or below the last line", async () => {
  const view = await given("a\nb\nc");
  const kept = view.s.changes;
  try {
    view.setChanges(view.s, { added: new Set([0]), changed: new Set([1]), removed: new Set([2, 3]) });
    assert.match(view.changeMark(0), /ed-change added/);
    assert.match(view.changeMark(1), /ed-change changed/);
    assert.match(view.changeMark(2), /ed-change removed" data-change="2"/);
    assert.match(view.changeMark(2), /ed-change removed below" data-change="3"/);
    view.setChanges(view.s, null);
    assert.equal(view.changeMark(0), "", "no marks where none are set");
  } finally {
    view.setChanges(view.s, kept);
  }
});

test("a commit's date stands on the first line of the run it left, and a line no commit holds has no mark", async () => {
  const view = await given("a\nb\nc");
  const kept = view.s.history;
  const when = Date.UTC(2026, 0, 15, 12) / 1000;
  const commit = { id: "0123456789abcdef", when, author: "Someone", subject: "The subject" };
  try {
    view.setLineHistory(view.s, { lines: [0, 0, -1], commits: [commit] });
    const date = new Date(when * 1000);
    const day = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
    const first = view.historyMark(0);
    assert.match(first, /class="ed-history first"/);
    assert.ok(first.includes(`>${day}`), "the date shown on the run's first line");
    assert.ok(first.includes("01234567") && first.includes("The subject"), "the id and subject under the pointer");
    assert.match(view.historyMark(1), /class="ed-history" [^>]*><\/span>/, "the run's next line marked, no date shown");
    assert.equal(view.historyMark(2), "");
  } finally {
    view.setLineHistory(view.s, kept);
  }
});

test("each depth of nested brackets has a color no other depth has, a bracket and the one that closes it the same", async () => {
  const view = await given("");
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text: "(((())))\n{{{{}}}}[[[[]]]]" }], null);
  const was = view.bracketsOn;
  try {
    view.setBrackets(true);
    const depths = (line) => [...view.rowHtml(line).matchAll(/class="ed-br" style="--depth:(\d+)"/g)].map((found) => Number(found[1]));
    const nest = [0, 1, 2, 3, 3, 2, 1, 0];
    assert.deepEqual(depths(0), nest);
    assert.deepEqual(depths(1), [...nest, ...nest]);
    const colors = [0, 1, 2, 3, 4, 5, 6, 7].map((depth) => {
      const span = document.createElement("span");
      span.className = "ed-br";
      span.style.setProperty("--depth", String(depth));
      view.host.append(span);
      const color = getComputedStyle(span).color;
      span.remove();
      return color;
    });
    assert.equal(new Set(colors).size, colors.length, `eight depths, eight colors: ${colors.join(", ")}`);
  } finally {
    view.setBrackets(was);
  }
});

test("the window is put back as the file found it", putBackAsFound);
