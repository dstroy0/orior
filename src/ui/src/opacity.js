// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bar opacity and the menu opacity, each a share in STEPS: how much of the ink the glass lays
// over the lattice on the bar, the strip and the status bar, and how much of its own color every
// menu lays over what stands under it. Each is the reader's to set in Preferences and kept for the
// next time; early.js sets them before the page's first frame from the same keys.

export const OPACITY_SETTINGS = [
  { name: "--bar-opacity", label: "Bar opacity", key: "orior.opacity.bar", fallback: 30 },
  { name: "--menu-opacity", label: "Menu opacity", key: "orior.opacity.menu", fallback: 100 },
];

export const STEPS = [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100];

// The share a setting stands at, in percent.
export function opacity(setting) {
  const kept = localStorage.getItem(setting.key);
  return kept !== null && STEPS.includes(Number(kept)) ? Number(kept) : setting.fallback;
}

export function setOpacity(setting, percent) {
  localStorage.setItem(setting.key, String(percent));
  document.documentElement.style.setProperty(setting.name, `${percent}%`);
}
