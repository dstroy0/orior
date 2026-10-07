// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The app's start: the scheme, the tree to work on, and the two views.

import { invoke, pick } from "./bridge.js";
import { forgetTree, openFile, startEdit } from "./edit.js";
import { keepLattices } from "./lattice.js";
import { hideLoading, showLoading } from "./loading.js";
import { loadRun, startRun } from "./run.js";
import { keepScheme } from "./scheme.js";
import { watch } from "./status.js";
import { startTerminal } from "./terminal.js";

function mode(name) {
  document.querySelectorAll(".modes button").forEach((button) => button.setAttribute("aria-selected", String(button.dataset.mode === name)));
  document.querySelectorAll(".mode").forEach((section) => (section.dataset.active = String(section.id === `mode-${name}`)));
}

async function openInEditor(path) {
  mode("edit");
  await openFile(path);
}

// Shows the tree the app works on, or the pane that asks for one.
async function settle(root, said) {
  const pane = document.getElementById("open-tree");
  document.getElementById("tree-path").textContent = root ?? "";
  pane.hidden = Boolean(root);
  document.querySelector(".modes").hidden = !root;
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
    runs.addEventListener("click", () => mode("run"));
    parts.push(runs);
  }
  for (const [path, { read, size }] of held.reads) {
    const name = path.split("/").pop();
    parts.push(Object.assign(document.createElement("span"), { textContent: `${name} ${Math.floor((100 * read) / Math.max(1, size))}%`, title: path }));
  }
  document.getElementById("pulse").replaceChildren(...parts);
}

async function start() {
  keepScheme();
  keepLattices();
  watch(drawPulse);
  document.querySelectorAll(".modes button").forEach((button) => button.addEventListener("click", () => mode(button.dataset.mode)));
  document.getElementById("open-button").addEventListener("click", async () => {
    const chosen = await pick("dir");
    if (typeof chosen !== "string") {
      return;
    }
    try {
      const root = await invoke("root_set", { path: chosen });
      forgetTree();
      await begin(root);
    } catch (error) {
      document.getElementById("open-said").textContent = String(error);
    }
  });
  await startRun(openInEditor);
  await startTerminal();
  await begin(await invoke("root_get"));
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
  } finally {
    await hideLoading();
  }
}

start();
