// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A build, started from anywhere, shows in the build window, the run view with its job chosen, and
// says every command it runs as it runs it, unless its trace is quiet.

import { invoke } from "../../gui/src-js/bridge.js";
import { chosenJob, loadRun, startJob } from "../../gui/src-js/run.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const BUILD = "build/src/build_traced.sh";

// The console's text once the run shown has ended.
async function ended() {
  await ui.waitFor(() => document.querySelector("#job-stage .console-head .run[aria-current='true']")?.className.match(/\b(ok|failed)\b/), 15000);
  return document.getElementById("lines")?.textContent ?? "";
}

test("a build started from the editor shows in the build window, each command it runs traced, and quiet where asked", async () => {
  await invoke("file_write", { path: "src/build_traced.sh", text: "#!/usr/bin/env bash\nstep=one\necho \"built $step\"\n" });
  await loadRun();
  showView("edit");
  await startJob(BUILD, { trace: ["verbose"], arguments: [""] });
  assert.equal(document.body.dataset.view, "run", "the build window shows");
  assert.equal(chosenJob()?.id, BUILD, "with the build chosen");
  const traced = await ended();
  assert.match(traced, /SHELLOPTS=xtrace bash src\/build_traced\.sh/, "the command, traced");
  assert.match(traced, /\+ step=one/, "each command as bash runs it");
  assert.match(traced, /\+ echo 'built one'/);
  assert.match(traced, /built one/);
  showView("edit");
  await startJob(BUILD, { trace: ["quiet"], arguments: [""] });
  assert.equal(document.body.dataset.view, "run");
  const quiet = await ended();
  assert.ok(!/\+ step=one/.test(quiet), `quiet, no command traced: ${quiet}`);
  assert.match(quiet, /built one/, "and the script's own words");
});
