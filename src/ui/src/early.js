// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Every entry the page keeps under a key starting `orior.` is kept as well in orior's own folder, laid
// out as the README's "orior's own folder" says, KEEP_REST after it last changed and as the page goes.
// The folder's entries are in the page's storage before this script runs; where the folder keeps
// nothing yet, every entry is written there now.
{
  const KEEP_REST = 400;
  const changed = new Map();
  let wait = 0;
  const flush = () => {
    window.clearTimeout(wait);
    if (changed.size) {
      const changes = Object.fromEntries(changed);
      changed.clear();
      window.__TAURI_INTERNALS__?.invoke("kept_write", { changes }).catch(() => {});
    }
  };
  const note = (key, text) => {
    if (key.startsWith("orior.")) {
      changed.set(key, text);
      window.clearTimeout(wait);
      wait = window.setTimeout(flush, KEEP_REST);
    }
  };
  const set = Storage.prototype.setItem;
  const remove = Storage.prototype.removeItem;
  Storage.prototype.setItem = function (key, text) {
    set.call(this, key, text);
    if (this === window.localStorage) {
      note(String(key), String(text));
    }
  };
  Storage.prototype.removeItem = function (key) {
    remove.call(this, key);
    if (this === window.localStorage) {
      note(String(key), null);
    }
  };
  window.addEventListener("pagehide", flush);
  if (window.oriorKeepAll) {
    for (const key of Object.keys(localStorage).filter((one) => one.startsWith("orior."))) {
      changed.set(key, localStorage.getItem(key));
    }
    flush();
  }
}

// The kept scheme, the colors of the theme chosen for each scheme, the reader's fonts and the bar's
// and the menus' opacity, set before the page draws anything, so that its first frame is already in
// them. themes.js, fonts.js and opacity.js keep them under the same keys, and fonts.js writes the
// same rule.
const kept = localStorage.getItem("orior.scheme");
const followed = kept === "system" ? (window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light") : kept;
document.documentElement.dataset.scheme = followed === "light" ? "light" : "dark";
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
for (const [name, key] of [
  ["--bar-opacity", "orior.opacity.bar"],
  ["--menu-opacity", "orior.opacity.menu"],
]) {
  const percent = Number(localStorage.getItem(key) ?? "");
  if (localStorage.getItem(key) !== null && percent >= 0 && percent <= 100) {
    document.documentElement.style.setProperty(name, `${percent}%`);
  }
}
