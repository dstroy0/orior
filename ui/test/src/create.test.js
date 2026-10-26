// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Making a file, a folder, a repository or a project.

import { editing } from "../../gui/src-js/edit.js";
import { made, press, putBackAsFound, sheetShown } from "../menus.js";

const { test, assert } = window.__harness;

test("File, Create makes the file or folder the command line names, and asks for one where none is named", async () => {
  await press("create-file", "made_by_command.txt");
  assert.equal(editing().active, "made_by_command.txt", "the file made and opened");
  await press("create-folder", "made_by_command");
  assert.ok(made("folder_create").some((one) => one.args.path === "made_by_command"));
  await press("create-file");
  assert.equal((await sheetShown(".sheet-create")).querySelector("h2").textContent, "Create File");
  await press("create-repository");
  assert.equal((await sheetShown("form")).querySelector("h2").textContent, "Create Repository");
  await press("create-project");
  assert.equal((await sheetShown("h2")).textContent, "Project from Template");
});

test("the window is put back as the file found it", putBackAsFound);
