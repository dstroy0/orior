// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The pane the app shows once, on the first start in the system's session, while it reads the tree: the
// name standing over the window's lattice, glass over it as every view is. It stays at least SHORTEST
// milliseconds, so a fast read shows the pane whole instead of flashing it, and fades after. It is killed
// at once, its wait ended, where the window goes while it still shows, and where the app asks for that.
// The eye that follows the pointer (eye.js) is held back from it.

export const SHORTEST = 6000;
export const FADE = 350;

let shownAt = 0;
// How many times the pane has been shown: a hide leaves the pane where it has been shown since the hide
// began.
let shows = 0;
// The wait a hide is in, its timer and what ends it early, or null where none is.
let waiting = null;

// Ends the wait a hide is in, where one is.
function ended() {
  if (waiting) {
    clearTimeout(waiting.timer);
    const { resolve } = waiting;
    waiting = null;
    resolve();
  }
}

// Rests `ms` milliseconds, or less where the pane is shown again or killed in the meantime.
function rested(ms) {
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      waiting = null;
      resolve();
    }, ms);
    waiting = { timer, resolve };
  });
}

// The pane shown, and any hide it was in ended with the pane left shown.
export function showLoading() {
  const pane = document.getElementById("loading");
  shows += 1;
  ended();
  pane.hidden = false;
  pane.dataset.leaving = "false";
  shownAt = performance.now();
}

export async function hideLoading() {
  const pane = document.getElementById("loading");
  const shown = shows;
  const waited = performance.now() - shownAt;
  if (waited < SHORTEST) {
    await rested(SHORTEST - waited);
  }
  if (pane.hidden || shows !== shown) {
    return;
  }
  pane.dataset.leaving = "true";
  await rested(FADE);
  if (shows === shown) {
    pane.hidden = true;
  }
}

// The pane ended at once: any wait a hide is in let go and the pane hidden.
export function killLoading() {
  const pane = document.getElementById("loading");
  ended();
  pane.dataset.leaving = "true";
  pane.hidden = true;
}

window.addEventListener("pagehide", killLoading);
