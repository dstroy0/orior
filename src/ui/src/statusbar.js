// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bar along the bottom of the window. On its left the breadcrumbs of the open file, then the runs
// going and the files still being read, then a word for a moment from what was last done, such as
// what a formatter said, which the top bar's bell keeps. On its right what the app holds in memory:
// its own process and every process it started, the web view's among them, read again every
// MEMORY_EVERY while the window shows; then the editor's own line, shown in the edit view only:
// where the cursor is as line:column, what is chosen, the line ends, the encoding, the indent, and
// the lock. The branch the tree is on, with a star where a file differs from the last commit, is on
// the top bar beside the tree's name.

import { invoke } from "./bridge.js";
import { icon } from "./icons.js";
import { still } from "./motion.js";

const MEMORY_EVERY = 2000;

const megabytes = (bytes) => {
  const mb = bytes / (1024 * 1024);
  return `${mb < 100 ? mb.toFixed(1) : Math.round(mb)} MB`;
};

// The total in RAM on the bar, and over it each program's share: its processes, what it holds in RAM
// and what it has reserved in all.
async function drawMemory() {
  const node = document.getElementById("status-memory");
  if (still()) {
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
    node.replaceChildren(icon("git"), `${branch}${changed ? "*" : ""}`);
    node.title = branch;
  }
}

// How long a word on the bar stays, and a failure longer.
const SAID_FOR = 5000;
const FAILED_FOR = 15000;
let saidTimer = 0;

// What the bar has said, the newest last, which the top bar's bell lists, and how many of them came
// since it was last opened.
const NOTICES_KEPT = 200;
const notices = [];
let unseen = 0;
const noticeListeners = [];

export function noticesSaid() {
  return notices;
}

export function noticesUnseen() {
  return unseen;
}

export function noticesSeen() {
  unseen = 0;
  noticeListeners.forEach((listener) => listener());
}

export function clearNotices() {
  notices.length = 0;
  noticesSeen();
}

export function onNotice(listener) {
  noticeListeners.push(listener);
}

// Puts a word on the bar for a moment: its first line, and all of it over it. A press takes it away,
// and does `act` first where there is one. The bell keeps it.
export function say(text, { failed = false, act = null } = {}) {
  notices.push({ text: String(text), failed, at: Date.now() });
  if (notices.length > NOTICES_KEPT) {
    notices.shift();
  }
  unseen += 1;
  noticeListeners.forEach((listener) => listener());
  const node = document.getElementById("status-said");
  window.clearTimeout(saidTimer);
  node.textContent = String(text).split("\n")[0];
  node.title = String(text);
  node.classList.toggle("failed", failed);
  node.hidden = false;
  node.onclick = () => {
    node.hidden = true;
    act?.();
  };
  saidTimer = window.setTimeout(() => (node.hidden = true), failed ? FAILED_FOR : SAID_FOR);
}
