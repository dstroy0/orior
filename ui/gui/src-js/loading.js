// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The pane the app shows while it reads a tree: the lattice with the eye over it. It stays at least
// SHORTEST milliseconds, long enough for the eye to follow the pointer before it rolls, so a fast
// read shows the eye whole instead of flashing it. It is killed at once, its wait and its eye both
// ended, where the window goes while it still shows, and where the app asks for that.

import { startEye } from "./eye.js";
import { drawLattice } from "./lattice.js";

export const SHORTEST = 10000;
export const FADE = 350;

let stop = null;
let shownAt = 0;
// The wait a hide is in, its timer and what ends it early, or null where none is.
let waiting = null;

// Rests `ms` milliseconds, or less where the pane is killed in the meantime.
function rested(ms) {
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      waiting = null;
      resolve();
    }, ms);
    waiting = { timer, resolve };
  });
}

export function showLoading() {
  const pane = document.getElementById("loading");
  pane.hidden = false;
  pane.dataset.leaving = "false";
  shownAt = performance.now();
  drawLattice(pane.querySelector("canvas.lattice"));
  stop?.();
  stop = startEye(pane.querySelector("canvas.eye"));
}

export async function hideLoading() {
  const pane = document.getElementById("loading");
  const waited = performance.now() - shownAt;
  if (waited < SHORTEST) {
    await rested(SHORTEST - waited);
  }
  if (pane.hidden) {
    return;
  }
  pane.dataset.leaving = "true";
  await rested(FADE);
  pane.hidden = true;
  stop?.();
  stop = null;
}

// The pane ended at once: any wait a hide is in let go, the eye stopped and the pane hidden.
export function killLoading() {
  const pane = document.getElementById("loading");
  if (waiting) {
    clearTimeout(waiting.timer);
    const { resolve } = waiting;
    waiting = null;
    resolve();
  }
  stop?.();
  stop = null;
  pane.dataset.leaving = "true";
  pane.hidden = true;
}

window.addEventListener("pagehide", killLoading);
