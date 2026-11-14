// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Each option of Preferences as a command, and Preferences drawn again where it is open as one changes what it shows.

import { invoke, pick } from "../bridge.js";
import { FONT_SETTINGS, setFont } from "../fonts.js";
import { keysOf, keySettings, SETS, setKeySettings } from "../keys.js";
import { setLanguage } from "../language.js";
import { RETAINED_LEAST, machineCores, setCoresKept } from "../cores.js";
import { OPACITY_SETTINGS, setOpacity, STEPS as OPACITY_STEPS } from "../opacity.js";
import { PATTERN_PARTS, setPatterns, setSets } from "../patterns.js";
import { scheme } from "../scheme.js";
import { settingsInForce, TREE_FILE } from "../settings.js";
import { bringBack, say } from "../statusbar.js";
import { deleteTheme, paletteOf } from "../themes.js";
import { setZoom, STEPS } from "../zoom.js";
import { chooseImage, chooseTheme, exportTheme, IMAGE_SHARES, importTheme, retainIcons, newTheme, removeImage, renameTheme, setColor, shareImage, displayedTheme } from "./theme.js";

// Preferences drawn again where it is open, as a command changes what it shows: `draw` is set as the sheet opens.
export const displayed = { draw: () => {} };

const failed = (text) => say(text, { failed: true });
const shares = (steps, scale) => steps.map((step) => Math.round(step * scale)).join(", ");
const partOf = (setting) => setting.key.split(".").at(-1);

// The keys of the command `command` in force, two at max.
async function keysNow(command) {
  const { allCommands } = await import("../menubar.js");
  const item = allCommands().find((one) => one.command === command);
  return item ? keysOf(command, [item.keys, item.also]) : null;
}

const setOwnKeys = (command, keys) => setKeySettings({ own: { ...keySettings().own, [command]: keys } });

// Each option of Preferences as a command, by the names commands.json gives them, Preferences drawn
// again where it is open.
const PREFERENCES = {
  "prefs-zoom": ([percent]) => {
    const step = STEPS.find((one) => Math.round(one * 100) === Number(percent));
    return step ? setZoom(step) : failed(`Zoom is one of ${shares(STEPS, 100)}.`);
  },
  "prefs-language": ([tag]) => setLanguage(!tag || tag === "system" ? "" : tag),
  "prefs-cores": ([retained]) => {
    const max = Math.max(RETAINED_LEAST, machineCores() - 1);
    const count = Number(retained);
    return Number.isInteger(count) && count >= RETAINED_LEAST && count <= max ? setCoresKept(count) : failed(`Cores retained for you is from ${RETAINED_LEAST} to ${max}.`);
  },
  "prefs-opacity": ([part, percent]) => {
    const setting = OPACITY_SETTINGS.find((one) => partOf(one) === part);
    if (!setting || !OPACITY_STEPS.includes(Number(percent))) {
      failed(`Opacity is of ${OPACITY_SETTINGS.map(partOf).join(", ")}, at one of ${shares(OPACITY_STEPS, 1)}.`);
      return;
    }
    setOpacity(setting, Number(percent));
  },
  "prefs-font": ([name, ...value]) => {
    const setting = FONT_SETTINGS.find((one) => one.name === `--${name}`);
    if (!setting) {
      failed(`A font is one of ${FONT_SETTINGS.map((one) => one.name.slice(2)).join(", ")}.`);
      return;
    }
    const text = value.join(" ").trim();
    setFont(setting.name, setting.size && text ? `${text}px` : text);
  },
  "prefs-patterns": ([part, ...lines]) => (PATTERN_PARTS.some((one) => one.part === part) ? setPatterns(part, lines.join("\n")) : failed(`Patterns are of ${PATTERN_PARTS.map((one) => one.part).join(", ")}.`)),
  "prefs-search-sets": (lines) => setSets(lines.join("\n")),
  "theme-choose": (words) => chooseTheme(words.join(" ")) || failed(`No theme is named ${words.join(" ")}.`),
  "theme-new": () => newTheme(),
  "theme-delete": () => displayedTheme() && deleteTheme(displayedTheme().name),
  "theme-rename": (words) => renameTheme(words.join(" ").trim()),
  "theme-export": () => say(exportTheme()),
  "theme-import": async () => {
    const { text, failed: wrong } = await importTheme();
    say(text, { failed: Boolean(wrong) });
  },
  "theme-color": ([name, ...value]) => {
    const next = value.join(" ").trim();
    if (!paletteOf(scheme()).has(name)) {
      failed(`The palette holds no color ${name}.`);
    } else if (next && !CSS.supports("color", next)) {
      failed(`${next} is not a color.`);
    } else {
      setColor(name, next || null);
    }
  },
  "theme-image": async (words) => {
    const path = words.length ? words.join(" ") : await pick("file").catch(() => null);
    if (typeof path === "string") {
      await chooseImage(path).catch((error) => failed(String(error)));
    }
  },
  "theme-image-share": ([percent]) => {
    const share = IMAGE_SHARES.find((one) => Math.round(one * 100) === Number(percent));
    return share ? shareImage(share) : failed(`The image's share is one of ${shares(IMAGE_SHARES, 100)}.`);
  },
  "theme-image-remove": () => removeImage(),
  // The icon of the pattern given, changed, or added after the others where none has that pattern.
  "theme-icon": ([pattern, mark = "", color = ""]) => {
    const icons = displayedTheme()?.icons ?? [];
    const at = icons.findIndex((one) => one.pattern === pattern);
    retainIcons(at < 0 ? [...icons, { pattern, mark, color }] : icons.map((one, index) => (index === at ? { pattern, mark, color } : one)));
  },
  "theme-icon-remove": ([pattern]) => retainIcons((displayedTheme()?.icons ?? []).filter((one) => one.pattern !== pattern)),
  "keys-from": ([set]) => (SETS[set] ? setKeySettings({ set }) : failed(`Keys are from ${Object.keys(SETS).join(", ")}.`)),
  "keys-character": ([by]) => (["character", "place"].includes(by) ? setKeySettings({ by }) : failed("A character's key is by character or by place.")),
  // A command's keys, the keys given first and the one it retained before after them, two at max.
  "key-set": async ([command, keys]) => {
    const now = await keysNow(command);
    return now && keys ? setOwnKeys(command, [keys, ...now.filter((one) => one !== keys)].slice(0, 2)) : failed(`No command ${command} takes keys ${keys ?? ""}.`);
  },
  "key-off": async ([command, keys]) => {
    const now = await keysNow(command);
    return now ? setOwnKeys(command, now.filter((one) => one !== keys)) : failed(`No command is named ${command}.`);
  },
  "key-reset": ([command]) => {
    const own = { ...keySettings().own };
    delete own[command];
    setKeySettings({ own });
  },
  "notice-show-again": (words) => bringBack(words.join(" ")),
  "settings-own": () => invoke("home_reveal", { what: "settings" }).catch((error) => failed(String(error))),
  "settings-tree": async () => {
    if (!settingsInForce().files.includes(TREE_FILE)) {
      await invoke("folder_create", { path: TREE_FILE.split("/")[0] }).catch(() => {});
      await invoke("file_write", { path: TREE_FILE, text: "{\n}\n" }).catch(() => {});
    }
    document.querySelector(".sheet-prefs")?.closest("dialog")?.close();
    const [{ openFile }, { showView }] = await Promise.all([import("../edit.js"), import("../views.js")]);
    showView("edit");
    await openFile(TREE_FILE);
  },
};

export const prefsCommands = Object.fromEntries(
  Object.entries(PREFERENCES).map(([name, run]) => [
    name,
    async (args) => {
      await run(args);
      displayed.draw();
    },
  ]),
);
