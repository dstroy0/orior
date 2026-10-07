// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The kept scheme and the colors of the theme chosen for each scheme, set before the page draws
// anything, so that its first frame is already in them. themes.js keeps both under the same keys.
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
