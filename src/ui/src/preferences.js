// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Preferences: the color theme of each scheme and every color of its palette, the zoom, the
// bar's and the menus' opacity, each item of the menus that is set on or off, the settings no menu
// lists, each setting a settings file sets with its value and the file it comes from, the settings
// carried to and from another machine, the patterns
// that keep the explorer, the search and the watching of files from files, the sets of files Find in
// Files searches in, and the keys: the set of keys, how a letter's key is bound, each key more than
// one command holds, and each command's keys, set by pressing them. A color changed in the scheme's
// own theme starts a theme of the reader's from it, and every change shows at once; the palette holds
// a color for every kind of token a grammar names, and each color whose contrast with the color under
// it is too low for text is marked. A theme lays an image behind the editor and gives files icons by
// patterns of their names. A theme copies out as text and reads back in from the clipboard.

import { FONT_SETTINGS, fontDefault, fonts, setFont } from "./fonts.js";
import { buttonName, keySettings, pressName, setKeySettings, SETS, sharedKeys, wheelName } from "./keys.js";
import { clipText, copyText } from "./menu.js";
import { OPACITY_SETTINGS, opacity, setOpacity, STEPS as OPACITY_STEPS } from "./opacity.js";
import { PATTERN_PARTS, patternsOf, setPatterns, setSets, setsText } from "./patterns.js";
import { scheme, setScheme } from "./scheme.js";
import { BASES, choose, chosen, contrastOf, contrastPairs, CONTRAST_LEAST, deleteTheme, freeName, iconColor, IMAGE_OPACITY, paletteOf, readTheme, saveTheme, themeNamed, themes, themeText } from "./themes.js";
import { setZoom, STEPS, zoom } from "./zoom.js";
import { readSettings, settingsInForce, TREE_FILE } from "./settings.js";
import { invoke, pick } from "./bridge.js";
import { carryInto } from "./carry.js";

const BASE_NAMES = { dark: "Dark", light: "Light" };

// The shares of the image behind the editor to choose from.
const IMAGE_SHARES = [0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.75, 1];

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

export function showPreferences(sheet, { menus, runCommand, checks, more = [], openFile = null, reloadOwn = null }) {
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

  // Marks each color whose contrast with the one under it is too low, once a color changes; set by
  // the palette's section as it is drawn.
  let drawContrast = () => {};

  // The image the theme lays behind the editor: chosen from a file, the share of it that shows, and
  // taken off.
  function imageRow() {
    const image = shownTheme()?.image ?? null;
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
    share.addEventListener("change", () => {
      const own = ownTheme();
      saveTheme({ ...own, image: { ...own.image, opacity: Number(share.value) } });
    });
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
          const src = await invoke("image_data", { path });
          const own = ownTheme();
          saveTheme({ ...own, image: { src, opacity: own.image?.opacity ?? IMAGE_OPACITY } });
          draw();
        } catch (error) {
          said.textContent = String(error);
        }
      }),
      share,
      button(
        "Remove",
        () => {
          const own = { ...ownTheme() };
          delete own.image;
          saveTheme(own);
          draw();
        },
        !image,
      ),
    );
  }

  // The icons the theme gives files by patterns of their names, the first whose pattern a name
  // matches giving its icon: each a pattern, a mark and a color, changed in place and taken off by
  // its cross, and one more added by Add.
  function iconsSection() {
    const icons = shownTheme()?.icons ?? [];
    const keep = (next) => {
      const own = ownTheme();
      saveTheme({ ...own, icons: next });
    };
    const rows = icons.map((icon, at) => {
      const mark = element("span", { className: "icon prefs-icon-shown", ariaHidden: "true" });
      const drawMark = () => {
        const now = shownTheme()?.icons?.[at];
        mark.textContent = now?.mark ?? "";
        mark.style.color = iconColor(now?.color) || "";
      };
      const field = (key, label) => {
        const input = element("input", { className: `report-field prefs-icon-${key}`, type: "text", value: icon[key] ?? "", placeholder: label, ariaLabel: label, spellcheck: false });
        input.addEventListener("change", () => {
          keep((shownTheme()?.icons ?? []).map((one, index) => (index === at ? { ...one, [key]: input.value.trim() } : one)));
          drawMark();
        });
        return input;
      };
      const off = element("button", { type: "button", className: "prefs-key-off", textContent: "×", title: "Take this icon off" });
      off.addEventListener("click", () => {
        keep((shownTheme()?.icons ?? []).filter((_, index) => index !== at));
        draw();
      });
      drawMark();
      return element("div", { className: "prefs-icon-row" }, mark, field("pattern", "Pattern"), field("mark", "Mark"), field("color", "Color"), off);
    });
    const add = element("button", { type: "button", className: "prefs-button", textContent: "Add" });
    add.addEventListener("click", () => {
      keep([...(shownTheme()?.icons ?? []), { pattern: "", mark: "", color: "" }]);
      draw();
      [...body.querySelectorAll(".prefs-icon-pattern")].at(-1)?.focus();
    });
    return element(
      "div",
      { className: "prefs-icons" },
      element("h4", { textContent: "File icons" }),
      element("p", { className: "prefs-icons-said", textContent: "A file takes the first icon whose pattern its name matches, * standing for any letters and ? for one; a color is one of the palette's, as --t2, or any color." }),
      ...rows,
      add,
    );
  }

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
    return element("section", { className: "prefs-theme" }, element("h3", { textContent: "Color Theme" }), element("div", { className: "prefs-line" }, select, name), actions, said, imageRow(), iconsSection());
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
      drawContrast();
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
    row.append(swatch, element("code", { textContent: name }), text, element("span", { className: "prefs-contrast" }), reset);
    return row;
  }

  function colorsSection() {
    const base = paletteOf(scheme());
    const theme = shownTheme();
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
        groups.get(group).push(colorRow(name, base, theme));
      }
      grid.replaceChildren(...[...groups].flatMap(([group, rows]) => [element("h4", { textContent: group }), ...rows]));
    };
    // Each color of text whose contrast with the color under it is under CONTRAST_LEAST, marked on its
    // row with the ratio and the color under it, and listed over the palette.
    drawContrast = () => {
      const colors = new Map(base);
      for (const [name, value] of Object.entries(shownTheme()?.colors ?? {})) {
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
      drawContrast();
    });
    fill();
    drawContrast();
    return element("section", { className: "prefs-palette" }, element("h3", { textContent: "Colors" }), contrasts, filter, grid);
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

  // The settings in force: each setting a settings file sets, its value, the file and the part of it
  // the value comes from, and the files it stands over; the files read, each opened by a press, the
  // tree's in the editor and the reader's own as the system opens it; and what the files could not
  // say.
  function inForceSection() {
    const read = settingsInForce();
    const yours = "your settings.json";
    // A file the tree holds opens in the editor, the reader's own as the system opens it, and any
    // other is named alone.
    const opener = (file) => {
      if (/^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(file) && file !== read.own_file) {
        return element("span", { className: "prefs-file-far", textContent: file });
      }
      const button = element("button", { type: "button", className: "prefs-file", textContent: file === read.own_file ? yours : file, title: `Open ${file}` });
      button.addEventListener("click", async () => {
        if (file === read.own_file) {
          await invoke("home_reveal", { what: "settings" }).catch(() => {});
          return;
        }
        body.closest("dialog")?.close();
        await openFile?.(file);
      });
      return button;
    };
    const named = (source) => {
      const file = source.file === read.own_file ? yours : source.file;
      return source.part ? `${file} for ${source.part}` : file;
    };
    const rows = Object.entries(read.from).map(([key, sources]) => {
      const [first, ...under] = sources;
      const value = key.startsWith("[") ? first.text : (localStorage.getItem(`orior.${key}`) ?? first.text);
      const row = element(
        "div",
        { className: "prefs-force-row" },
        element("code", { className: "prefs-force-key", textContent: key }),
        element("code", { className: "prefs-force-value", textContent: value, title: value }),
        element("span", { className: "prefs-force-from" }, opener(first.file), first.part ? ` for ${first.part}` : null, under.length ? element("span", { className: "prefs-force-under", textContent: ` over ${under.map(named).join(", ")}`, title: under.map((one) => one.file).join("\n") }) : null),
      );
      row.dataset.key = key;
      return row;
    });
    const own = element("button", { type: "button", className: "prefs-button", textContent: "Your settings.json" });
    own.addEventListener("click", () => invoke("home_reveal", { what: "settings" }).catch(() => {}));
    const tree = element("button", { type: "button", className: "prefs-button", textContent: `The tree's ${TREE_FILE}` });
    tree.addEventListener("click", async () => {
      if (!read.files.includes(TREE_FILE)) {
        await invoke("folder_create", { path: TREE_FILE.split("/")[0] }).catch(() => {});
        await invoke("file_write", { path: TREE_FILE, text: "{\n}\n" }).catch(() => {});
      }
      body.closest("dialog")?.close();
      await openFile?.(TREE_FILE);
    });
    return element(
      "section",
      { className: "prefs-force" },
      element("h3", { textContent: "Settings in force" }),
      element("div", { className: "prefs-force-files" }, own, tree),
      rows.length ? element("div", { className: "prefs-force-rows" }, ...rows) : element("p", { className: "prefs-force-none", textContent: "No settings file sets a setting: each is your own or orior's." }),
      read.problems.length ? element("ul", { className: "prefs-force-problems" }, ...read.problems.map((problem) => element("li", { textContent: problem }))) : null,
    );
  }

  // Settings on every machine (carry.js): the file or folder they go through, and each taking kept.
  function carrySection() {
    const section = element("section", { className: "prefs-carry" });
    carryInto(section, { reloadOwn, pick });
    return section;
  }

  // The keys: the set chosen, how a letter's key is bound, each key more than one command holds with
  // those commands, and each command's keys, two at most, each set by pressing it, a button of the
  // mouse past its first three or the wheel turned with a key held, and each taken off by its cross.
  function keysSection() {
    const settings = keySettings();
    const items = menus.flatMap((menu) => (menu.all ?? []).filter((item) => item.command).map((item) => ({ item, label: `${menu.title}: ${item.label.replace(/…$/, "")}` })));
    const set = element("select", { className: "report-field", ariaLabel: "Keys from" }, ...Object.entries(SETS).map(([id, one]) => element("option", { value: id, textContent: one.name, selected: id === settings.set })));
    set.addEventListener("change", () => {
      setKeySettings({ set: set.value });
      draw();
    });
    const by = element(
      "select",
      { className: "report-field", ariaLabel: "A letter's key" },
      element("option", { value: "character", textContent: "the key that types it", selected: settings.by === "character" }),
      element("option", { value: "place", textContent: "the key in its place on the board", selected: settings.by === "place" }),
    );
    by.addEventListener("change", () => {
      setKeySettings({ by: by.value });
      draw();
    });
    const shared = [...sharedKeys(items.map(({ item }) => item))].map(([key, held]) => element("li", {}, element("kbd", { textContent: key }), ` ${held.map((item) => items.find((one) => one.item === item)?.label ?? item.label).join(", ")}`));
    const filter = element("input", { className: "report-field prefs-keys-filter", type: "search", placeholder: "Filter commands", value: state.keysFilter ?? "", ariaLabel: "Filter commands", spellcheck: false });
    const rows = element("div", { className: "prefs-key-rows" });
    const own = (command) => keySettings().own[command];
    const setOwn = (command, keys) => {
      const now = keySettings().own;
      setKeySettings({ own: { ...now, [command]: keys } });
      draw();
    };
    const fill = () => {
      const wanted = filter.value.trim().toLowerCase();
      rows.replaceChildren(
        ...items
          .filter(({ label, item }) => !wanted || label.toLowerCase().includes(wanted) || [item.keys, item.also].some((key) => key?.toLowerCase().includes(wanted)))
          .map(({ item, label }) => {
            const keys = [item.keys, item.also].filter(Boolean);
            const chips = keys.map((key) => {
              const off = element("button", { type: "button", className: "prefs-key-off", textContent: "×", title: `Take ${key} off` });
              off.addEventListener("click", () => setOwn(item.command, keys.filter((one) => one !== key)));
              return element("span", { className: "prefs-key" }, element("kbd", { textContent: key }), off);
            });
            const record = element("button", { type: "button", className: "prefs-button prefs-key-set", textContent: "Set…" });
            record.addEventListener("click", () => {
              record.textContent = "Press the keys";
              const done = (name) => {
                document.removeEventListener("keydown", onKey, true);
                document.removeEventListener("mousedown", onButton, true);
                document.removeEventListener("wheel", onWheel, true);
                if (name) {
                  setOwn(item.command, [name, ...keys.filter((one) => one !== name)].slice(0, 2));
                } else {
                  record.textContent = "Set…";
                }
              };
              const onKey = (event) => {
                event.preventDefault();
                event.stopPropagation();
                if (event.key === "Escape") {
                  done(null);
                } else if (pressName(event)) {
                  done(pressName(event));
                }
              };
              const onButton = (event) => {
                if (buttonName(event)) {
                  event.preventDefault();
                  done(buttonName(event));
                }
              };
              const onWheel = (event) => {
                if (wheelName(event)) {
                  event.preventDefault();
                  done(wheelName(event));
                }
              };
              document.addEventListener("keydown", onKey, true);
              document.addEventListener("mousedown", onButton, true);
              document.addEventListener("wheel", onWheel, { capture: true, passive: false });
            });
            const reset = element("button", { type: "button", className: "prefs-button", textContent: "Reset", disabled: own(item.command) === undefined });
            reset.addEventListener("click", () => {
              const now = { ...keySettings().own };
              delete now[item.command];
              setKeySettings({ own: now });
              draw();
            });
            const row = element("div", { className: "prefs-key-row" }, element("span", { className: "prefs-key-label", textContent: label }), element("span", { className: "prefs-key-keys" }, ...chips), record, reset);
            row.dataset.command = item.command;
            return row;
          }),
      );
    };
    filter.addEventListener("input", () => {
      state.keysFilter = filter.value;
      fill();
    });
    fill();
    return element(
      "section",
      { className: "prefs-keys" },
      element("h3", { textContent: "Keys" }),
      element("label", { className: "report-row" }, element("span", { textContent: "Keys from" }), set),
      element("label", { className: "report-row" }, element("span", { textContent: "A letter's key is" }), by),
      shared.length ? element("div", { className: "prefs-shared" }, element("p", { textContent: "Keys more than one command holds, each acting where its command can:" }), element("ul", {}, ...shared)) : null,
      filter,
      rows,
    );
  }

  function draw() {
    const focused = document.activeElement;
    const focusName = body.contains(focused) ? focused.closest(".prefs-color")?.dataset.name : null;
    const scroll = body.querySelector(".prefs-colors")?.scrollTop ?? 0;
    const keysScroll = body.querySelector(".prefs-key-rows")?.scrollTop ?? 0;
    body.replaceChildren(element("h2", { textContent: "Preferences" }), settingsSection(), inForceSection(), carrySection(), keysSection(), fontsSection(), patternsSection(), themeRow(), colorsSection());
    const keyRows = body.querySelector(".prefs-key-rows");
    if (keyRows) {
      keyRows.scrollTop = keysScroll;
    }
    const grid = body.querySelector(".prefs-colors");
    grid.scrollTop = scroll;
    if (focusName) {
      body.querySelector(`.prefs-color[data-name="${CSS.escape(focusName)}"] .prefs-value`)?.focus();
    }
  }

  draw();
  sheet(body);
  // The files are read again as Preferences opens, so that each setting kept since shows with its
  // file; the settings in force are drawn again where the reading changed them.
  const before = JSON.stringify(settingsInForce());
  readSettings().then((read) => {
    if (body.isConnected && JSON.stringify(read) !== before) {
      body.querySelector(".prefs-force")?.replaceWith(inForceSection());
    }
  });
}
