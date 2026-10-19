// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fonts: the interface's, the reading text's and the code's, each a CSS font list, and the
// code's size, from LEAST to MOST pixels. Each is the reader's to set over the stylesheet's own and
// kept apart from the color themes; early.js sets them before the page's first frame from the same
// key. The code's line height follows its size at LINE_PER_SIZE, rounded to a whole pixel.

export const FONTS = "orior.fonts";

export const FONT_SETTINGS = [
  { name: "--head", label: "Interface font" },
  { name: "--text", label: "Reading font" },
  { name: "--code", label: "Code font" },
  { name: "--code-size", label: "Code size", size: true },
];

const LEAST = 9;
const MOST = 32;
const LINE_PER_SIZE = 20 / 13;

const listeners = [];

export function onFonts(listener) {
  listeners.push(listener);
}

// The fonts the reader set, by variable.
export function fonts() {
  try {
    const kept = JSON.parse(localStorage.getItem(FONTS) ?? "{}");
    return kept && typeof kept === "object" ? kept : {};
  } catch {
    return {};
  }
}

// The stylesheet's own value of a font variable.
export function fontDefault(name) {
  for (const sheet of document.styleSheets) {
    let rules;
    try {
      rules = sheet.cssRules;
    } catch {
      continue;
    }
    for (const rule of rules) {
      if (rule.selectorText === ":root") {
        const value = rule.style.getPropertyValue(name).trim();
        if (value) {
          return value;
        }
      }
    }
  }
  return "";
}

// The rule that sets the reader's fonts, and the code's line height with its size.
export function fontRule(set) {
  const lines = [];
  for (const { name, size } of FONT_SETTINGS) {
    const value = String(set[name] ?? "").trim();
    if (!value || /[;{}]/.test(value)) {
      continue;
    }
    if (size) {
      const px = Number.parseFloat(value);
      if (!(px >= LEAST && px <= MOST)) {
        continue;
      }
      lines.push(`--code-size: ${px}px;`, `--code-line: ${Math.round(px * LINE_PER_SIZE)}px;`);
    } else {
      lines.push(`${name}: ${value};`);
    }
  }
  return lines.length ? `:root[data-scheme] { ${lines.join(" ")} }` : "";
}

export function applyFonts() {
  let node = document.getElementById("fonts");
  if (!node) {
    node = Object.assign(document.createElement("style"), { id: "fonts" });
    document.head.append(node);
  }
  node.textContent = fontRule(fonts());
  listeners.forEach((listener) => listener());
}

// Sets one font, or with an empty value puts the stylesheet's back.
export function setFont(name, value) {
  const set = fonts();
  const text = String(value ?? "").trim();
  if (text) {
    set[name] = text;
  } else {
    delete set[name];
  }
  localStorage.setItem(FONTS, JSON.stringify(set));
  applyFonts();
}
