// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the theme tests share: a theme made and chosen, and taken away again, and what of Preferences answers a
// selector in the sheet open now.

import { scheme } from "../gui/src-js/scheme.js";
import { choose, deleteTheme, saveTheme } from "../gui/src-js/themes.js";

// What of Preferences answers `selector` in the sheet open now: a sheet closed a moment before stays on the
// page until its close is heard, with its own rows.
export const inSheet = (selector) => [...(document.querySelector("dialog.sheet[open]")?.querySelectorAll(selector) ?? [])];

// Makes the theme `name` over the scheme shown, from `parts`, and chooses it.
export function useTheme(name, parts) {
  saveTheme({ name, base: scheme(), colors: {}, ...parts });
  choose(scheme(), name);
}

// The window's own colors chosen again, and the theme `name` taken away.
export function leave(name) {
  choose(scheme(), null);
  deleteTheme(name);
}
