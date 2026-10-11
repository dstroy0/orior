// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar's keys: a key two commands for different kinds of file share goes to the one whose
// kind the file shown is.

import { firstKeys } from "../../src/menubar.js";

const { test, assert } = window.__harness;

test("a key two commands for different kinds of file share goes to the one the file shown is for", () => {
  const items = [
    { command: "run-statement", keys: "Ctrl+Enter", needs: "sql" },
    { command: "send-request", keys: "Ctrl+Enter", needs: "http" },
    { command: "run-sql-file", keys: "Ctrl+Shift+Enter", needs: "sql" },
  ];
  let shown = "a.sql";
  const needs = { sql: () => shown.endsWith(".sql"), http: () => shown.endsWith(".http") };
  const ran = [];
  const keys = firstKeys(items, needs, (command) => ran.push(command));
  assert.deepEqual(keys.map((one) => one.keys), ["Ctrl+Enter", "Ctrl+Shift+Enter"], "one binding for each key");
  const enter = keys[0].run;
  assert.equal(enter(), true);
  shown = "calls.http";
  assert.equal(enter(), true);
  shown = "notes.md";
  assert.equal(enter(), false, "the editor's own use of the key where neither is for the file");
  assert.deepEqual(ran, ["run-statement", "send-request"]);
});
