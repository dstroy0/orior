// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's geometry: where a place is across, the place under a point the page gives back the same, and the
// continuation indent a new line inside a bracket takes.

import { retentionStorage } from "../../../helpers.js";
import { given, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

test("a place further along its line is further right, each col a character's width on", async () => {
  const view = await given("abcdefgh");
  const x2 = view.xOf({ line: 0, col: 2 });
  const x6 = view.xOf({ line: 0, col: 6 });
  assert.ok(Math.abs((x6 - x2) / 4 - view.cw) < 0.5, "a character's width each col");
});

test("the place under a point is the place whose rectangle holds it", async () => {
  const view = await given("first line\nsecond line");
  const rect = view.rectOf({ line: 1, col: 3 });
  const p = view.posAt({ clientX: rect.left + 1, clientY: (rect.top + rect.bottom) / 2 });
  assert.deepEqual([p.line, p.col], [1, 3]);
});

test("a new line inside a bracket takes the continuation set for where the bracket is, else one step", async () => {
  const view = await given("result = call(\ndef f(");
  const retained = view.continuation;
  await retentionStorage(["orior.continuation"], () => {
    try {
      view.setContinuation(null);
      assert.equal(view.continuationAt(0, 13), view.unit(), "one step where none is set");
      view.setContinuation({ call: 8, declaration: 2, expression: 4 });
      assert.equal(view.continuationAt(0, 13), " ".repeat(8), "a call's arguments");
      assert.equal(view.continuationAt(1, 5), " ".repeat(2), "a declaration's parameters");
    } finally {
      view.continuation = retained;
    }
  });
});

test("the window is put back as the file found it", putBackAsFound);
