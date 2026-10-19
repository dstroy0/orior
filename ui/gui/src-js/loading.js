// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The pane the app shows while it reads a tree: the lattice with the eye over it. It stays at least
// SHORTEST milliseconds. A fast read then shows the eye open instead of flashing it.

import { startEye } from "./eye.js";
import { drawLattice } from "./lattice.js";

const SHORTEST = 900;
const FADE = 350;

let stop = null;
let shownAt = 0;

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
    await new Promise((resolve) => setTimeout(resolve, SHORTEST - waited));
  }
  pane.dataset.leaving = "true";
  await new Promise((resolve) => setTimeout(resolve, FADE));
  pane.hidden = true;
  stop?.();
  stop = null;
}
