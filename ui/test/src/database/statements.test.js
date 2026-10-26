// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Running a statement and a SQL file.

import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { made, press, putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

test("Run, Run Statement and Run File of a SQL file read its statements", async () => {
  showView("edit");
  await openFile("queries.sql");
  editing().editor.goTo(0, 1);
  await press("run-statement");
  assert.ok(made("db_statement_at").length, "the statement at the cursor read");
  await press("run-sql-file");
  assert.ok(made("db_statements").length, "every statement read");
});

test("the window is put back as the file found it", putBackAsFound);
