// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window's zoom, a step at a time through STEPS and kept for the next time. Away from 100% the
// status bar shows it, and a click there sets it back.

import { invoke } from "./bridge.js";

const KEY = "orior.zoom";
export const STEPS = [0.5, 0.67, 0.75, 0.8, 0.9, 1, 1.1, 1.25, 1.5, 1.75, 2, 2.5, 3];

export function zoom() {
  const kept = Number(localStorage.getItem(KEY));
  return STEPS.includes(kept) ? kept : 1;
}

function drawZoom(factor) {
  const node = document.getElementById("status-zoom");
  node.hidden = factor === 1;
  node.textContent = `${Math.round(factor * 100)}%`;
}

export function setZoom(factor) {
  localStorage.setItem(KEY, String(factor));
  invoke("zoom_set", { factor }).catch(() => {});
  drawZoom(factor);
}

// Zooms in by 1, out by -1, and back to 100% by 0.
export function zoomBy(by) {
  if (by === 0) {
    setZoom(1);
    return;
  }
  const at = STEPS.indexOf(zoom());
  setZoom(STEPS[Math.max(0, Math.min(STEPS.length - 1, at + by))]);
}

export function keepZoom() {
  const node = document.getElementById("status-zoom");
  node.title = "Reset Zoom";
  node.addEventListener("click", () => setZoom(1));
  if (zoom() !== 1) {
    setZoom(zoom());
  }
}
