// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger as simple as print: Print Here sets a message breakpoint that writes the line's values
// with the file unchanged, and the prints a change added stand in the Commit window and are taken out
// in one step, from a tab that holds the file and from a file no tab holds.

import { invoke } from "../../src/bridge.js";
import { breakpointsOf } from "../../src/debug.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { addedPrints, messageOf } from "../../src/prints.js";
import { togglePane } from "../../src/sides.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

test("a line's message names each value it reads, or the expression selected", () => {
  assert.equal(messageOf("total = price * count  # the sum", "", "a.py:3 "), "a.py:3 total={total} price={price} count={count}");
  assert.equal(messageOf("x = compute(a, b.c)"), "x={x} a={a} b={b}", "not a name called, nor one after a dot");
  assert.equal(messageOf('if name == "it":'), "name={name}", "not a keyword, nor a string's words");
  assert.equal(messageOf("anything", "a + b"), "a + b = {a + b}");
});

test("Print Here sets a message breakpoint on the line, the file unchanged", async () => {
  await invoke("file_write", { path: "printing_here.py", text: "def f(price, count):\n    total = price * count\n    return total\n" });
  showView("edit");
  await openFile("printing_here.py");
  await ui.waitFor(() => editing().active === "printing_here.py", 5000);
  const before = editing().editor.s.doc.text();
  editing().editor.goTo(1, 4);
  runCommand("print-here");
  assert.equal(breakpointsOf("printing_here.py")?.get(1)?.log, "printing_here.py:2 total={total} price={price} count={count}");
  assert.equal(editing().editor.s.doc.text(), before, "the file is unchanged");
});

test("the prints a change added stand in the Commit window and are taken out in one step", async () => {
  const root = await invoke("root_get");
  await invoke("folder_create", { path: "printing" }).catch(() => {});
  await invoke("repo_init", { folder: `${root}/printing` });
  await invoke("file_write", { path: "printing/a.py", text: "def f(x):\n    print(\"kept\")\n    return x\n" });
  await invoke("git_commit", { message: "first", paths: ["printing/a.py"] });
  await invoke("file_write", { path: "printing/a.py", text: "def f(x):\n    print(\"kept\")\n    print(x)\n    return x\n" });
  await invoke("file_write", { path: "printing/b.js", text: "const a = 1;\nconsole.log(a);\nexport { a };\n" });
  const found = await addedPrints();
  assert.deepEqual(found.map((one) => [one.path, one.line, one.text]), [["printing/a.py", 2, "    print(x)"], ["printing/b.js", 1, "console.log(a);"]], "a print already committed is not one the change added");
  await openFile("printing/a.py");
  await ui.waitFor(() => editing().active === "printing/a.py", 5000);
  togglePane(true, { take: false, pin: true });
  runCommand("commit-view");
  await ui.waitFor(() => document.querySelectorAll(".commit-print").length === 2 && document.querySelector(".commit-prints").checkVisibility(), 10000);
  assert.equal(document.querySelector(".commit-prints-head").textContent, "2 prints added since the last commit");
  await ui.click([...document.querySelectorAll(".commit-prints button")].find((one) => one.textContent === "Take Them Out"));
  await ui.waitFor(() => !editing().sessionOf("printing/a.py").doc.text().includes("print(x)"), 5000);
  assert.equal(editing().sessionOf("printing/a.py").doc.text(), "def f(x):\n    print(\"kept\")\n    return x\n", "the tab's own text, one edit undo takes back");
  assert.equal((await invoke("file_read", { path: "printing/b.js" })).text, "const a = 1;\nexport { a };\n", "the file no tab holds, as it is written");
});
