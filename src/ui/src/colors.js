// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The colors the canvases draw in, read from the stylesheet's variables: a theme reaches the eye,
// the plasma, the fuse and the lattice as it reaches the page. Each is read once and kept until the
// scheme or a theme changes.

import { onScheme } from "./scheme.js";

const kept = new Map();
let pen = null;

onScheme(() => kept.clear());

// A variable's color as { rgb: [red, green, blue] from 0 to 255, alpha from 0 to 1, text }, the
// text as a canvas takes it. A variable that holds no color reads as black.
function read(name) {
  let found = kept.get(name);
  if (!found) {
    const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    pen ??= document.createElement("canvas").getContext("2d");
    pen.fillStyle = "#000000";
    pen.fillStyle = value || "#000000";
    const text = String(pen.fillStyle);
    let rgb;
    let alpha = 1;
    if (text.startsWith("#")) {
      rgb = [1, 3, 5].map((at) => parseInt(text.slice(at, at + 2), 16));
    } else {
      const parts = (text.match(/[\d.]+/g) ?? ["0", "0", "0"]).map(Number);
      rgb = parts.slice(0, 3);
      alpha = parts[3] ?? 1;
    }
    found = { rgb, alpha, text };
    kept.set(name, found);
  }
  return found;
}

export const rgbOf = (name) => read(name).rgb;

export const cssOf = (name) => read(name).text;

// The variable's color with its alpha in place of its own, as CSS text.
export function withAlpha(name, alpha) {
  const [r, g, b] = read(name).rgb;
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}
