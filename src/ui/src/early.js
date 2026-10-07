// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The kept scheme, the colors of the theme chosen for each scheme and the reader's fonts, set before
// the page draws anything, so that its first frame is already in them. themes.js and fonts.js keep
// them under the same keys, and fonts.js writes the same rule.
document.documentElement.dataset.scheme = localStorage.getItem("orior.scheme") === "light" ? "light" : "dark";
try {
  const list = JSON.parse(localStorage.getItem("orior.themes") ?? "[]");
  let text = "";
  for (const base of ["dark", "light"]) {
    const name = localStorage.getItem(`orior.theme.${base}`);
    const theme = Array.isArray(list) ? list.find((one) => one && one.name === name && one.base === base) : null;
    const colors = Object.entries(theme?.colors ?? {}).filter(([key, value]) => /^--[\w-]+$/.test(key) && !/[;{}]/.test(String(value)));
    if (colors.length) {
      text += `:root[data-scheme="${base}"] {${colors.map(([key, value]) => `${key}: ${value};`).join(" ")}}\n`;
    }
  }
  const node = document.createElement("style");
  node.id = "theme-colors";
  node.textContent = text;
  document.head.append(node);
} catch {
  // Kept themes that do not read leave the scheme's own colors.
}
try {
  const set = JSON.parse(localStorage.getItem("orior.fonts") ?? "{}");
  const lines = [];
  for (const name of ["--head", "--text", "--code"]) {
    const value = String(set?.[name] ?? "").trim();
    if (value && !/[;{}]/.test(value)) {
      lines.push(`${name}: ${value};`);
    }
  }
  const px = Number.parseFloat(set?.["--code-size"] ?? "");
  if (px >= 9 && px <= 32) {
    lines.push(`--code-size: ${px}px;`, `--code-line: ${Math.round((px * 20) / 13)}px;`);
  }
  const node = document.createElement("style");
  node.id = "fonts";
  node.textContent = lines.length ? `:root[data-scheme] { ${lines.join(" ")} }` : "";
  document.head.append(node);
} catch {
  // Kept fonts that do not read leave the stylesheet's own.
}
