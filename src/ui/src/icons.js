// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The app's icons: the shapes an IDE's chrome is read by, a folder for the files, a branch for git,
// a bell for notifications, each drawn on a 16-unit square in one weight of line with round ends, in
// the color of the text around it. The parts of a shape that are filled in, such as dots, are in
// FILLS.

// A cog's outline: eight teeth around a wheel, each tooth's top a little narrower than its root.
function cog() {
  const teeth = 8;
  const [outer, inner] = [6.6, 4.9];
  const at = (radius, turn) => `${(8 + radius * Math.cos(turn)).toFixed(2)} ${(8 + radius * Math.sin(turn)).toFixed(2)}`;
  const step = (2 * Math.PI) / teeth;
  let d = "";
  for (let tooth = 0; tooth < teeth; tooth += 1) {
    const mid = tooth * step;
    d += `${tooth ? "L" : "M"}${at(inner, mid - step * 0.32)}L${at(outer, mid - step * 0.18)}L${at(outer, mid + step * 0.18)}L${at(inner, mid + step * 0.32)}`;
  }
  return `${d}Z`;
}

const SHAPES = {
  folder: ["M2 4.6A1.6 1.6 0 0 1 3.6 3h2.8l1.6 1.6h4.4A1.6 1.6 0 0 1 14 6.2v5.2a1.6 1.6 0 0 1-1.6 1.6H3.6A1.6 1.6 0 0 1 2 11.4z"],
  structure: ["M2.5 2.5h4v3h-4zM9.5 6.5h4v3h-4zM9.5 11h4v3h-4z", "M4.5 5.5v7h5M4.5 8h5"],
  commit: ["M1.5 8h4M10.5 8h4", "M8 5.4a2.6 2.6 0 1 1 0 5.2a2.6 2.6 0 0 1 0-5.2z"],
  more: [],
  run: ["M2 13.5h12", "M3.2 10.8 6.4 7.3l2.7 2.2 3.7-4.8"],
  debug: ["M8 1.8l5.4 3.1v6.2L8 14.2l-5.4-3.1V4.9z", "M6.7 5.9v4.2L10.1 8z"],
  terminal: ["M3.4 3h9.2A1.4 1.4 0 0 1 14 4.4v7.2a1.4 1.4 0 0 1-1.4 1.4H3.4A1.4 1.4 0 0 1 2 11.6V4.4A1.4 1.4 0 0 1 3.4 3z", "M5 6.4 7 8l-2 1.6M8.6 10h2.6"],
  problems: ["M8 2a6 6 0 1 1 0 12A6 6 0 0 1 8 2z", "M8 4.9v3.8"],
  git: ["M5 3.6v8.8", "M5 10c0-2.6 6-2.2 6-5"],
  tests: ["M6.2 2h3.6", "M6.8 2v4.3L3.1 12.5A1 1 0 0 0 4 14h8a1 1 0 0 0 .9-1.5L9.2 6.3V2", "M4.7 10.4h6.6"],
  gear: [cog(), "M8 5.9a2.1 2.1 0 1 1 0 4.2a2.1 2.1 0 0 1 0-4.2z"],
  bell: ["M4.2 11.2V7.6a3.8 3.8 0 0 1 7.6 0v3.6l1.2 1.4H3z", "M6.6 13.9a1.5 1.5 0 0 0 2.8 0"],
  database: ["M3 4.2C3 3 5.2 2 8 2s5 1 5 2.2-2.2 2.2-5 2.2-5-1-5-2.2z", "M3 4.2v7.6C3 13 5.2 14 8 14s5-1 5-2.2V4.2", "M3 8c0 1.2 2.2 2.2 5 2.2s5-1 5-2.2"],
  lock: ["M4.6 7.4h6.8a.9.9 0 0 1 .9.9v4.6a.9.9 0 0 1-.9.9H4.6a.9.9 0 0 1-.9-.9V8.3a.9.9 0 0 1 .9-.9z", "M5.9 7.4V5.6a2.1 2.1 0 0 1 4.2 0v1.8"],
  unlock: ["M4.6 7.4h6.8a.9.9 0 0 1 .9.9v4.6a.9.9 0 0 1-.9.9H4.6a.9.9 0 0 1-.9-.9V8.3a.9.9 0 0 1 .9-.9z", "M5.9 7.4V5.6a2.1 2.1 0 0 1 4-.9"],
  chevron: ["M6 4l4 4-4 4"],
  refresh: ["M13 8a5 5 0 1 1-1.5-3.6", "M13 2.4v3.2H9.8"],
  undo: ["M5.5 4 2.5 7l3 3", "M2.8 7H10a3.5 3.5 0 0 1 0 7H7"],
  pull: ["M8 2.5v8M4.8 7.6 8 10.8l3.2-3.2", "M3 13.5h10"],
  push: ["M8 13.5v-8M4.8 8.4 8 5.2l3.2 3.2", "M3 2.5h10"],
  minimize: ["M3.5 8h9"],
  maximize: ["M4 4h8v8H4z"],
  restore: ["M4 6h6v6H4z", "M6 6V4h6v6h-2"],
  close: ["M4.2 4.2l7.6 7.6M11.8 4.2l-7.6 7.6"],
  protocol: ["M3.5 2.5v11M12.5 2.5v11", "M3.5 5.5h7.4M9.4 4l1.5 1.5-1.5 1.5", "M12.5 10.5H5.1M6.6 9l-1.5 1.5 1.5 1.5"],
  ingest: ["M8 2.2v7.3M5 6.6l3 3 3-3", "M2.5 10.2v1.9a1.4 1.4 0 0 0 1.4 1.4h8.2a1.4 1.4 0 0 0 1.4-1.4v-1.9"],
  render: ["M3.4 3h9.2A1.4 1.4 0 0 1 14 4.4v7.2a1.4 1.4 0 0 1-1.4 1.4H3.4A1.4 1.4 0 0 1 2 11.6V4.4A1.4 1.4 0 0 1 3.4 3z", "M2.4 11.4 6 7.8l2.6 2.6 1.6-1.6 3.4 3.4"],
  sim: ["M1.8 8.6C3.3 3.4 4.9 3.4 6.4 8.2s3.2 4.8 4.7.4c.9-2.6 2-3.4 3.1-2.6"],
  pipeline: ["M1.5 6h3.2v4H1.5zM6.4 6h3.2v4H6.4zM11.3 6h3.2v4h-3.2z", "M4.7 8h1.7M9.6 8h1.7"],
  stage: ["M8 2.3 14 5.3 8 8.3 2 5.3z", "M2 8.1l6 3 6-3", "M2 10.9l6 3 6-3"],
};

// The filled parts: dots and the like, as circles [cx, cy, r].
const FILLS = {
  more: [[3.5, 8, 1.2], [8, 8, 1.2], [12.5, 8, 1.2]],
  problems: [[8, 11.1, 0.9]],
  git: [[5, 3, 1.5], [5, 13, 1.5], [11, 4.6, 1.5]],
  render: [[10.6, 6.1, 1.1]],
};

const SVG = "http://www.w3.org/2000/svg";

// The icon named `name` as an SVG element, or a letter in its place where `name` is one character,
// as the m that marks the definitions.
export function icon(name) {
  if (name.length === 1) {
    return Object.assign(document.createElement("span"), { className: "icon-letter", textContent: name, ariaHidden: "true" });
  }
  const svg = document.createElementNS(SVG, "svg");
  svg.setAttribute("viewBox", "0 0 16 16");
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("class", `glyph glyph-${name}`);
  for (const d of SHAPES[name] ?? []) {
    const path = document.createElementNS(SVG, "path");
    path.setAttribute("d", d);
    svg.append(path);
  }
  for (const [cx, cy, r] of FILLS[name] ?? []) {
    const dot = document.createElementNS(SVG, "circle");
    dot.setAttribute("cx", String(cx));
    dot.setAttribute("cy", String(cy));
    dot.setAttribute("r", String(r));
    dot.setAttribute("class", "fill");
    svg.append(dot);
  }
  return svg;
}
