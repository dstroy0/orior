// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Git menu, and the lists the window's commands show: branches, stashes, tests, the profile and the quick
// fixes, each item from the command line.

import { allCommands, hasCommand, runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { closeMenu } from "../../../gui/src-js/menu.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { toggleDebugPanel } from "../../../gui/src-js/debug.js";
import { asFound, calls, itemsUnder, made, press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Git, Branches lists them and New Branch makes one", async () => {
  const repo = editing().repo();
  await press("branches");
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the branches");
  assert.ok(menu.textContent.includes("New Branch…"));
  closeMenu(false);
  await press("new-branch", "made-by-command");
  await ui.waitFor(async () => (await invoke("git_branches", { repo })).some((one) => one.name === "made-by-command"), 10000, "the branch made");
  await invoke("git_branch", { act: "delete", name: "made-by-command", repo }).catch(() => {});
});

test("Git, Stash Changes puts the changes aside and Stashes lists them", async () => {
  const repo = editing().repo();
  const before = (await invoke("file_read", { path: "README.md" })).text;
  await invoke("file_write", { path: "README.md", text: `${before}stashed by the test\n` });
  await press("stash", "stashed by the test");
  await ui.waitFor(async () => !(await invoke("file_read", { path: "README.md" })).text.includes("stashed by the test"), 10000, "the change put aside");
  await press("stashes");
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the stashes");
  assert.ok(menu.textContent.includes("stashed by the test"));
  closeMenu(false);
  // The stash holds the tree's other changes too: each is given back, and README as it was.
  const [stash] = await invoke("git_stashes", { repo });
  await invoke("git_stash", { act: "pop", name: stash.name, repo });
  await invoke("file_write", { path: "README.md", text: before });
});

test("every item of the lists the window's commands show runs a command the command line runs: branches, stashes, commits, environments and docstring forms", async () => {
  asFound();
  showView("edit");
  await openFile("hello.py");
  calls.answer("git_branches", () => [{ name: "main", current: true, remote: false }, { name: "other", current: false, remote: false }, { name: "origin/other", current: false, remote: true }]);
  calls.answer("git_stashes", () => [{ name: "stash@{0}", subject: "kept by the test" }]);
  calls.answer("git_elsewhere", () => [{ id: "abc1234def5678", subject: "made elsewhere" }]);
  calls.answer("envs_found", () => [{ kind: "venv", place: "", requires: [] }]);
  const named = new Set(allCommands().map((item) => item.command));
  const wrong = [];
  for (const command of ["branches", "stashes", "cherry-pick", "environment", "docstring-form"]) {
    await runCommand(command);
    await ui.waitFor(() => document.querySelector("body > .menu"), 5000, `the list ${command} shows`);
    const items = itemsUnder([...document.querySelectorAll("body > .menu > .menu-item")].map((button) => button.item));
    closeMenu(false);
    assert.ok(items.length, `${command} lists something`);
    for (const item of items) {
      if (!item.command || !named.has(item.command) || !hasCommand(item.command)) {
        wrong.push(`${command}: ${item.label} runs ${item.command ?? "no command"}`);
      }
    }
  }
  assert.deepEqual(wrong, []);
});

test("a branch's menu, from the command line: switched to, merged, rebased onto, renamed and deleted", async () => {
  asFound();
  calls.answer("git_branch", ({ act, name }) => `${act} ${name} by the test`);
  const asked = () => made("git_branch").map(({ args }) => [args.act, args.name, args.to ?? null]);
  await press("branch-switch", "other");
  await press("branch-merge", "other");
  await press("branch-rebase", "other");
  await press("branch-rename", "other", "renamed");
  await press("branch-delete", "other");
  await ui.waitFor(() => asked().length >= 5, 5000, "each asked of Git");
  assert.deepEqual(asked(), [["switch", "other", null], ["merge", "other", null], ["rebase", "other", null], ["rename", "other", "renamed"], ["delete", "other", null]]);
});

test("a stash's menu, from the command line: applied, popped, and dropped once the reader says yes", async () => {
  asFound();
  calls.answer("git_stash", ({ act, name }) => `${act} ${name} by the test`);
  calls.answer("git_stashes", () => [{ name: "stash@{0}", subject: "kept by the test" }]);
  const asked = () => made("git_stash").map(({ args }) => [args.act, args.name]);
  await press("stash-apply", "stash@{0}");
  await press("stash-pop", "stash@{0}");
  await ui.waitFor(() => asked().length >= 2, 5000, "each asked of Git");
  await press("stash-drop", "stash@{0}");
  const yes = await ui.waitFor(() => document.querySelector("dialog[open] .sheet-ask button.primary"), 5000, "the question before the stash is thrown away");
  assert.equal(yes.textContent, "Drop");
  yes.click();
  await ui.waitFor(() => asked().length >= 3, 5000, "the drop asked of Git");
  assert.deepEqual(asked(), [["apply", "stash@{0}"], ["pop", "stash@{0}"], ["drop", "stash@{0}"]]);
});

test("a test's menu, from the command line: a test run, run for its coverage, and debugged", async () => {
  asFound();
  calls.answer("tests_run", () => null);
  calls.answer("debug_start", () => {
    throw new Error("debug_start: answered by the test");
  });
  const id = "test_menus.py::test_passes";
  await press("test-run", id);
  await ui.waitFor(() => made("tests_run").length, 5000, "the test run");
  assert.deepEqual(made("tests_run").at(-1).args.given, [id]);
  assert.equal(made("tests_run").at(-1).args.cover, false);
  await press("test-cover", "test_menus.py");
  await ui.waitFor(() => made("tests_run").length > 1, 5000, "the file run for its coverage");
  assert.deepEqual(made("tests_run").at(-1).args.given, ["test_menus.py"]);
  assert.equal(made("tests_run").at(-1).args.cover, true);
  await press("test-debug", id);
  await ui.waitFor(() => made("debug_start").length, 5000, "the test debugged");
  assert.equal(made("debug_start").at(-1).args.start.id, id);
  toggleDebugPanel(false);
});

test("the window is put back as the file found it", putBackAsFound);
