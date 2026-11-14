// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sheets of settings: the memory budget, the checkers, the tree's environment and Maven's, the margins, and the memory reading.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { memoryBudget } from "../../../gui/src-js/statusbar.js";
import { checkersNamed, setCheckers } from "../../../gui/src-js/servers.js";
import { calls, given, made, press, putBackAsFound, sheetShown, told } from "../../menus.js";
import { editing } from "../../../gui/src-js/edit.js";

const { test, assert, ui } = window.__harness;

test("View, Memory Budget takes the megabytes given", async () => {
  const was = memoryBudget();
  try {
    await press("memory-budget", "777");
    assert.equal(memoryBudget(), 777);
  } finally {
    await runCommand("memory-budget", [String(was)]);
  }
});

test("View, Checkers runs the checkers named, and says a name none answers to", async () => {
  const was = checkersNamed();
  const known = await invoke("checkers_known");
  assert.ok(known.length, "the manifest knows a checker");
  const name = known[0][1];
  try {
    await press("checkers", name);
    assert.deepEqual(checkersNamed(), [name]);
    await press("checkers", "no-such-checker");
    assert.ok(told(/No checker is named no-such-checker/), "the unknown name said");
  } finally {
    await setCheckers(was);
  }
});

test("File, Tree Environment and Maven Settings", async () => {
  await press("environment", "none");
  assert.ok(made("env_use").some((one) => one.args.type === null), "the toolchains' own Python used");
  calls.answer("maven_settings", () => [{ group: "pom", label: "Java Release", value: "17", settable: true, key: "release", file: "pom.xml" }]);
  await press("maven-settings");
  const displayed = await sheetShown(".sheet-maven");
  assert.ok(displayed.textContent.includes("Java Release"));
});

test("Edit, Fill Paragraph fills a paragraph to the margin, and Continuation Indent and Documentation Margin take their columns", async () => {
  const words = Array.from({ length: 40 }, (_, at) => `word${at}`).join(" ");
  await given(`# ${words}|\n`);
  await press("fill-paragraph");
  await ui.waitFor(() => editing().editor.doc.count > 3, 5000, "the paragraph filled");
  const width = editing().docMargin() ?? editing().editor.s.margin ?? 79;
  assert.ok(editing().editor.doc.lines.every((line) => line.length <= width), `filled to ${width}: ${editing().editor.doc.text()}`);
  assert.equal(editing().editor.doc.text().replace(/\s*#\s*/g, " ").trim(), words, "every word retained, in order");
  const continued = editing().continuation();
  const margin = editing().docMargin();
  try {
    await press("continuation", "4", "8", "2");
    assert.deepEqual(editing().continuation(), { call: 4, declaration: 8, expression: 2 });
    await press("doc-margin", "72");
    assert.equal(editing().docMargin(), 72);
    await press("doc-margin", "none");
    assert.equal(editing().docMargin(), null);
  } finally {
    editing().setContinuation(continued);
    editing().setDocMargin(margin);
  }
});

test("the memory reading's menu, from the command line: the servers no open tab needs stopped", async () => {
  await press("servers-stop-idle");
  assert.ok(made("servers_unneeded_stop").some((one) => one.args.budget === Number.MAX_SAFE_INTEGER), "every server no open tab needs let go");
});

test("the window is put back as the file found it", putBackAsFound);
