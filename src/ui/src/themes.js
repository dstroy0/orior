// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Color themes. The palette is every color the stylesheet gives the dark and the light scheme, read
// from its own rules, and the ink the two share, with a color for every kind of token a grammar
// names. A theme of the reader's is a name, the scheme it is drawn over, the colors it sets in place
// of that scheme's, an image it lays behind the editor, and the icons it gives files by patterns of
// their names; each scheme shows the theme chosen for it, or its own colors where none is. The themes
// and the choice are kept, and early.js sets the colors before the page's first frame from the same
// keys.

import { tokenKinds } from "./editor/tokens.js";
import { notifyScheme, scheme } from "./scheme.js";
import { kindSetting } from "./settings.js";

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

// A kind of token as a color of the palette: --t-keyword for keyword, --t-number-cost for number.cost.
const tokenColor = (kind) => `--t-${kind.split(".").join("-")}`;

// The palette of the scheme `base` with every kind of token the grammars name: a kind the stylesheet
// gives no color of its own takes the color of the kind it is part of, or the editor's text's.
export function paletteOf(base) {
  const own = builtIn()[base];
  const palette = new Map(own);
  for (const kind of tokenKinds()) {
    const parts = kind.split(".");
    for (let length = 1; length <= parts.length; length += 1) {
      const name = tokenColor(parts.slice(0, length).join("."));
      if (/^--t-[\w-]+$/.test(name) && !palette.has(name)) {
        palette.set(name, palette.get(tokenColor(parts.slice(0, length - 1).join("."))) ?? own.get("--ed-fg"));
      }
    }
  }
  return palette;
}

// The rule that colors a kind of token the stylesheet gives no color of its own, `--t-a-b`, through
// the kinds it is part of to the text's own color where no theme sets one. Its class is named twice
// to stand over the stylesheet's rule for the kind it is part of, wherever the two stand in the page.
function tokenRule(name) {
  const parts = name.slice("--t-".length).split("-");
  let value = "currentColor";
  for (let length = 1; length <= parts.length; length += 1) {
    value = `var(--t-${parts.slice(0, length).join("-")}, ${value})`;
  }
  const kind = `.t-${parts.join("-")}`;
  return `${kind}${kind} { color: ${value}; }\n`;
}

// How much of a theme's image shows where the theme names no share.
export const IMAGE_OPACITY = 0.15;

// The image a theme lays behind the editor, as the custom properties that draw it: a picture kept in
// the theme as a data address, and the share of it that shows, from 0 to 1. None where the theme has
// no image.
export function imageProperties(theme) {
  const src = theme?.image?.src;
  if (typeof src !== "string" || !/^data:image\/[\w.+-]+;base64,[A-Za-z0-9+/=]+$/.test(src)) {
    return [];
  }
  const share = Number(theme.image.opacity ?? IMAGE_OPACITY);
  return [
    ["--ed-image", `url("${src}")`],
    ["--ed-image-opacity", String(Number.isFinite(share) ? Math.min(1, Math.max(0, share)) : IMAGE_OPACITY)],
  ];
}

// The icon the theme shown gives a file's name: the first of its icons whose pattern matches the
// name, `*` standing for any letters and `?` for one, with its mark, a letter or a few, and its
// color, one of the palette's by name or any color. Null where none matches.
export function themeIcon(name) {
  const theme = themeNamed(chosen(scheme()) ?? "");
  for (const icon of Array.isArray(theme?.icons) ? theme.icons : []) {
    if (icon?.pattern && typeof icon.pattern === "string" && typeof icon.mark === "string" && globOf(icon.pattern).test(name)) {
      return { mark: icon.mark.slice(0, 3), color: iconColor(icon.color) };
    }
  }
  return null;
}

// The icons of the theme shown, as text that changes as they do.
export function iconsKey() {
  return JSON.stringify(themeNamed(chosen(scheme()) ?? "")?.icons ?? []);
}

const globs = new Map();

function globOf(pattern) {
  let found = globs.get(pattern);
  if (!found) {
    const source = [...pattern].map((letter) => (letter === "*" ? "[^/]*" : letter === "?" ? "[^/]" : letter.replace(/[\\^$.|+()[\]{}]/g, "\\$&"))).join("");
    found = new RegExp(`^${source}$`, "i");
    globs.set(pattern, found);
  }
  return found;
}

export const iconColor = (color) => (typeof color !== "string" ? "" : /^--[\w-]+$/.test(color) ? `var(${color})` : CSS.supports("color", color) ? color : "");

// The least contrast a color of text has with the color under it, as WCAG 2's level AA asks of text.
export const CONTRAST_LEAST = 4.5;

// The pairs of a color and the color under it whose contrast Preferences checks: the editor's text,
// its line numbers and each kind of token over the editor's background, and the window's text and
// links over its own.
export function contrastPairs(palette) {
  const pairs = [
    ["--ed-fg", "--ed-bg"],
    ["--ed-num", "--ed-bg"],
    ["--fg", "--bg"],
    ["--fg-2", "--bg"],
    ["--fg", "--surface"],
    ["--link", "--bg"],
  ];
  for (const name of palette.keys()) {
    if (name.startsWith("--t-")) {
      pairs.push([name, "--ed-bg"]);
    }
  }
  return pairs.filter(([fore, back]) => palette.has(fore) && palette.has(back));
}

const probe = document.createElement("canvas").getContext("2d");

// A color as red, green, blue and alpha, each of the first three 0 to 255 and alpha 0 to 1.
function rgbaOf(color) {
  probe.fillStyle = "#000000";
  probe.fillStyle = color;
  const read = probe.fillStyle;
  if (read.startsWith("#")) {
    return [1, 3, 5].map((at) => parseInt(read.slice(at, at + 2), 16)).concat(1);
  }
  const parts = read.match(/[\d.]+/g)?.map(Number) ?? [0, 0, 0, 1];
  return [parts[0], parts[1], parts[2], parts[3] ?? 1];
}

const over = ([r, g, b, a], [r2, g2, b2]) => [r * a + r2 * (1 - a), g * a + g2 * (1 - a), b * a + b2 * (1 - a), 1];

const luminance = (rgb) =>
  rgb
    .slice(0, 3)
    .map((channel) => channel / 255)
    .map((channel) => (channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4))
    .reduce((sum, channel, at) => sum + channel * [0.2126, 0.7152, 0.0722][at], 0);

// The contrast of the color `fore` over `back`, from 1 to 21, as WCAG 2 measures it; a color with
// alpha is laid over the color under it first, and `back` over `ground` where it has alpha.
export function contrastOf(fore, back, ground = "#ffffff") {
  const under = over(rgbaOf(back), rgbaOf(ground));
  const [one, two] = [luminance(over(rgbaOf(fore), under)), luminance(under)].sort((a, b) => b - a);
  return (one + 0.05) / (two + 0.05);
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

// The rule that sets each scheme's chosen theme over the stylesheet's colors, with its image, and
// colors each kind of token any theme sets that the stylesheet gives no color of its own.
export function themeRule(list, chosenOf) {
  let text = "";
  for (const base of BASES) {
    const theme = list.find((one) => one.name === chosenOf(base) && one.base === base);
    const colors = Object.entries(theme?.colors ?? {}).filter(([name, value]) => /^--[\w-]+$/.test(name) && !/[;{}]/.test(String(value))).concat(imageProperties(theme));
    if (colors.length) {
      text += `:root[data-scheme="${base}"] {\n${colors.map(([name, value]) => `  ${name}: ${value};`).join("\n")}\n}\n`;
    }
  }
  const own = builtIn();
  const kinds = new Set(list.flatMap((theme) => Object.keys(theme.colors ?? {})).filter((name) => /^--t-[\w-]+$/.test(name) && !own.dark.has(name) && !own.light.has(name)));
  return text + [...kinds].map(tokenRule).join("");
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

// The colors the editor shows a file of the language `kind` in, where a settings file gives that kind
// a theme for the scheme shown (settings.js): every color of the theme's own scheme with the theme's
// over them, Dark and Light being the schemes' own. Null where none is given or the theme named is
// not one of the reader's.
export function kindColors(kind) {
  const name = kind ? kindSetting(kind, CHOSEN.slice("orior.".length) + scheme()) : null;
  const own = Object.entries(BASE_OWN).find(([, named]) => named === name)?.[0];
  const theme = own ? { base: own, colors: {} } : name ? themeNamed(name) : null;
  if (!theme) {
    return null;
  }
  const image = imageProperties(theme);
  const colors = Object.fromEntries(image.length ? image : [["--ed-image", "none"]]);
  Object.assign(colors, Object.fromEntries(builtIn()[theme.base]));
  for (const [color, value] of Object.entries(theme.colors ?? {})) {
    if (/^--[\w-]+$/.test(color) && !/[;{}]/.test(String(value))) {
      colors[color] = value;
    }
  }
  return colors;
}

// The names of the schemes' own colors, as a setting names them.
const BASE_OWN = { dark: "Dark", light: "Light" };

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
// the palette, an image or an icon is left out.
export function themeText(theme) {
  return JSON.stringify({ name: theme.name, base: theme.base, colors: theme.colors, ...(theme.image ? { image: theme.image } : {}), ...(theme.icons?.length ? { icons: theme.icons } : {}) }, null, 2);
}

export function readTheme(text) {
  const read = JSON.parse(text);
  if (!read || !BASES.includes(read.base) || typeof read.colors !== "object") {
    throw new Error("A theme names its base, dark or light, and its colors.");
  }
  const known = paletteOf(read.base);
  const colors = Object.fromEntries(Object.entries(read.colors).filter(([name, value]) => (known.has(name) || /^--t-[\w-]+$/.test(name)) && typeof value === "string" && CSS.supports("color", value)));
  const made = { name: freeName(String(read.name || read.base).slice(0, 60)), base: read.base, colors };
  if (imageProperties(read).length) {
    made.image = { src: read.image.src, opacity: Number(imageProperties(read)[1][1]) };
  }
  const icons = (Array.isArray(read.icons) ? read.icons : []).filter((icon) => typeof icon?.pattern === "string" && typeof icon.mark === "string").map(({ pattern, mark, color }) => ({ pattern, mark: mark.slice(0, 3), color: typeof color === "string" ? color : "" }));
  if (icons.length) {
    made.icons = icons;
  }
  return made;
}
