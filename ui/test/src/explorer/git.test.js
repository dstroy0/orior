// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Git pane's graph and the commits' menus.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing } from "../../../gui/src-js/edit.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { asFound, calls, press, putBackAsFound, TREE } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the commits' menus, from the command line: a revision opened, compared, its id and message copied, its files and changes shown", async () => {
  asFound();
  const tree = TREE();
  const git = (...words) => ui.run("git", ["-C", tree, "-c", "user.name=orior", "-c", "user.email=orior@localhost", ...words]);
  const before = (await invoke("file_read", { path: "hello.py" })).text;
  await invoke("file_write", { path: "hello.py", text: `${before}# a second revision\n` });
  await git("commit", "-qam", "a second revision of hello");
  try {
    const [newest, older] = await invoke("file_commits", { path: "hello.py" });
    assert.ok(newest && older, "two revisions of hello.py");
    await press("commit-open", "hello.py", older.id.slice(0, 7));
    await ui.waitFor(() => editing().active === `hello.py@${older.id}`, 5000, "hello.py as the older revision left it");
    await press("commit-compare", "hello.py", older.id.slice(0, 7));
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the revision beside the file");
    closeDiff();
    await press("commit-compare", "hello.py", newest.id.slice(0, 7), older.id.slice(0, 7));
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the revisions beside each other");
    closeDiff();
    const copied = [];
    calls.answer("clip_write", ({ text }) => copied.push(text));
    await press("commit-copy-id", newest.id.slice(0, 7));
    await press("commit-copy-message", newest.id.slice(0, 7), "hello.py");
    assert.deepEqual(copied, [newest.id, "a second revision of hello"]);
    await press("commit-files", newest.id.slice(0, 7));
    const row = () => [...document.querySelectorAll(".git-commit")].find((one) => one.dataset.key === newest.id);
    await ui.waitFor(() => row()?.getAttribute("aria-expanded") === "true", 5000, "the commit's files shown");
    await press("commit-file-changes", newest.id.slice(0, 7), "hello.py");
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the commit's change to hello.py");
    closeDiff();
    await press("commit-files", newest.id.slice(0, 7));
    await ui.waitFor(() => row()?.getAttribute("aria-expanded") === "false", 5000, "and hidden again");
  } finally {
    await git("reset", "-q", "--hard", "HEAD~1");
    asFound();
  }
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
