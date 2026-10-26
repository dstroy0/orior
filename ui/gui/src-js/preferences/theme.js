// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The themes Preferences shows and changes: the one shown, a new one, one chosen, renamed, copied out and read
// back in, a color of its palette, the image behind the editor and the files' icons.

import { invoke } from "../bridge.js";
import { clipText, copyText } from "../menu.js";
import { scheme, setScheme } from "../scheme.js";
import { BASES, choose, chosen, freeName, IMAGE_OPACITY, paletteOf, readTheme, saveTheme, themeNamed, themeText } from "../themes.js";

export const BASE_NAMES = { dark: "Dark", light: "Light" };

// The shares of the image behind the editor to choose from.
export const IMAGE_SHARES = [0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.75, 1];

// A color as the swatch takes it, #rrggbb, and the alpha the swatch cannot show.
const probe = document.createElement("canvas").getContext("2d");
export function swatchOf(value) {
  probe.fillStyle = "#000000";
  probe.fillStyle = value;
  const read = probe.fillStyle;
  if (read.startsWith("#")) {
    return { hex: read, alpha: 1 };
  }
  const parts = read.match(/[\d.]+/g)?.map(Number) ?? [0, 0, 0, 1];
  const hex = `#${parts
    .slice(0, 3)
    .map((part) => Math.round(part).toString(16).padStart(2, "0"))
    .join("")}`;
  return { hex, alpha: parts[3] ?? 1 };
}

export function withAlpha(hex, alpha) {
  if (alpha >= 1) {
    return hex;
  }
  const [r, g, b] = [1, 3, 5].map((at) => parseInt(hex.slice(at, at + 2), 16));
  return `rgba(${r}, ${g}, ${b}, ${Number(alpha.toFixed(3))})`;
}

// The groups the palette's names fall in, by what they start with.
export function groupOf(name) {
  if (/^--t\d+$/.test(name)) {
    return "--t0 … --t15";
  }
  if (name.startsWith("--t-")) {
    return "--t-*";
  }
  if (name.startsWith("--ed-")) {
    return "--ed-*";
  }
  return "--*";
}

// The theme shown: the scheme's chosen theme of the reader's, or null for the scheme's own.
export function shownTheme() {
  const name = chosen(scheme());
  return name ? themeNamed(name) : null;
}

// A theme of the reader's to change: the one shown, or a new one from the scheme's own colors.
function ownTheme() {
  const theme = shownTheme();
  if (theme) {
    return theme;
  }
  const made = { name: freeName(`${BASE_NAMES[scheme()]} 2`), base: scheme(), colors: {} };
  saveTheme(made);
  choose(made.base, made.name);
  return made;
}

// A theme of the reader's from the one shown, or from the scheme's own colors, chosen.
export function newTheme() {
  const from = shownTheme();
  const made = { name: freeName(`${from?.name ?? BASE_NAMES[scheme()]} 2`), base: scheme(), colors: { ...(from?.colors ?? {}) } };
  saveTheme(made);
  choose(made.base, made.name);
}

// The scheme's own theme, or a theme of the reader's, chosen by its name.
export function chooseTheme(value) {
  if (BASES.includes(value)) {
    setScheme(value);
    choose(value, null);
    return true;
  }
  const picked = themeNamed(value);
  if (!picked) {
    return false;
  }
  setScheme(picked.base);
  choose(picked.base, picked.name);
  return true;
}

// The theme shown, renamed `wanted` where that is free.
export function renameTheme(wanted) {
  const was = shownTheme();
  if (was && wanted && wanted !== was.name) {
    saveTheme({ ...was, name: freeName(wanted) }, was.name);
  }
}

export function exportTheme() {
  copyText(themeText(shownTheme() ?? { name: BASE_NAMES[scheme()], base: scheme(), colors: {} }));
  return "Copied to the clipboard.";
}

export async function importTheme() {
  try {
    const read = readTheme(await clipText());
    saveTheme(read);
    setScheme(read.base);
    choose(read.base, read.name);
    return { text: `Imported ${read.name}.` };
  } catch (error) {
    return { text: error instanceof SyntaxError ? "The clipboard holds no theme." : String(error.message ?? error), failed: true };
  }
}

// A color of the palette set in the theme of the reader's, or with `next` null put back to the
// scheme's own.
export function setColor(name, next) {
  const base = paletteOf(scheme());
  const own = ownTheme();
  const colors = { ...own.colors };
  if (next === null || next === base.get(name)) {
    delete colors[name];
  } else {
    colors[name] = next;
  }
  saveTheme({ ...own, colors });
  return colors;
}

// The image behind the editor: read from the file at `path`, its share, and taken off.
export async function chooseImage(path) {
  const src = await invoke("image_data", { path });
  const own = ownTheme();
  saveTheme({ ...own, image: { src, opacity: own.image?.opacity ?? IMAGE_OPACITY } });
}

export function shareImage(share) {
  const own = ownTheme();
  saveTheme({ ...own, image: { ...own.image, opacity: share } });
}

export function removeImage() {
  const own = { ...ownTheme() };
  delete own.image;
  saveTheme(own);
}

export const keepIcons = (next) => saveTheme({ ...ownTheme(), icons: next });
