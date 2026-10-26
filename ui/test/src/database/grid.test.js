// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Database window's rows: read a page at a time and ahead, exported, the history, and a table's first rows.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { asFound, calls, made, press, putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

test("the Database tree's menu, from the command line: a table's first rows, a name copied, and a level read again", async () => {
  asFound();
  calls.answer("db_list", () => [{ connection: { name: "made", kind: "postgres" }, open: true }]);
  calls.answer("db_run", () => ({ done: "SELECT 0", ms: 1 }));
  calls.answer("db_connect", () => ({ version: "the test's" }));
  await press("db-first-rows", "public", "things", "made");
  assert.ok(made("db_run").some((one) => one.args.name === "made" && one.args.sql === 'SELECT * FROM "public"."things" LIMIT 200'), JSON.stringify(made("db_run").map((one) => one.args)));
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("db-copy-name", "things");
  assert.deepEqual(copied, ["things"]);
  await press("db-read-again", "public");
  if (!document.getElementById("database").hidden) {
    runCommand("database-view");
  }
  asFound();
});

test("the window is put back as the file found it", putBackAsFound);
