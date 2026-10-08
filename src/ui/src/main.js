// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The app's start: the scheme, the tree to work on, and the two views.

import { invoke, pick } from "./bridge.js";
import { forgetTree, openAt, openFile, restoreSession, startEdit } from "./edit.js";
import { keepLattices } from "./lattice.js";
import { hideLoading, showLoading } from "./loading.js";
import { startMenus } from "./menu.js";
import { drawMenubar, keysOf, runCommand, runLaunch, startMenubar } from "./menubar.js";
import { forgetFiles } from "./palette.js";
import { loadRun, startRun } from "./run.js";
import { catchErrors } from "./reports.js";
import { keepScheme } from "./scheme.js";
import { startSearch } from "./search.js";
import { keepPane, settlePanes } from "./sides.js";
import { watch } from "./status.js";
import { startTerminal } from "./terminal.js";
import { onView, showView, startModes } from "./views.js";
import { startWordmark } from "./wordmark.js";
import { keepZoom } from "./zoom.js";
import { keepMemory, say } from "./statusbar.js";
import { startMotion } from "./motion.js";
import { keepUserCss } from "./usercss.js";

async function openInEditor(path) {
  showView("edit");
  await openFile(path);
}

// Shows the tree the app works on, or the pane that asks for one.
async function settle(root, said) {
  const pane = document.getElementById("open-tree");
  document.getElementById("tree-path").textContent = root ?? "";
  pane.hidden = Boolean(root);
  if (!root) {
    document.getElementById("open-said").textContent = said ?? "";
    return false;
  }
  return true;
}

// The bar's reading of the status block: the runs going and the files still being read.
function drawPulse(held) {
  const parts = [];
  if (held.runs.size) {
    const runs = Object.assign(document.createElement("button"), { type: "button", textContent: `${held.runs.size} running` });
    runs.addEventListener("click", () => showView("run"));
    parts.push(runs);
  }
  for (const [path, { read, size }] of held.reads) {
    const name = path.split("/").pop();
    parts.push(Object.assign(document.createElement("span"), { textContent: `${name} ${Math.floor((100 * read) / Math.max(1, size))}%`, title: path }));
  }
  document.getElementById("pulse").replaceChildren(...parts);
}

async function start() {
  invoke("window_show").catch(() => {});
  catchErrors();
  await startMotion();
  startWordmark();
  keepScheme();
  await keepUserCss();
  keepZoom();
  keepMemory();
  startMenus();
  keepLattices();
  watch(drawPulse);
  document.getElementById("open-button").addEventListener("click", () => openFolder());
  document.getElementById("clone-button").addEventListener("click", () => runCommand("clone"));
  await startMenubar({ openFolder });
  startModes((view) => keysOf(`${view}-view`));
  startSearch((path, line, col) => {
    showView("edit");
    openAt(path, line, col);
  });
  await startRun(openInEditor);
  await startTerminal();
  keepPane(document.getElementById("job-side"), "left");
  keepPane(document.getElementById("explorer"), "left");
  keepPane(document.getElementById("defs-side"), "right");
  await begin(await invoke("root_get"));
  settlePanes();
  onView(settlePanes);
  await runLaunch();
}

// Works on the tree in `folder`, or in one asked for, in place of the one open. A folder that is no
// tree is said on the pane that asks for one, and on the bar where a tree is open over that pane.
async function openFolder(folder) {
  const chosen = folder ?? (await pick("dir"));
  if (typeof chosen !== "string") {
    return;
  }
  try {
    const root = await invoke("root_set", { path: chosen });
    forgetTree();
    forgetFiles();
    await begin(root);
  } catch (error) {
    document.getElementById("open-said").textContent = String(error);
    if (document.getElementById("open-tree").hidden) {
      say(String(error), { failed: true });
    }
  }
}

// Says which of the toolchains every job needs, as toolchains.json marks them, orior cannot find,
// a press on the word opening File, Toolchains.
async function checkNeeded() {
  const read = await invoke("toolchains_check").catch(() => null);
  const missing = (read?.tools ?? []).filter((tool) => tool.needed && tool.state === "missing").map((tool) => tool.name);
  if (missing.length) {
    const names = missing.length > 1 ? `${missing.slice(0, -1).join(", ")} and ${missing.at(-1)}` : missing[0];
    say(`${names} not found, and the jobs need ${missing.length > 1 ? "them" : "it"}: press here for File, Toolchains`, { failed: true, act: () => runCommand("toolchains") });
  }
}

let started = false;

// Reads the tree behind the eye, and takes the eye away whether the read succeeds or fails.
async function begin(root) {
  showLoading();
  try {
    if (!(await settle(root))) {
      return;
    }
    const defs = await invoke("definitions_read");
    if (!started) {
      started = true;
      await startEdit(defs);
    }
    await loadRun();
    drawMenubar();
    await restoreSession();
    checkNeeded();
  } finally {
    await hideLoading();
  }
}

start();
