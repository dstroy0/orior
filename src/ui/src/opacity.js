// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The opacity of each part of the window, each a share in STEPS: how much of the ink the glass lays
// over the lattice on the bar, the strip and the status bar, how much of its own color every menu
// lays over what stands under it, and how much of its own ground each of the panes, the tabs, the
// editor, the panels and the sheets lays over the lattice. Each is the reader's to set in Preferences
// and kept for the next time; early.js sets them before the page's first frame from the same keys.

export const OPACITY_SETTINGS = [
  { name: "--bar-opacity", label: "Bar opacity", key: "orior.opacity.bar", fallback: 30 },
  { name: "--menu-opacity", label: "Menu opacity", key: "orior.opacity.menu", fallback: 100 },
  { name: "--pane-opacity", label: "Pane opacity", key: "orior.opacity.pane", fallback: 100 },
  { name: "--tab-opacity", label: "Tab opacity", key: "orior.opacity.tab", fallback: 100 },
  { name: "--editor-opacity", label: "Editor opacity", key: "orior.opacity.editor", fallback: 100 },
  { name: "--panel-opacity", label: "Panel opacity", key: "orior.opacity.panel", fallback: 100 },
  { name: "--sheet-opacity", label: "Sheet opacity", key: "orior.opacity.sheet", fallback: 100 },
];

export const STEPS = [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100];

// The share a setting stands at, in percent.
export function opacity(setting) {
  const kept = localStorage.getItem(setting.key);
  return STEPS.map(String).includes(kept) ? Number(kept) : setting.fallback;
}

// Shows each share in force without keeping it, as a settings file changes it.
export function showOpacity() {
  for (const setting of OPACITY_SETTINGS) {
    document.documentElement.style.setProperty(setting.name, `${opacity(setting)}%`);
  }
}

export function setOpacity(setting, percent) {
  localStorage.setItem(setting.key, String(percent));
  document.documentElement.style.setProperty(setting.name, `${percent}%`);
}
