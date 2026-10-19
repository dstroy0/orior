// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The cores kept for the reader as orior's own work spreads over the machine's: KEPT_LEAST at the
// fewest, the reader's to raise in Preferences or as `cores-kept` in their settings file, which a
// tree's file does not set. The window gives the count to orior's side as it starts and each time it
// changes, and the search, the tests and the reading of a tree's files spread over what is left.

import { invoke } from "./bridge.js";

export const CORES_KEY = "orior.cores-kept";

export const KEPT_LEAST = 1;

// The cores the machine has, as the page is told.
export function machineCores() {
  return navigator.hardwareConcurrency || 1;
}

// The cores kept for the reader, KEPT_LEAST where nothing kept says more.
export function coresKept() {
  const kept = Number(localStorage.getItem(CORES_KEY));
  return Number.isInteger(kept) && kept >= KEPT_LEAST ? kept : KEPT_LEAST;
}

// Gives orior's side the cores kept, and answers what it took: the cores kept, those its work may
// take, and the machine's.
export function giveCores() {
  return invoke("cores_keep", { kept: coresKept() }).catch(() => null);
}

export function setCoresKept(kept) {
  localStorage.setItem(CORES_KEY, String(Math.max(KEPT_LEAST, Math.round(kept))));
  return giveCores();
}
