// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Database tool window's state: the connection chosen, the connection a file runs on, and a schema level's key.

import { CHOSEN, chosen, connectionForFile, connectionOf, FILES, filesKept, keyOf, state } from "../../../gui/src-js/database/state.js";
import { keepingStorage } from "../../helpers.js";

const { test, assert } = window.__harness;

test("a file runs on the connection chosen for it, or else on the window's", async () => {
  await keepingStorage([CHOSEN, FILES], () => {
    localStorage.setItem(CHOSEN, "local");
    localStorage.setItem(FILES, JSON.stringify({ "q/report.sql": "warehouse" }));
    assert.equal(chosen(), "local");
    assert.equal(connectionForFile("q/report.sql"), "warehouse");
    assert.equal(connectionForFile("other.sql"), "local");
  });
});

test("files kept unreadable are none kept, and nothing chosen is the empty name", async () => {
  await keepingStorage([CHOSEN, FILES], () => {
    localStorage.removeItem(CHOSEN);
    localStorage.setItem(FILES, "{not json");
    assert.deepEqual(filesKept(), {});
    assert.equal(connectionForFile("a.sql"), "");
  });
});

test("a connection is found by its name among those read, and a level's path is one key", () => {
  const kept = state.connections;
  state.connections = [{ connection: { name: "local", kind: "postgres" } }];
  try {
    assert.deepEqual(connectionOf("local"), { name: "local", kind: "postgres" });
    assert.equal(connectionOf("gone"), null);
  } finally {
    state.connections = kept;
  }
  assert.equal(keyOf(["db", "public", "users"]), "db\u0000public\u0000users");
  assert.notEqual(keyOf(["a", "bc"]), keyOf(["ab", "c"]));
});
