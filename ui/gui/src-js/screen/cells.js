// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A screen's cells and their colors: a style each cell is drawn in, a line of blank cells, the 256 colors, and a line
// or a text with escape sequences in it drawn as HTML in its colors.

// A style is shared by every cell drawn in it and never changed; a new one is made for each change.
export const PLAIN = Object.freeze({ fg: null, bg: null, bold: false, dim: false, italic: false, underline: false, inverse: false, strike: false });

export const blank = (style) => [" ", style.bg === null ? PLAIN : Object.freeze({ ...PLAIN, bg: style.bg })];

export function lineOf(cols, style = PLAIN) {
  const cell = blank(style);
  return { cells: Array.from({ length: cols }, () => cell), dirty: true };
}

// A color as the page draws it: one of the 16 by its class, any other as a CSS color.
function cube(index) {
  if (index >= 232) {
    const gray = 8 + (index - 232) * 10;
    return `rgb(${gray}, ${gray}, ${gray})`;
  }
  const at = index - 16;
  const part = (value) => (value ? 55 + value * 40 : 0);
  return `rgb(${part(Math.floor(at / 36))}, ${part(Math.floor(at / 6) % 6)}, ${part(at % 6)})`;
}

const escapeHtml = (text) => text.replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" })[c]);

// The markup of a run of cells in one style.
function runHtml(text, style, cursor) {
  let fg = style.fg;
  let bg = style.bg;
  const classes = [];
  const inline = [];
  if (style.inverse) {
    [fg, bg] = [bg ?? "default-bg", fg ?? "default-fg"];
  }
  for (const [color, type] of [
    [fg, "f"],
    [bg, "b"],
  ]) {
    if (color === null) {
      continue;
    }
    if (typeof color === "number" && color < 16) {
      classes.push(`t-${type}${color}`);
    } else if (color === "default-fg" || color === "default-bg") {
      classes.push(`t-${type}-${color}`);
    } else {
      inline.push(`${type === "f" ? "color" : "background"}:${typeof color === "number" ? cube(color) : color}`);
    }
  }
  for (const [on, name] of [
    [style.bold, "t-bold"],
    [style.dim, "t-dim"],
    [style.italic, "t-italic"],
    [style.underline, "t-under"],
    [style.strike, "t-strike"],
    [cursor, "t-cursor"],
  ]) {
    if (on) {
      classes.push(name);
    }
  }
  if (!classes.length && !inline.length) {
    return escapeHtml(text);
  }
  const classAttr = classes.length ? ` class="${classes.join(" ")}"` : "";
  const styleAttr = inline.length ? ` style="${inline.join(";")}"` : "";
  return `<span${classAttr}${styleAttr}>${escapeHtml(text)}</span>`;
}

// A style changed by a Select Graphic Rendition sequence's numbers.
export function restyled(from, numbers) {
  const style = { ...from };
  for (let at = 0; at < numbers.length; at += 1) {
    const value = numbers[at] ?? 0;
    if (value === 0) {
      Object.assign(style, PLAIN);
    } else if (value === 1) {
      style.bold = true;
    } else if (value === 2) {
      style.dim = true;
    } else if (value === 3) {
      style.italic = true;
    } else if (value === 4) {
      style.underline = true;
    } else if (value === 7) {
      style.inverse = true;
    } else if (value === 9) {
      style.strike = true;
    } else if (value === 22) {
      style.bold = false;
      style.dim = false;
    } else if (value === 23) {
      style.italic = false;
    } else if (value === 24) {
      style.underline = false;
    } else if (value === 27) {
      style.inverse = false;
    } else if (value === 29) {
      style.strike = false;
    } else if (value >= 30 && value <= 37) {
      style.fg = value - 30;
    } else if (value === 39) {
      style.fg = null;
    } else if (value >= 40 && value <= 47) {
      style.bg = value - 40;
    } else if (value === 49) {
      style.bg = null;
    } else if (value >= 90 && value <= 97) {
      style.fg = value - 90 + 8;
    } else if (value >= 100 && value <= 107) {
      style.bg = value - 100 + 8;
    } else if (value === 38 || value === 48) {
      const key = value === 38 ? "fg" : "bg";
      if (numbers[at + 1] === 5) {
        style[key] = numbers[at + 2] ?? 0;
        at += 2;
      } else if (numbers[at + 1] === 2) {
        // Some programs leave an empty color space before the three parts, as 38:2::r:g:b.
        const from = numbers[at + 2] === null ? at + 3 : at + 2;
        const [r, g, b] = numbers.slice(from, from + 3);
        style[key] = `rgb(${r ?? 0}, ${g ?? 0}, ${b ?? 0})`;
        at = from + 2;
      }
    }
  }
  return Object.freeze(style);
}

// A line of a run's output as the page draws it: in the colors and weight its sequences set, which
// start afresh on each line, with every other escape sequence read past.
export function coloredHtml(text) {
  const sequence = /\x1b(?:\[([\d;:]*)[ -/]*([@-~])|\][^\x07\x1b]*(?:\x07|\x1b\\)|[ -/]*[0-~])/g;
  let style = PLAIN;
  let html = "";
  let from = 0;
  for (const found of text.matchAll(sequence)) {
    html += text.slice(from, found.index) ? runHtml(text.slice(from, found.index), style, false) : "";
    if (found[2] === "m") {
      style = restyled(style, found[1] ? found[1].split(/[;:]/).map((part) => (part === "" ? null : Number(part))) : [0]);
    }
    from = found.index + found[0].length;
  }
  return html + (text.slice(from) ? runHtml(text.slice(from), style, false) : "");
}

// The lines of a text with color codes in it, each as its text with every escape sequence taken
// out and as the page draws it, in the colors and weight its sequences set, a style carried on from
// one line to the next.
export function coloredLines(lines) {
  const sequence = /\x1b(?:\[([\d;:]*)[ -/]*([@-~])|\][^\x07\x1b]*(?:\x07|\x1b\\)|[ -/]*[0-~])/g;
  let style = PLAIN;
  return lines.map((text) => {
    let plain = "";
    let html = "";
    let from = 0;
    const take = (part) => {
      if (part) {
        plain += part;
        html += runHtml(part, style, false);
      }
    };
    for (const found of text.matchAll(sequence)) {
      take(text.slice(from, found.index));
      if (found[2] === "m") {
        style = restyled(style, found[1] ? found[1].split(/[;:]/).map((part) => (part === "" ? null : Number(part))) : [0]);
      }
      from = found.index + found[0].length;
    }
    take(text.slice(from));
    return { text: plain, html };
  });
}

export function lineHtml(line, cursorAt) {
  let html = "";
  let start = 0;
  const cells = line.cells;
  for (let at = 1; at <= cells.length; at += 1) {
    const ends = at === cells.length || cells[at][1] !== cells[start][1] || at === cursorAt || start === cursorAt;
    if (!ends) {
      continue;
    }
    let text = "";
    for (let one = start; one < at; one += 1) {
      text += cells[one][0];
    }
    html += runHtml(text, cells[start][1], start === cursorAt);
    start = at;
  }
  return html || " ";
}

// A screen: its lines, and the rows of the page they are drawn in.
export function bufferOf(cols, rows, host) {
  const lines = Array.from({ length: rows }, () => lineOf(cols));
  const rowsOf = lines.map(() => host.appendChild(document.createElement("div")));
  return { lines, rows: rowsOf, host };
}
