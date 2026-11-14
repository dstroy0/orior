// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's layout: a step of indent, the visual column of a col with tabs reaching their stops, the col nearest a
// visual column, and the rows a long line takes where words wrap.

import { retentionStorage } from "../../../helpers.js";
import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("a step of indent is the file's own, and a tab reaches its next stop", async () => {
  const view = await given("x");
  assert.equal(view.unit(), " ".repeat(view.s.indent.size));
  const size = view.s.indent.size;
  assert.equal(view.vcolOf("\tx", 1), size);
  assert.equal(view.vcolOf("ab\tx", 3), size);
  assert.equal(view.vcolOf("abc", 99), 3, "no further than the line's end");
});

test("the col nearest a visual column, its left edge or the col the column falls in", async () => {
  const view = await given("x");
  const size = view.s.indent.size;
  assert.equal(view.colAtV("abcdef", 2.4), 2);
  assert.equal(view.colAtV("abcdef", 2.6), 3, "rounded to the nearer edge");
  assert.equal(view.colAtV("abcdef", 2.6, false), 2, "or the col it falls in");
  assert.equal(view.colAtV("\tx", size - 1), 1, "past the middle of a tab");
  assert.equal(view.colAtV("ab", 50), 2, "past the end, the end");
});

test("with words wrapped, a line longer than the view takes more rows, and the view's own setting comes back", async () => {
  const view = await given(`${"word ".repeat(200)}\nshort`);
  const retained = view.wrapped();
  await retentionStorage(["orior.word-wrap"], () => {
    try {
      view.setWrap(false);
      assert.equal(view.rows().size, 2);
      view.setWrap(true);
      assert.ok(view.wrapColumns() > 10);
      assert.ok(view.rows().size > 2, "the long line in rows");
      assert.equal(view.rows().lineOf(view.rows().size - 1), 1, "the short line its own last row");
    } finally {
      view.setWrap(retained);
    }
  });
});

test("the window is put back as the file found it", putBackAsFound);
