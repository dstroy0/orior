// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view: how far ahead of a view at rest its lines are colored.

import { coloringReach } from "../../../src/editor/view.js";

const { test, assert } = window.__harness;

test("a view that came to rest slowly is colored three screens ahead", () => {
  assert.equal(coloringReach(40, 0, 30), 120);
  assert.equal(coloringReach(40, 0.1, 30), 120);
});

test("a view that came to rest fast is colored as far as its speed carries it over two screens' coloring", () => {
  const fast = coloringReach(40, 100, 30);
  assert.ok(fast > 120, `farther than three screens: ${fast}`);
  const twice = coloringReach(40, 200, 30);
  assert.ok(Math.abs(twice - 2 * fast) <= 2, `twice the speed reaches twice as far: ${fast}, ${twice}`);
  assert.ok(Math.abs(coloringReach(40, 100, 60) - 2 * fast) <= 2, "twice the coloring time reaches twice as far");
});
