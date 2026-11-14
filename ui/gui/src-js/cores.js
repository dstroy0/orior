// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The cores retained for the reader as orior's own work spreads over the machine's: RETAINED_LEAST at the
// fewest, the reader's to raise in Preferences or as `cores-reserved` in their settings file, which a
// tree's file does not set. The window gives the count to orior's side as it starts and each time it
// changes, and the search, the tests and the reading of a tree's files spread over what is left.

import { invoke } from "./bridge.js";

export const CORES_KEY = "orior.cores-reserved";

export const RETAINED_LEAST = 1;

// The cores the machine has, as the page is told.
export function machineCores() {
  return navigator.hardwareConcurrency || 1;
}

// The cores retained for the reader, RETAINED_LEAST where nothing retained says more.
export function coresKept() {
  const retained = Number(localStorage.getItem(CORES_KEY));
  return Number.isInteger(retained) && retained >= RETAINED_LEAST ? retained : RETAINED_LEAST;
}

// Gives orior's side the cores retained, and answers what it took: the cores retained, those its work may
// take, and the machine's.
export function giveCores() {
  return invoke("cores_reserve", { reserved: coresKept() }).catch(() => null);
}

export function setCoresKept(retained) {
  localStorage.setItem(CORES_KEY, String(Math.max(RETAINED_LEAST, Math.round(retained))));
  return giveCores();
}
