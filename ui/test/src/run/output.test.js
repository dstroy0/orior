// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menus of the Changes pane, the jobs and their output, the terminal, the status bar, the Containers window,
// the Database tree and the machine button, each item from the command line.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { closeDiff } from "../../../gui/src-js/diffview.js";
import { chosenJob, chosenLive, loadRun } from "../../../gui/src-js/run.js";
import { asFound, calls, press, putBackAsFound, QUICK, SLOW } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the Changes pane's menu, from the command line: a file's changes shown", async () => {
  asFound();
  const before = (await invoke("file_read", { path: "README.md" })).text;
  await invoke("file_write", { path: "README.md", text: `${before}a change of the test's\n` });
  try {
    await press("commit-view");
    await ui.waitFor(() => document.querySelector('.change-row[data-path="README.md"]'), 10000, "README.md among the changes");
    await press("change-show", "README.md");
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "README.md's changes");
  } finally {
    closeDiff();
    await invoke("file_write", { path: "README.md", text: before });
    asFound();
  }
});

test("the jobs' menu and their output's, from the command line: started, stopped, edited, their path and output copied", async () => {
  asFound();
  await invoke("file_write", { path: "src/build_quick.sh", text: "#!/usr/bin/env bash\necho quick\n" });
  await invoke("file_write", { path: "src/build_slow.sh", text: "#!/usr/bin/env bash\nsleep 30\n" });
  await loadRun();
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("job-start", SLOW);
  await ui.waitFor(() => chosenJob()?.id === SLOW && chosenLive(), 10000, "the job running");
  await press("job-stop", SLOW);
  await ui.waitFor(() => !chosenLive(), 10000, "the job stopped");
  await press("job-start", QUICK);
  await ui.waitFor(() => chosenJob()?.id === QUICK && !chosenLive() && /quick/.test(document.getElementById("lines")?.textContent ?? ""), 15000, "the quick job run");
  await press("output-copy-all", QUICK);
  assert.match(copied.at(-1) ?? "", /quick/, "the output copied whole");
  await press("output-select-all");
  assert.match(window.getSelection().getRangeAt(0).toString(), /quick/, "the output chosen whole");
  await press("output-copy");
  assert.match(copied.at(-1) ?? "", /quick/, "what is chosen copied");
  await press("job-copy-path", QUICK);
  assert.equal(copied.at(-1), "src/build_quick.sh");
  await press("job-edit", QUICK);
  await ui.waitFor(() => editing().active === "src/build_quick.sh", 5000, "the job's file open");
  showView("edit");
  asFound();
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
