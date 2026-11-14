// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A tree cloned, opened in a dev container, retained as a template, and trusted.

import { runCommand } from "../../gui/src-js/menubar.js";
import { calls, made, press, putBackAsFound, sheetShown, told } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("File, Open in Dev Container, Clone, Retain Tree as Template, Trust This Tree and Exit", async () => {
  calls.answer("devcontainer_line", () => "echo dev-container");
  await runCommand("dev-container");
  await ui.waitFor(() => /dev-container/.test(document.getElementById("term-views").textContent), 10000, "the container's line run in the terminal");
  await runCommand("clone");
  await sheetShown("form");
  const retained = [];
  calls.answer("template_save", ({ name }) => (retained.push(name), "templates/retained"));
  await press("retain-template", "retained_template");
  assert.deepEqual(retained, ["retained_template"]);
  // A tree that holds code its tools run is asked about; the fixture's holds none, and is told so.
  await press("trust-tree");
  await ui.waitFor(() => !document.getElementById("trust-ask").hidden || told(/nothing to trust/), 3000, "the trust asked or told");
  document.getElementById("trust-ask").hidden = true;
  calls.answer("app_exit", () => null);
  await press("exit");
  assert.equal(made("app_exit").length, 1, "the window asked to end");
  runCommand("toggle");
});

test("the window is put back as the file found it", putBackAsFound);
