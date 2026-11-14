// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { themeIcon } from "../themes.js";
import { icon } from "../icons.js";
import { element } from "../explorer.js";

// A file's icon: the one the theme gives its name, else a mark for its type or its name in a color
// of the palette, else the first two characters of its type, or of its name where it has none, quieter
// than a mark: no file is drawn without one. A folder's icon is the folder's shape.
const ICONS = {
  js: ["JS", 3], mjs: ["JS", 3], cjs: ["JS", 3], jsx: ["JS", 3], ts: ["TS", 4], tsx: ["TS", 4], mts: ["TS", 4], cts: ["TS", 4],
  json: ["{}", 3], jsonc: ["{}", 3], cfg: ["{}", 3], rs: ["rs", 1], py: ["py", 4], pyi: ["py", 5], ipynb: ["nb", 3],
  sh: ["$", 2], bash: ["$", 2], zsh: ["$", 2], fish: ["$", 2], ps1: ["PS", 4], psm1: ["PS", 4], bat: ["$", 7], cmd: ["$", 7],
  c: ["C", 4], h: ["h", 5], cu: ["cu", 2], cuh: ["cu", 5], cpp: ["C", 4], cc: ["C", 4], cxx: ["C", 4], hpp: ["h", 5], hh: ["h", 5],
  cs: ["C#", 6], fs: ["F#", 4], go: ["go", 4], java: ["jv", 1], kt: ["kt", 6], kts: ["kt", 6], scala: ["sc", 1], swift: ["sw", 1],
  rb: ["rb", 1], php: ["ph", 6], lua: ["lu", 4], r: ["R", 4], jl: ["jl", 6], hs: ["hs", 6], ml: ["ml", 3], ex: ["ex", 6], exs: ["ex", 6],
  erl: ["er", 1], dart: ["dt", 4], zig: ["zg", 3], nim: ["nm", 3], lean: ["L", 4], m: ["m", 3], mojo: ["mo", 1], v: ["v", 2], sv: ["sv", 2],
  vhd: ["vh", 2], vhdl: ["vh", 2], asm: ["as", 8], s: ["as", 8], sql: ["sq", 5], qry: ["q", 2], graphql: ["gq", 6], proto: ["pb", 4],
  md: ["M", 4], mdx: ["M", 4], rst: ["rs", 7], txt: ["tx", 8], log: ["lg", 8], html: ["<>", 1], htm: ["<>", 1], svg: ["<>", 3],
  xml: ["<>", 1], vue: ["<>", 2], svelte: ["<>", 1], css: ["#", 4], scss: ["#", 6], less: ["#", 4],
  tex: ["T", 2], sty: ["T", 2], cls: ["T", 2], bib: ["T", 6], toml: ["≡", 7], ini: ["≡", 7], env: ["≡", 7],
  yml: ["≡", 5], yaml: ["≡", 5], tsv: ["⊞", 2], csv: ["⊞", 2], parquet: ["⊞", 5], lock: ["≡", 8],
  png: ["▣", 5], jpg: ["▣", 5], jpeg: ["▣", 5], gif: ["▣", 5], webp: ["▣", 5], bmp: ["▣", 5], ico: ["▣", 5], pdf: ["▤", 1],
  wav: ["au", 6], mp3: ["au", 6], flac: ["au", 6], ogg: ["au", 6], mp4: ["av", 6], mkv: ["av", 6], webm: ["av", 6],
  zip: ["zp", 7], tar: ["zp", 7], gz: ["zp", 7], tgz: ["zp", 7], xz: ["zp", 7], bz2: ["zp", 7], "7z": ["zp", 7], rar: ["zp", 7],
  bin: ["01", 8], exe: ["01", 8], dll: ["01", 8], so: ["01", 8], dylib: ["01", 8], o: ["01", 8], a: ["01", 8], obj: ["01", 8], wasm: ["01", 3],
};

const NAMED = {
  ".gitignore": ["◆", 1], ".gitattributes": ["◆", 1], ".gitmodules": ["◆", 1], ".clang-format": ["≡", 7], ".editorconfig": ["≡", 7],
  makefile: ["mk", 7], gnumakefile: ["mk", 7], "cmakelists.txt": ["cm", 2], dockerfile: ["dk", 4], containerfile: ["dk", 4],
  license: ["Li", 8], licence: ["Li", 8], copying: ["Li", 8], readme: ["M", 4], changelog: ["M", 4], justfile: ["mk", 7], procfile: ["≡", 7],
};

// The quieter mark a file of no type or name above is drawn with: the first two characters of its type,
// or of its name where it has none.
function markOf(name, ext) {
  const from = ext || name.replace(/^\.+/, "");
  return from.slice(0, 2).toLowerCase() || "··";
}

// How far up each mark is moved, in pixels, for its ink to center on the middle of the capitals of
// the name beside it: a mark of small characters, as py or zp, is with its ink lower than the name's,
// and the mark's line and the name's are not the same in every engine. Each is measured once the
// fonts are in, as a row sets the mark and the name.
const lifts = new Map();
let fonts = null;

// The ink of `text` in `font`, as { font, wide }: how far above its line it reaches, and below, in
// pixels, the font scaled to set a "0" `wide` pixels wide, as the page sets it. The text is drawn at
// 100px and its ink found row by row.
function inkOf(font, text) {
  const canvas = (inkOf.canvas ??= Object.assign(document.createElement("canvas"), { width: 400, height: 240 }));
  const pen = canvas.getContext("2d", { willReadFrequently: true });
  pen.font = font.font;
  pen.clearRect(0, 0, canvas.width, canvas.height);
  pen.fillStyle = "#000";
  pen.textBaseline = "alphabetic";
  pen.fillText(text, 10, 160);
  const { data } = pen.getImageData(0, 0, canvas.width, canvas.height);
  let top = -1;
  let bottom = -1;
  for (let row = 0; row < canvas.height; row += 1) {
    for (let at = row * canvas.width * 4 + 3; at < (row + 1) * canvas.width * 4; at += 4) {
      if (data[at] > 96) {
        top = top < 0 ? row : top;
        bottom = row + 1;
        break;
      }
    }
  }
  const scale = font.wide / pen.measureText("0").width;
  return top < 0 ? { above: 0, below: 0 } : { above: (160 - top) * scale, below: (bottom - 160) * scale };
}

export function liftOf(mark) {
  if (lifts.has(mark)) {
    return lifts.get(mark);
  }
  if (document.fonts?.status !== "loaded") {
    return 0;
  }
  if (!fonts) {
    // Each font's "0" measured as the row sets it, in a box of ten of them.
    const zeros = (className) => element("span", { className, textContent: "0000000000" });
    const icon = zeros("icon");
    const name = zeros("name");
    const row = element("div", { className: "node" }, icon, name);
    row.style.cssText = "position:fixed;visibility:hidden;width:auto";
    icon.style.width = "auto";
    name.style.flex = "none";
    document.body.append(row);
    // A box of no height set in a text is on the text's line.
    const lineOf = (node) => {
      const probe = element("span");
      probe.style.cssText = "display:inline-block;width:0;height:0;vertical-align:baseline";
      node.append(probe);
      const at = probe.getBoundingClientRect().bottom;
      probe.remove();
      return at;
    };
    const of = (node) => {
      const style = getComputedStyle(node);
      return { font: `${style.fontWeight} 100px ${style.fontFamily}`, wide: node.getBoundingClientRect().width / 10 };
    };
    fonts = { icon: of(icon), name: of(name), below: lineOf(icon) - lineOf(name) };
    row.remove();
  }
  const ink = inkOf(fonts.icon, mark);
  const lift = inkOf(fonts.name, "H").above / 2 - (ink.above - ink.below) / 2 + fonts.below;
  lifts.set(mark, lift);
  return lift;
}

export function iconOf(name, known = false) {
  const ext = name.includes(".") ? name.split(".").pop().toLowerCase() : "";
  const themed = known ? null : themeIcon(name);
  // A name looked up whole, and without its type where the type has no mark: Dockerfile.dev is a
  // Dockerfile, and license.py is Python.
  const named = NAMED[name] ?? NAMED[name.toLowerCase()] ?? (ICONS[ext] ? null : NAMED[name.toLowerCase().split(".")[0]]);
  const [mark, color] = themed ? [themed.mark, null] : known ? ["K", 2] : (named ?? ICONS[ext] ?? [markOf(name, ext), 8]);
  const icon = element("span", { className: "icon", textContent: mark });
  icon.setAttribute("aria-hidden", "true");
  const lift = liftOf(mark);
  if (Math.abs(lift) > 0.1) {
    icon.style.transform = `translateY(${(-lift).toFixed(2)}px)`;
  }
  if (!themed && !known && !named && !ICONS[ext]) {
    icon.classList.add("unmarked");
  }
  if (themed?.color) {
    icon.style.color = themed.color;
  } else if (mark && color !== null) {
    icon.style.color = `var(--t${color})`;
  }
  return icon;
}

// A folder's icon: the folder's shape.
export function folderIcon() {
  const shape = element("span", { className: "icon folder" }, icon("folder"));
  shape.setAttribute("aria-hidden", "true");
  return shape;
}
