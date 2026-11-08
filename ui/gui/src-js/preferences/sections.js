// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sections of Preferences that show the window's settings: the zoom, the language, the cores kept, each part's
// opacity and each setting a menu turns on or off; the fonts; the files' patterns; the settings in force; the
// notices put away; and the settings carried to another machine. Each section takes the sheet it is drawn in.

import { invoke, pick } from "../bridge.js";
import { carryInto } from "../carry.js";
import { coresKept, KEPT_LEAST, machineCores, setCoresKept } from "../cores.js";
import { FONT_SETTINGS, fontDefault, fonts, setFont } from "../fonts.js";
import { chosenLanguage, languages, setLanguage } from "../language.js";
import { OPACITY_SETTINGS, opacity, setOpacity, STEPS as OPACITY_STEPS } from "../opacity.js";
import { PATTERN_PARTS, patternsOf, setPatterns, setSets, setsText } from "../patterns.js";
import { settingsInForce, TREE_FILE } from "../settings.js";
import { bringBack, noticesAway } from "../statusbar.js";
import { setZoom, STEPS, zoom } from "../zoom.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function settingsSection(at) {
  const { menus, runCommand, checks, more } = at;
  const rows = [];
  const zoomSelect = element("select", { className: "report-field", ariaLabel: "Zoom" }, ...STEPS.map((step) => element("option", { value: String(step), textContent: `${Math.round(step * 100)}%`, selected: step === zoom() })));
  zoomSelect.addEventListener("change", () => setZoom(Number(zoomSelect.value)));
  rows.push(element("label", { className: "report-row" }, element("span", { textContent: "Zoom" }), zoomSelect));
  // The window's language: the system's, English, or one the reader has a language file for.
  const own = localStorage.getItem("orior.language") ?? "";
  const languageSelect = element(
    "select",
    { className: "report-field prefs-language", ariaLabel: "Language" },
    element("option", { value: "", textContent: `The system's, ${navigator.language}`, selected: !own }),
    element("option", { value: "en", textContent: "English", selected: own === "en" }),
  );
  languages().then((tags) => {
    for (const tag of tags.filter((one) => one !== "en")) {
      languageSelect.append(element("option", { value: tag, textContent: tag, selected: own === tag }));
    }
    languageSelect.value = own && [...languageSelect.options].some((option) => option.value === own) ? own : own ? chosenLanguage() : "";
  });
  languageSelect.addEventListener("change", () => setLanguage(languageSelect.value));
  rows.push(element("label", { className: "report-row" }, element("span", { textContent: "Language" }), languageSelect));
  // The cores kept for the reader, from KEPT_LEAST to all the machine's but one.
  const machine = machineCores();
  const cores = element("select", { className: "report-field prefs-cores", ariaLabel: "Cores kept for you" }, ...Array.from({ length: Math.max(1, machine - KEPT_LEAST) }, (_, at) => KEPT_LEAST + at).map((kept) => element("option", { value: String(kept), textContent: `${kept} of ${machine}`, selected: kept === coresKept() })));
  cores.addEventListener("change", () => setCoresKept(Number(cores.value)));
  rows.push(element("label", { className: "report-row" }, element("span", { textContent: "Cores kept for you" }), cores));
  // Each part's opacity a slider by the steps, its share said beside it, the window changing as it moves.
  for (const setting of OPACITY_SETTINGS) {
    const slider = element("input", { type: "range", className: "prefs-opacity-slider", min: String(OPACITY_STEPS[0]), max: String(OPACITY_STEPS.at(-1)), step: String(OPACITY_STEPS[1] - OPACITY_STEPS[0]), value: String(opacity(setting)), ariaLabel: setting.label });
    const said = element("span", { className: "prefs-opacity-said", textContent: `${opacity(setting)}%` });
    // the share the slider stands at, for the track's fill to stop at
    slider.style.setProperty("--share", slider.value);
    slider.addEventListener("input", () => {
      setOpacity(setting, Number(slider.value));
      said.textContent = `${slider.value}%`;
      slider.style.setProperty("--share", slider.value);
    });
    rows.push(element("label", { className: "report-row" }, element("span", { textContent: setting.label }), element("span", { className: "prefs-opacity" }, slider, said)));
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
export function fontsSection() {
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
export function patternsSection() {
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
export function inForceSection(at) {
  const { body, openFile } = at;
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
// The notices put away for good, each with a press that shows its type again.
export function noticesSection(at) {
  const { draw } = at;
  const away = noticesAway();
  const rows = away.map((type) => {
    const back = element("button", { type: "button", className: "prefs-button", textContent: "Show Again", ariaLabel: `Show again: ${type}` });
    back.addEventListener("click", () => {
      bringBack(type);
      draw();
    });
    return element("div", { className: "prefs-line prefs-notice-away" }, element("span", { textContent: type }), back);
  });
  return element(
    "section",
    { className: "prefs-notices" },
    element("h3", { textContent: "Notices" }),
    ...(rows.length ? rows : [element("p", { className: "prefs-said", textContent: "No notice is put away. Silence, beside a notice on the status bar, puts its kind away." })]),
  );
}

export function carrySection(at) {
  const { reloadOwn } = at;
  const section = element("section", { className: "prefs-carry" });
  carryInto(section, { reloadOwn, pick });
  return section;
}
