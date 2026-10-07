// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bar along the bottom of the window. On its left the branch the tree is on, with a star where a
// file differs from the last commit, then the runs going and the files still being read, then a word
// for a moment from what was last done, such as what a formatter said. On its right
// the editor's own line: where the cursor is, what is chosen, the indent, the line ends and the
// language, shown in the edit view only. Past the runs, what the app holds in memory: its own
// process and every process it started, the web view's among them, read again every MEMORY_EVERY
// while the window shows.

import { invoke } from "./bridge.js";

const SVG = "http://www.w3.org/2000/svg";

// A branch mark: two commits on one line and a third off it, joined.
function branchMark() {
  const svg = document.createElementNS(SVG, "svg");
  svg.setAttribute("viewBox", "0 0 16 16");
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("class", "branch-mark");
  const path = document.createElementNS(SVG, "path");
  path.setAttribute("d", "M5 3.5v9M5 10.5c0-3 6-2.5 6-5.5");
  svg.append(path);
  for (const [cx, cy] of [
    [5, 3],
    [5, 13],
    [11, 4.5],
  ]) {
    const circle = document.createElementNS(SVG, "circle");
    circle.setAttribute("cx", String(cx));
    circle.setAttribute("cy", String(cy));
    circle.setAttribute("r", "1.7");
    svg.append(circle);
  }
  return svg;
}

const MEMORY_EVERY = 2000;

const megabytes = (bytes) => {
  const mb = bytes / (1024 * 1024);
  return `${mb < 100 ? mb.toFixed(1) : Math.round(mb)} MB`;
};

// The total in RAM on the bar, and over it each program's share: its processes, what it holds in RAM
// and what it has reserved in all.
async function drawMemory() {
  const node = document.getElementById("status-memory");
  if (document.hidden) {
    return;
  }
  const read = await invoke("memory_use").catch(() => null);
  node.hidden = !read;
  if (!read) {
    return;
  }
  node.textContent = megabytes(read.working);
  const rows = read.parts.map((part) => `${part.name}${part.processes > 1 ? ` (${part.processes} processes)` : ""}: ${megabytes(part.working)}, ${megabytes(part.commit)} committed`);
  node.title = [`${megabytes(read.working)} in RAM, ${megabytes(read.commit)} committed`, ...rows].join("\n");
}

export function keepMemory() {
  drawMemory();
  window.setInterval(drawMemory, MEMORY_EVERY);
  document.addEventListener("visibilitychange", drawMemory);
}

export function drawBranch(branch, changed) {
  const node = document.getElementById("status-branch");
  node.hidden = !branch;
  if (branch) {
    node.replaceChildren(branchMark(), `${branch}${changed ? "*" : ""}`);
    node.title = branch;
  }
}

// How long a word on the bar stays, and a failure longer.
const SAID_FOR = 5000;
const FAILED_FOR = 15000;
let saidTimer = 0;

// Puts a word on the bar for a moment: its first line, and all of it over it. A press takes it away.
export function say(text, { failed = false } = {}) {
  const node = document.getElementById("status-said");
  window.clearTimeout(saidTimer);
  node.textContent = String(text).split("\n")[0];
  node.title = String(text);
  node.classList.toggle("failed", failed);
  node.hidden = false;
  node.onclick = () => (node.hidden = true);
  saidTimer = window.setTimeout(() => (node.hidden = true), failed ? FAILED_FOR : SAID_FOR);
}
