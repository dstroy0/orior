// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Extract Function in Rust and the C family: it takes its types from the language server, and says so where no
// server serves the file, and where the selection is in no function.

import { extractTyped } from "../../../gui/src-js/extract/typed.js";
import { given, putBackAsFound, told } from "../../menus.js";

const { test, assert } = window.__harness;

test("with no server serving the file, Extract Function in Rust and the C family says it takes its types from one", async () => {
  const view = await given("let x = 1;|");
  await extractTyped(view, { served: false }, "rust", {}, { s: view.s, doc: view.doc, head: 0 });
  assert.ok(told(/Extract Function in Rust takes its types from the language server, and no server serves this file/));
  await extractTyped(view, null, "c", {}, { s: view.s, doc: view.doc, head: 0 });
  assert.ok(told(/Extract Function in the C family takes its types/));
});

test("a selection in no function is refused", async () => {
  const view = await given("int x = 1;|");
  await extractTyped(view, { served: true, file: "a.c" }, "c", {}, { s: view.s, doc: view.doc, head: -1 });
  assert.ok(told(/The selection is in no function/));
});

test("the window is put back as the file found it", putBackAsFound);
