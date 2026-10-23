// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Files of PAGED_FROM bytes or more, held as pages (ui/cli/src/editor/pages.c). The tree's side holds the file and the
// lines written over it, and an index of its lines; the page holds a window of the lines about the
// view, never more than MOST of them, as a file still being read is held: the view scrolls the whole
// file, and the lines outside the window stand as padding.
//
// The window moves as the view comes within NEAR lines of its edge, STEP lines read in on that side
// and, once it holds more than MOST, as many let go on the far side. A line the history still
// reaches is let go only past HARD lines, and the history with it. A jump to a line outside the
// window puts the window there. Each edit goes to the tree's side as it is made, the calls about one
// file one after another, and a save writes the file there. A paged file is not backed up as it
// changes, and its tab is never split.

import { invoke } from "./bridge.js";
import { Session } from "./editor/session.js";
import { say } from "./statusbar.js";

export const PAGED_FROM = 64 * 1024 * 1024;
const NEAR = 2000;
const STEP = 20000;
const MOST = 120000;
const HARD = 400000;

let opened = 0;

const rest = (ms) => new Promise((done) => setTimeout(done, ms));

// Runs `work`, a call about the paged file `paged`, once every call before it about the file is
// answered.
function along(paged, work) {
  const next = paged.chain.then(work);
  paged.chain = next.catch(() => {});
  return next;
}

// Opens the file at `path` paged, its window about the line `line`, in `language`. Gives its session.
export async function openPaged(path, line, language) {
  opened += 1;
  const key = `${path}#paged${opened}`;
  let shape = await invoke("pages_open", { key, path });
  // A line far down the file waits for the index to reach it.
  while (!shape.indexed && shape.lines <= line + NEAR) {
    await rest(25);
    shape = await invoke("pages_shape", { key });
  }
  const from = Math.max(0, Math.min(line - NEAR, shape.lines - 1));
  const to = Math.min(shape.lines, from + STEP);
  const lines = await invoke("pages_read", { key, from, to });
  const paged = { key, chain: Promise.resolve(), indexed: shape.indexed, moving: false };
  const session = new Session(lines.join("\n"), language, { base: from, window: { paged, start: 0, end: 0, size: shape.size, after: shape.lines - to } });
  paged.session = session;
  // Each edit goes to the tree's side in the file's lines, as the window stands when it is made.
  paged.unwatch = session.doc.watch(({ edits }) => {
    const base = session.base;
    const change = edits.map(({ from: a, to: b, text }) => ({ from: { line: a.line + base, col: a.col }, to: { line: b.line + base, col: b.col }, text }));
    along(paged, () => invoke("pages_edit", { key, changes: [change] })).catch((error) => say(`${path}: ${error}`, { failed: true }));
  });
  if (!shape.indexed) {
    followIndex(paged);
  }
  return session;
}

// Counts the lines below the window again as the index reads on, until it is whole.
async function followIndex(paged) {
  while (!paged.indexed && !paged.gone) {
    await rest(200);
    const shape = await along(paged, () => invoke("pages_shape", { key: paged.key })).catch(() => null);
    if (!shape) {
      return;
    }
    const s = paged.session;
    paged.indexed = shape.indexed;
    s.window.after = Math.max(0, shape.lines - s.base - s.doc.count);
    s.view?.size();
    s.view?.schedule();
  }
}

// Moves the window of the paged session `s` toward the view of `view` where the view has come
// within NEAR lines of its edge, reading STEP lines on that side and letting go of as many on the
// other once it holds more than MOST.
export async function follow(s, view) {
  const paged = s?.window?.paged;
  if (!paged || paged.moving || view.s !== s) {
    return;
  }
  const first = Math.floor(view.scrollY() / view.lineHeight);
  const last = first + Math.ceil(view.viewSize().height / view.lineHeight);
  const below = last > s.doc.count - NEAR && s.window.after > 0;
  const above = first < NEAR && s.base > 0;
  if (!below && !above) {
    return;
  }
  paged.moving = true;
  try {
    // A view that has gone past the window altogether puts the window where it is.
    if (first > s.doc.count || last < 0) {
      await jumpPaged(s, s.base + first);
      return;
    }
    if (below) {
      const from = s.base + s.doc.count;
      const lines = await along(paged, () => invoke("pages_read", { key: paged.key, from, to: from + Math.min(STEP, s.window.after) }));
      s.window.after = Math.max(0, s.window.after - lines.length);
      s.growLines(lines, false);
      trim(s, view, true);
    } else {
      const from = Math.max(0, s.base - STEP);
      const lines = await along(paged, () => invoke("pages_read", { key: paged.key, from, to: s.base }));
      s.growLines(lines, true);
      trim(s, view, false);
    }
  } finally {
    paged.moving = false;
  }
}

// Lets go of the window's lines past MOST on the side away from where it grew: those above it where
// it grew below, and below where above. Lines the history reaches stay until HARD.
function trim(s, view, grewBelow) {
  const over = s.doc.count - MOST;
  if (over <= 0) {
    return;
  }
  const first = Math.floor(view.scrollY() / view.lineHeight);
  const roomAbove = Math.max(0, first - NEAR);
  const roomBelow = Math.max(0, s.doc.count - first - Math.ceil(view.viewSize().height / view.lineHeight) - NEAR);
  const count = Math.min(over, grewBelow ? roomAbove : roomBelow);
  if (count <= 0) {
    return;
  }
  const reached = grewBelow ? (line) => line < count : (line) => line >= s.doc.count - count;
  if (s.doc.count < HARD && s.doc.reaches(reached)) {
    return;
  }
  if (grewBelow) {
    s.shrink(count, true);
  } else {
    s.window.after += count;
    s.shrink(count, false);
  }
}

// Puts the window of the paged session `s` about the file's line `line`, where it is outside it.
export async function jumpPaged(s, line) {
  const paged = s.window.paged;
  const total = s.base + s.doc.count + s.window.after;
  const from = Math.max(0, Math.min(line - NEAR, total - 1));
  const to = Math.min(total, from + STEP);
  const lines = await along(paged, () => invoke("pages_read", { key: paged.key, from, to }));
  s.window.after = Math.max(0, total - from - lines.length);
  s.replaceWindow(lines, from, line - from);
}

// Whether the file's line `line` lies in the window of the paged session `s`.
export const inWindow = (s, line) => line >= s.base && line < s.base + s.doc.count;

// The paged session `s`'s file's lines from `from` to before `to`, as the tree's side holds them once
// every edit before has gone there.
export function readPaged(s, from, to) {
  const paged = s.window.paged;
  return along(paged, () => invoke("pages_read", { key: paged.key, from, to }));
}

// Saves a paged file on the tree's side, once every edit before it has gone there.
export function savePaged(s) {
  const paged = s.window.paged;
  return along(paged, () => invoke("pages_save", { key: paged.key }));
}

// Lets go of a paged file on the tree's side, as its tab closes.
export function releasePaged(s) {
  const paged = s?.window?.paged;
  if (!paged) {
    return;
  }
  paged.gone = true;
  paged.unwatch();
  along(paged, () => invoke("pages_release", { key: paged.key })).catch(() => {});
}
