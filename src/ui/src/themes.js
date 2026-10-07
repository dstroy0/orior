// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Color themes. The palette is every color the stylesheet gives the dark and the light scheme, read
// from its own rules, and the ink the two share. A theme of the reader's is a name, the scheme it is
// drawn over, and the colors it sets in place of that scheme's; each scheme shows the theme chosen
// for it, or its own colors where none is. The themes and the choice are kept, and early.js sets them
// before the page's first frame from the same keys.

import { notifyScheme } from "./scheme.js";

export const THEMES = "orior.themes";
export const CHOSEN = "orior.theme.";
export const BASES = ["dark", "light"];

let palette = null;

// The scheme's own colors, by name, in the stylesheet's order: { dark: Map, light: Map }.
export function builtIn() {
  if (palette) {
    return palette;
  }
  palette = { dark: new Map(), light: new Map() };
  const shared = new Map();
  for (const sheet of document.styleSheets) {
    let rules;
    try {
      rules = sheet.cssRules;
    } catch {
      continue;
    }
    for (const rule of rules) {
      const into = rule.selectorText === ":root" ? shared : BASES.find((base) => rule.selectorText === `[data-scheme="${base}"]`);
      if (!into) {
        continue;
      }
      for (let at = 0; at < rule.style.length; at += 1) {
        const name = rule.style.item(at);
        const value = rule.style.getPropertyValue(name).trim();
        if (!name.startsWith("--") || !CSS.supports("color", value)) {
          continue;
        }
        (into === shared ? shared : palette[into]).set(name, value);
      }
    }
  }
  for (const base of BASES) {
    for (const [name, value] of shared) {
      palette[base].set(name, value);
    }
  }
  return palette;
}

export function themes() {
  try {
    const kept = JSON.parse(localStorage.getItem(THEMES) ?? "[]");
    return Array.isArray(kept) ? kept.filter((theme) => theme && typeof theme.name === "string" && BASES.includes(theme.base)) : [];
  } catch {
    return [];
  }
}

function keep(list) {
  localStorage.setItem(THEMES, JSON.stringify(list));
}

export function themeNamed(name) {
  return themes().find((theme) => theme.name === name) ?? null;
}

// The name of the theme chosen for a scheme, or null for the scheme's own colors.
export function chosen(base) {
  const name = localStorage.getItem(CHOSEN + base);
  return name && themeNamed(name)?.base === base ? name : null;
}

// The rule that sets each scheme's chosen theme over the stylesheet's colors.
export function themeRule(list, chosenOf) {
  let text = "";
  for (const base of BASES) {
    const theme = list.find((one) => one.name === chosenOf(base) && one.base === base);
    const colors = Object.entries(theme?.colors ?? {}).filter(([name, value]) => /^--[\w-]+$/.test(name) && !/[;{}]/.test(String(value)));
    if (colors.length) {
      text += `:root[data-scheme="${base}"] {\n${colors.map(([name, value]) => `  ${name}: ${value};`).join("\n")}\n}\n`;
    }
  }
  return text;
}

export function applyThemes() {
  let node = document.getElementById("theme-colors");
  if (!node) {
    node = Object.assign(document.createElement("style"), { id: "theme-colors" });
    document.head.append(node);
  }
  node.textContent = themeRule(themes(), chosen);
  notifyScheme();
}

export function choose(base, name) {
  if (name) {
    localStorage.setItem(CHOSEN + base, name);
  } else {
    localStorage.removeItem(CHOSEN + base);
  }
  applyThemes();
}

// A name no theme has yet, from `wanted`, with a number after it where it is taken.
export function freeName(wanted) {
  const taken = new Set([...themes().map((theme) => theme.name), "Dark", "Light"]);
  if (!taken.has(wanted)) {
    return wanted;
  }
  let number = 2;
  const stem = wanted.replace(/\s+\d+$/, "");
  while (taken.has(`${stem} ${number}`)) {
    number += 1;
  }
  return `${stem} ${number}`;
}

// Keeps a theme, in place of the one of the name `was` where it is given.
export function saveTheme(theme, was = theme.name) {
  const list = themes();
  const at = list.findIndex((one) => one.name === was);
  if (at >= 0) {
    list[at] = theme;
  } else {
    list.push(theme);
  }
  keep(list);
  if (was !== theme.name && localStorage.getItem(CHOSEN + theme.base) === was) {
    localStorage.setItem(CHOSEN + theme.base, theme.name);
  }
  applyThemes();
}

export function deleteTheme(name) {
  const theme = themeNamed(name);
  keep(themes().filter((one) => one.name !== name));
  if (theme && localStorage.getItem(CHOSEN + theme.base) === name) {
    localStorage.removeItem(CHOSEN + theme.base);
  }
  applyThemes();
}

// A theme as text to share, and one read back from such text. What it sets that is not a color of
// the palette is left out.
export function themeText(theme) {
  return JSON.stringify({ name: theme.name, base: theme.base, colors: theme.colors }, null, 2);
}

export function readTheme(text) {
  const read = JSON.parse(text);
  if (!read || !BASES.includes(read.base) || typeof read.colors !== "object") {
    throw new Error("A theme names its base, dark or light, and its colors.");
  }
  const known = builtIn()[read.base];
  const colors = Object.fromEntries(Object.entries(read.colors).filter(([name, value]) => known.has(name) && typeof value === "string" && CSS.supports("color", value)));
  return { name: freeName(String(read.name || read.base).slice(0, 60)), base: read.base, colors };
}
