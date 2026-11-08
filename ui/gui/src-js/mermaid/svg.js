// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What every diagram is drawn with: text made safe for SVG, a label's width in the diagrams' font and its lines, a
// label drawn, a line of a diagram's text with its comment cut, and the SVG that holds a drawing.

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
export const escape = (text) => String(text).replace(/[&<>"]/g, (letter) => ESCAPES[letter]);

const FONT = "14px system-ui, sans-serif";

let measurer = null;
// The width `text` takes in the diagrams' font, in pixels.
export function width(text) {
  measurer ??= document.createElement("canvas").getContext("2d");
  measurer.font = FONT;
  return measurer.measureText(text).width;
}

// A label's lines: <br> and \n each start a new one.
export const linesOf = (text) => String(text).split(/<br\s*\/?>|\\n/i);
export const widest = (text) => Math.max(...linesOf(text).map(width));

export function textAt(x, y, text, anchor = "middle", type = "mm-text") {
  const lines = linesOf(text);
  const top = y - ((lines.length - 1) * 17) / 2;
  return lines.map((line, at) => `<text class="${type}" x="${x}" y="${top + at * 17}" text-anchor="${anchor}" dominant-baseline="central">${escape(line)}</text>`).join("");
}

export const lineOf = (text) => text.replace(/%%.*$/, "").trim();

export function svg(w, h, inner, type) {
  const defs = `<defs><marker id="mm-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path class="mm-head" d="M0,0 L10,5 L0,10 z" /></marker><marker id="mm-arrow-back" viewBox="0 0 10 10" refX="1" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path class="mm-head" d="M0,0 L10,5 L0,10 z" /></marker><marker id="mm-circle" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7"><circle class="mm-head" cx="5" cy="5" r="4" /></marker><marker id="mm-cross" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="8" markerHeight="8"><path class="mm-edge" d="M1,1 L9,9 M9,1 L1,9" /></marker><marker id="mm-triangle" viewBox="0 0 12 12" refX="11" refY="6" markerWidth="12" markerHeight="12" orient="auto-start-reverse"><path class="mm-hollow" d="M1,1 L11,6 L1,11 z" /></marker><marker id="mm-diamond" viewBox="0 0 14 8" refX="13" refY="4" markerWidth="14" markerHeight="8" orient="auto-start-reverse"><path class="mm-head" d="M1,4 L7,1 L13,4 L7,7 z" /></marker><marker id="mm-diamond-open" viewBox="0 0 14 8" refX="13" refY="4" markerWidth="14" markerHeight="8" orient="auto-start-reverse"><path class="mm-hollow" d="M1,4 L7,1 L13,4 L7,7 z" /></marker></defs>`;
  return `<svg class="mermaid-diagram ${type}" xmlns="http://www.w3.org/2000/svg" width="${Math.ceil(w)}" height="${Math.ceil(h)}" viewBox="0 0 ${Math.ceil(w)} ${Math.ceil(h)}" role="img">${defs}${inner}</svg>`;
}
