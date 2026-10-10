// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Preferences: the color theme of each scheme and every color of its palette, the zoom, the
// bar's and the menus' opacity, each item of the menus that is set on or off, the settings no menu
// lists, the patterns that keep the explorer, the search and the watching of files from files, and
// the sets of files Find in Files searches in. A color changed in the scheme's own theme starts a
// theme of the reader's from it, and every change shows at once. A theme copies out as text and
// reads back in from the clipboard.

import { FONT_SETTINGS, fontDefault, fonts, setFont } from "./fonts.js";
import { clipText, copyText } from "./menu.js";
import { OPACITY_SETTINGS, opacity, setOpacity, STEPS as OPACITY_STEPS } from "./opacity.js";
import { PATTERN_PARTS, patternsOf, setPatterns, setSets, setsText } from "./patterns.js";
import { scheme, setScheme } from "./scheme.js";
import { BASES, builtIn, choose, chosen, deleteTheme, freeName, readTheme, saveTheme, themeNamed, themes, themeText } from "./themes.js";
import { setZoom, STEPS, zoom } from "./zoom.js";

const BASE_NAMES = { dark: "Dark", light: "Light" };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// A color as the swatch takes it, #rrggbb, and the alpha the swatch cannot show.
const probe = document.createElement("canvas").getContext("2d");
function swatchOf(value) {
  probe.fillStyle = "#000000";
  probe.fillStyle = value;
  const read = probe.fillStyle;
  if (read.startsWith("#")) {
    return { hex: read, alpha: 1 };
  }
  const parts = read.match(/[\d.]+/g)?.map(Number) ?? [0, 0, 0, 1];
  const hex = `#${parts
    .slice(0, 3)
    .map((part) => Math.round(part).toString(16).padStart(2, "0"))
    .join("")}`;
  return { hex, alpha: parts[3] ?? 1 };
}

function withAlpha(hex, alpha) {
  if (alpha >= 1) {
    return hex;
  }
  const [r, g, b] = [1, 3, 5].map((at) => parseInt(hex.slice(at, at + 2), 16));
  return `rgba(${r}, ${g}, ${b}, ${Number(alpha.toFixed(3))})`;
}

// The groups the palette's names fall in, by what they start with.
function groupOf(name) {
  if (/^--t\d+$/.test(name)) {
    return "--t0 … --t15";
  }
  if (name.startsWith("--t-")) {
    return "--t-*";
  }
  if (name.startsWith("--ed-")) {
    return "--ed-*";
  }
  return "--*";
}

export function showPreferences(sheet, { menus, runCommand, checks, more = [] }) {
  const body = element("div", { className: "sheet-prefs" });
  const state = { filter: "" };

  // The theme shown: the scheme's chosen theme of the reader's, or null for the scheme's own.
  const shownTheme = () => {
    const name = chosen(scheme());
    return name ? themeNamed(name) : null;
  };

  // A theme of the reader's to change: the one shown, or a new one from the scheme's own colors.
  const ownTheme = () => {
    const theme = shownTheme();
    if (theme) {
      return theme;
    }
    const made = { name: freeName(`${BASE_NAMES[scheme()]} 2`), base: scheme(), colors: {} };
    saveTheme(made);
    choose(made.base, made.name);
    return made;
  };

  const said = element("p", { className: "prefs-said", ariaLive: "polite" });

  function themeRow() {
    const select = element("select", { className: "report-field", ariaLabel: "Color Theme" });
    for (const base of BASES) {
      select.append(element("option", { value: `base:${base}`, textContent: BASE_NAMES[base] }));
    }
    for (const theme of themes()) {
      select.append(element("option", { value: `theme:${theme.name}`, textContent: `${theme.name} (${BASE_NAMES[theme.base]})` }));
    }
    const theme = shownTheme();
    select.value = theme ? `theme:${theme.name}` : `base:${scheme()}`;
    select.addEventListener("change", () => {
      const [kind, name] = [select.value.slice(0, select.value.indexOf(":")), select.value.slice(select.value.indexOf(":") + 1)];
      if (kind === "base") {
        setScheme(name);
        choose(name, null);
      } else {
        const picked = themeNamed(name);
        setScheme(picked.base);
        choose(picked.base, picked.name);
      }
      draw();
    });
    const name = element("input", { className: "report-field prefs-name", type: "text", value: theme?.name ?? "", hidden: !theme, ariaLabel: "Name", spellcheck: false });
    name.addEventListener("change", () => {
      const was = shownTheme();
      const wanted = name.value.trim();
      if (!was || !wanted || wanted === was.name) {
        name.value = was?.name ?? "";
        return;
      }
      saveTheme({ ...was, name: freeName(wanted) }, was.name);
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
        const from = shownTheme();
        const made = { name: freeName(`${from?.name ?? BASE_NAMES[scheme()]} 2`), base: scheme(), colors: { ...(from?.colors ?? {}) } };
        saveTheme(made);
        choose(made.base, made.name);
        draw();
      }),
      button(
        "Delete",
        () => {
          deleteTheme(shownTheme().name);
          draw();
        },
        !theme
      ),
      button("Export", () => {
        copyText(themeText(shownTheme() ?? { name: BASE_NAMES[scheme()], base: scheme(), colors: {} }));
        said.textContent = "Copied to the clipboard.";
      }),
      button("Import", async () => {
        try {
          const read = readTheme(await clipText());
          saveTheme(read);
          setScheme(read.base);
          choose(read.base, read.name);
          draw();
          said.textContent = `Imported ${read.name}.`;
        } catch (error) {
          said.textContent = error instanceof SyntaxError ? "The clipboard holds no theme." : String(error.message ?? error);
        }
      })
    );
    return element("section", { className: "prefs-theme" }, element("h3", { textContent: "Color Theme" }), element("div", { className: "prefs-line" }, select, name), actions, said);
  }

  function colorRow(name, base, theme) {
    const value = theme?.colors[name] ?? base.get(name);
    const changed = Boolean(theme && name in theme.colors);
    const { hex, alpha } = swatchOf(value);
    const swatch = element("input", { type: "color", value: hex, className: "prefs-swatch", ariaLabel: name });
    const text = element("input", { type: "text", value, className: "prefs-value", spellcheck: false, ariaLabel: name });
    const row = element("div", { className: `prefs-color${changed ? " changed" : ""}` });
    row.dataset.name = name;
    const reset = element("button", { type: "button", className: "prefs-reset", textContent: "↺", title: base.get(name), ariaLabel: `${name} ${base.get(name)}`, disabled: !changed });
    const set = (next, redraw) => {
      const own = ownTheme();
      const colors = { ...own.colors };
      if (next === null || next === base.get(name)) {
        delete colors[name];
      } else {
        colors[name] = next;
      }
      saveTheme({ ...own, colors });
      if (redraw) {
        draw();
        return;
      }
      reset.disabled = !(name in colors);
      row.classList.toggle("changed", name in colors);
      // The first change to the scheme's own colors starts a theme, which the theme's row now names.
      if (!theme) {
        body.querySelector(".prefs-theme")?.replaceWith(themeRow());
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
    row.append(swatch, element("code", { textContent: name }), text, reset);
    return row;
  }

  function colorsSection() {
    const base = builtIn()[scheme()];
    const theme = shownTheme();
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
        groups.get(group).push(colorRow(name, base, theme));
      }
      grid.replaceChildren(...[...groups].flatMap(([group, rows]) => [element("h4", { textContent: group }), ...rows]));
    };
    filter.addEventListener("input", () => {
      state.filter = filter.value;
      fill();
    });
    fill();
    return element("section", { className: "prefs-palette" }, element("h3", { textContent: "Colors" }), filter, grid);
  }

  function settingsSection() {
    const rows = [];
    const zoomSelect = element("select", { className: "report-field", ariaLabel: "Zoom" }, ...STEPS.map((step) => element("option", { value: String(step), textContent: `${Math.round(step * 100)}%`, selected: step === zoom() })));
    zoomSelect.addEventListener("change", () => setZoom(Number(zoomSelect.value)));
    rows.push(element("label", { className: "report-row" }, element("span", { textContent: "Zoom" }), zoomSelect));
    for (const setting of OPACITY_SETTINGS) {
      const select = element("select", { className: "report-field", ariaLabel: setting.label }, ...OPACITY_STEPS.map((step) => element("option", { value: String(step), textContent: `${step}%`, selected: step === opacity(setting) })));
      select.addEventListener("change", () => setOpacity(setting, Number(select.value)));
      rows.push(element("label", { className: "report-row" }, element("span", { textContent: setting.label }), select));
    }
    for (const menu of menus) {
      for (const item of menu.all ?? []) {
        if (!item.checks || item.args !== "[on|off]") {
          continue;
        }
        const box = element("input", { type: "checkbox", checked: Boolean(checks[item.checks]?.()) });
        box.addEventListener("change", async () => {
          await runCommand(item.command, [box.checked ? "on" : "off"]);
          box.checked = Boolean(checks[item.checks]?.());
        });
        rows.push(element("label", { className: "report-check" }, box, element("span", { textContent: item.label })));
      }
    }
    for (const setting of more) {
      const box = element("input", { type: "checkbox", checked: setting.on() });
      box.addEventListener("change", () => {
        setting.set(box.checked);
        box.checked = setting.on();
      });
      rows.push(element("label", { className: "report-check" }, box, element("span", { textContent: setting.label })));
    }
    return element("section", { className: "prefs-settings" }, ...rows);
  }

  // The fonts, each a field the stylesheet's own font shows in while it is empty.
  function fontsSection() {
    const set = fonts();
    const rows = FONT_SETTINGS.map(({ name, label, size }) => {
      const field = element("input", {
        className: "report-field prefs-font",
        type: size ? "number" : "text",
        value: size ? String(Number.parseFloat(set[name] ?? "") || "") : (set[name] ?? ""),
        placeholder: size ? String(Number.parseFloat(fontDefault(name))) : fontDefault(name),
        spellcheck: false,
        ariaLabel: label,
      });
      if (size) {
        Object.assign(field, { min: 9, max: 32, step: 1 });
      }
      field.addEventListener("change", () => setFont(name, size && field.value ? `${field.value}px` : field.value));
      return element("label", { className: "report-row" }, element("span", { textContent: label }), field);
    });
    return element("section", { className: "prefs-fonts" }, element("h3", { textContent: "Fonts" }), ...rows);
  }

  // The patterns that keep the explorer, the search and the watching of files from files, one to a
  // line, and the sets of files Find in Files searches in, each list taken when its field is left.
  function patternsSection() {
    const rows = PATTERN_PARTS.map(({ part, label }) => {
      const field = element("textarea", { className: "report-field", rows: 4, value: patternsOf(part), spellcheck: false, ariaLabel: label });
      field.addEventListener("change", () => setPatterns(part, field.value));
      return element("label", { className: "report-row" }, element("span", { textContent: label }), field);
    });
    const sets = element("textarea", { className: "report-field", rows: 4, value: setsText(), placeholder: "Engine:\nsrc/cu/**\n!src/cu/test/**", spellcheck: false, ariaLabel: "Sets of files to search" });
    sets.addEventListener("change", () => setSets(sets.value));
    rows.push(element("label", { className: "report-row" }, element("span", { textContent: "Sets of files to search" }), sets));
    return element("section", { className: "prefs-patterns" }, element("h3", { textContent: "Files" }), ...rows);
  }

  function draw() {
    const focused = document.activeElement;
    const focusName = body.contains(focused) ? focused.closest(".prefs-color")?.dataset.name : null;
    const scroll = body.querySelector(".prefs-colors")?.scrollTop ?? 0;
    body.replaceChildren(element("h2", { textContent: "Preferences" }), settingsSection(), fontsSection(), patternsSection(), themeRow(), colorsSection());
    const grid = body.querySelector(".prefs-colors");
    grid.scrollTop = scroll;
    if (focusName) {
      body.querySelector(`.prefs-color[data-name="${CSS.escape(focusName)}"] .prefs-value`)?.focus();
    }
  }

  draw();
  sheet(body);
}
