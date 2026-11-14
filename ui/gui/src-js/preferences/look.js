// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sections of Preferences that show the theme: its name and what is done with it, the image behind the editor,
// the files' icons, and the palette with each color whose contrast is too low for text marked. Each section takes
// the sheet it is drawn in.

import { pick } from "../bridge.js";
import { scheme } from "../scheme.js";
import { BASES, contrastOf, contrastPairs, CONTRAST_LEAST, deleteTheme, iconColor, IMAGE_OPACITY, paletteOf, themes } from "../themes.js";
import { BASE_NAMES, chooseImage, chooseTheme, exportTheme, groupOf, IMAGE_SHARES, importTheme, retainIcons, newTheme, removeImage, renameTheme, setColor, shareImage, displayedTheme, swatchOf, withAlpha } from "./theme.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The image the theme positions behind the editor: chosen from a file, the share of it that shows, and
// taken off.
function imageRow(at) {
  const { said, draw } = at;
  const image = displayedTheme()?.image ?? null;
  const button = (text, run, disabled = false) => {
    const made = element("button", { type: "button", className: "prefs-button", textContent: text, disabled });
    made.addEventListener("click", run);
    return made;
  };
  const share = element(
    "select",
    { className: "report-field prefs-image-share", ariaLabel: "The image's share", disabled: !image },
    ...IMAGE_SHARES.map((step) => element("option", { value: String(step), textContent: `${Math.round(step * 100)}%`, selected: Math.abs(step - (image?.opacity ?? IMAGE_OPACITY)) < 0.001 })),
  );
  share.addEventListener("change", () => shareImage(Number(share.value)));
  const thumb = element("span", { className: "prefs-image-thumb", hidden: !image });
  if (image) {
    thumb.style.backgroundImage = `url("${image.src}")`;
  }
  return element(
    "div",
    { className: "prefs-line prefs-image" },
    element("span", { className: "prefs-image-label", textContent: "Image behind the editor" }),
    thumb,
    button("Choose…", async () => {
      const path = await pick("file").catch(() => null);
      if (typeof path !== "string") {
        return;
      }
      try {
        await chooseImage(path);
        draw();
      } catch (error) {
        said.textContent = String(error);
      }
    }),
    share,
    button(
      "Remove",
      () => {
        removeImage();
        draw();
      },
      !image,
    ),
  );
}

// The icons the theme gives files by patterns of their names, the first whose pattern a name
// matches giving its icon: each a pattern, a mark and a color, changed in place and taken off by
// its cross, and one more added by Add.
function iconsSection(at) {
  const { body, draw } = at;
  const icons = displayedTheme()?.icons ?? [];
  const retain = retainIcons;
  const rows = icons.map((icon, at) => {
    const mark = element("span", { className: "icon prefs-icon-displayed", ariaHidden: "true" });
    const drawMark = () => {
      const now = displayedTheme()?.icons?.[at];
      mark.textContent = now?.mark ?? "";
      mark.style.color = iconColor(now?.color) || "";
    };
    const field = (key, label) => {
      const input = element("input", { className: `report-field prefs-icon-${key}`, type: "text", value: icon[key] ?? "", placeholder: label, ariaLabel: label, spellcheck: false });
      input.addEventListener("change", () => {
        retain((displayedTheme()?.icons ?? []).map((one, index) => (index === at ? { ...one, [key]: input.value.trim() } : one)));
        drawMark();
      });
      return input;
    };
    const off = element("button", { type: "button", className: "prefs-key-off", textContent: "×", title: "Take this icon off" });
    off.addEventListener("click", () => {
      retain((displayedTheme()?.icons ?? []).filter((_, index) => index !== at));
      draw();
    });
    drawMark();
    return element("div", { className: "prefs-icon-row" }, mark, field("pattern", "Pattern"), field("mark", "Mark"), field("color", "Color"), off);
  });
  const add = element("button", { type: "button", className: "prefs-button", textContent: "Add" });
  add.addEventListener("click", () => {
    retain([...(displayedTheme()?.icons ?? []), { pattern: "", mark: "", color: "" }]);
    draw();
    [...body.querySelectorAll(".prefs-icon-pattern")].at(-1)?.focus();
  });
  return element(
    "div",
    { className: "prefs-icons" },
    element("h4", { textContent: "File icons" }),
    element("p", { className: "prefs-icons-said", textContent: "A file takes the first icon whose pattern its name matches, * located for any characters and ? for one; a color is one of the palette's, as --t2, or any color." }),
    ...rows,
    add,
  );
}

export function themeRow(at) {
  const { said, draw } = at;
  const select = element("select", { className: "report-field", ariaLabel: "Color Theme" });
  for (const base of BASES) {
    select.append(element("option", { value: `base:${base}`, textContent: BASE_NAMES[base] }));
  }
  for (const theme of themes()) {
    select.append(element("option", { value: `theme:${theme.name}`, textContent: `${theme.name} (${BASE_NAMES[theme.base]})` }));
  }
  const theme = displayedTheme();
  select.value = theme ? `theme:${theme.name}` : `base:${scheme()}`;
  select.addEventListener("change", () => {
    chooseTheme(select.value.slice(select.value.indexOf(":") + 1));
    draw();
  });
  const name = element("input", { className: "report-field prefs-name", type: "text", value: theme?.name ?? "", hidden: !theme, ariaLabel: "Name", spellcheck: false });
  name.addEventListener("change", () => {
    const was = displayedTheme();
    const wanted = name.value.trim();
    if (!was || !wanted || wanted === was.name) {
      name.value = was?.name ?? "";
      return;
    }
    renameTheme(wanted);
    draw();
  });
  const button = (text, run, disabled = false) => {
    const made = element("button", { type: "button", className: "prefs-button", textContent: text, disabled });
    made.addEventListener("click", run);
    return made;
  };
  const actions = element(
    "div",
    { className: "prefs-actions" },
    button("New", () => {
      newTheme();
      draw();
    }),
    button(
      "Delete",
      () => {
        deleteTheme(displayedTheme().name);
        draw();
      },
      !theme
    ),
    button("Export", () => {
      said.textContent = exportTheme();
    }),
    button("Import", async () => {
      const { text, failed: wrong } = await importTheme();
      if (!wrong) {
        draw();
      }
      said.textContent = text;
    })
  );
  return element("section", { className: "prefs-theme" }, element("h3", { textContent: "Color Theme" }), element("div", { className: "prefs-line" }, select, name), actions, said, imageRow(at), iconsSection(at));
}

function colorRow(at, name, base, theme) {
  const { body, said, draw } = at;
  const value = theme?.colors[name] ?? base.get(name);
  const changed = Boolean(theme && name in theme.colors);
  const { hex, alpha } = swatchOf(value);
  const swatch = element("input", { type: "color", value: hex, className: "prefs-swatch", ariaLabel: name });
  const text = element("input", { type: "text", value, className: "prefs-value", spellcheck: false, ariaLabel: name });
  const row = element("div", { className: `prefs-color${changed ? " changed" : ""}` });
  row.dataset.name = name;
  const reset = element("button", { type: "button", className: "prefs-reset", textContent: "↺", title: base.get(name), ariaLabel: `${name} ${base.get(name)}`, disabled: !changed });
  const set = (next, redraw) => {
    const colors = setColor(name, next);
    if (redraw) {
      draw();
      return;
    }
    at.drawContrast();
    reset.disabled = !(name in colors);
    row.classList.toggle("changed", name in colors);
    // The first change to the scheme's own colors starts a theme, which the theme's row now names.
    if (!theme) {
      body.querySelector(".prefs-theme")?.replaceWith(themeRow(at));
    }
  };
  swatch.addEventListener("input", () => {
    text.value = withAlpha(swatch.value, swatchOf(text.value).alpha ?? alpha);
    set(text.value, false);
  });
  text.addEventListener("change", () => {
    const next = text.value.trim();
    if (!CSS.supports("color", next)) {
      text.value = theme?.colors[name] ?? base.get(name);
      said.textContent = `${next} is not a color.`;
      return;
    }
    swatch.value = swatchOf(next).hex;
    set(next, false);
  });
  reset.addEventListener("click", () => set(null, true));
  row.append(swatch, element("code", { textContent: name }), text, element("span", { className: "prefs-contrast" }), reset);
  return row;
}

export function colorsSection(at) {
  const { state } = at;
  const base = paletteOf(scheme());
  const theme = displayedTheme();
  const contrasts = element("p", { className: "prefs-contrasts", hidden: true });
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", value: state.filter, spellcheck: false, ariaLabel: "Search colors" });
  const grid = element("div", { className: "prefs-colors" });
  const fill = () => {
    const query = state.filter.trim().toLowerCase();
    const groups = new Map();
    for (const name of base.keys()) {
      const value = (theme?.colors[name] ?? base.get(name)).toLowerCase();
      if (query && !name.includes(query) && !value.includes(query)) {
        continue;
      }
      const group = groupOf(name);
      if (!groups.has(group)) {
        groups.set(group, []);
      }
      groups.get(group).push(colorRow(at, name, base, theme));
    }
    grid.replaceChildren(...[...groups].flatMap(([group, rows]) => [element("h4", { textContent: group }), ...rows]));
  };
  // Each color of text whose contrast with the color under it is under CONTRAST_LEAST, marked on its
  // row with the ratio and the color under it, and listed over the palette.
  at.drawContrast = () => {
    const colors = new Map(base);
    for (const [name, value] of Object.entries(displayedTheme()?.colors ?? {})) {
      colors.set(name, value);
    }
    const low = new Map();
    for (const [fore, back] of contrastPairs(colors)) {
      const ratio = contrastOf(colors.get(fore), colors.get(back), colors.get("--bg"));
      if (ratio < CONTRAST_LEAST && !low.has(fore)) {
        low.set(fore, { back, ratio });
      }
    }
    for (const row of grid.querySelectorAll(".prefs-color")) {
      const found = low.get(row.dataset.name);
      row.classList.toggle("low", Boolean(found));
      row.querySelector(".prefs-contrast").textContent = found ? `${found.ratio.toFixed(1)}:1 on ${found.back}` : "";
    }
    contrasts.hidden = !low.size;
    contrasts.textContent = `Under ${CONTRAST_LEAST}:1, the contrast WCAG 2 asks of text: ${[...low].map(([name, { back, ratio }]) => `${name} on ${back}, ${ratio.toFixed(1)}:1`).join("; ")}.`;
  };
  filter.addEventListener("input", () => {
    state.filter = filter.value;
    fill();
    at.drawContrast();
  });
  fill();
  at.drawContrast();
  return element("section", { className: "prefs-palette" }, element("h3", { textContent: "Colors" }), contrasts, filter, grid);
}
