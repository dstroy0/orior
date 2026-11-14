// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences' settings, color theme, keys, notices and settings files, each from the command line.

import { editing, saving, vimKeys } from "../../../gui/src-js/edit.js";
import { scheme } from "../../../gui/src-js/scheme.js";
import { zoom } from "../../../gui/src-js/zoom.js";
import { fonts } from "../../../gui/src-js/fonts.js";
import { coresKept } from "../../../gui/src-js/cores.js";
import { opacity, OPACITY_SETTINGS } from "../../../gui/src-js/opacity.js";
import { patternsOf, setsText } from "../../../gui/src-js/patterns.js";
import { chosen, deleteTheme, themeNamed } from "../../../gui/src-js/themes.js";
import { keySettings, setKeySettings } from "../../../gui/src-js/keys.js";
import { noticesAway, putAway } from "../../../gui/src-js/statusbar.js";
import { carryPlace, setCarryPlace } from "../../../gui/src-js/carry.js";
import { asFound, calls, made, prefsField, press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Preferences' settings, from the command line: zoom, language, cores, opacity, the settings no menu lists, fonts and patterns, Preferences open showing each", async () => {
  asFound();
  const was = { zoom: Math.round(zoom() * 100), language: localStorage.getItem("orior.language"), cores: coresKept(), search: patternsOf("search"), sets: setsText(), trim: saving("trim"), vim: vimKeys() };
  const menu = OPACITY_SETTINGS.find((one) => one.key === "orior.opacity.menu");
  const menuWas = opacity(menu);
  await press("preferences");
  await ui.waitFor(() => prefsField("Zoom"), 5000, "Preferences");
  try {
    await press("prefs-zoom", "125");
    assert.equal(zoom(), 1.25);
    assert.equal(prefsField("Zoom").value, "1.25", "Preferences shows the zoom");
    await press("prefs-language", "en");
    assert.equal(localStorage.getItem("orior.language"), "en");
    await press("prefs-cores", "1");
    assert.equal(coresKept(), 1);
    assert.equal(prefsField("Cores retained for you").value, "1");
    await press("prefs-opacity", "menu", menuWas === 70 ? "80" : "70");
    assert.equal(opacity(menu), menuWas === 70 ? 80 : 70);
    assert.equal(document.documentElement.style.getPropertyValue("--menu-opacity"), `${opacity(menu)}%`);
    await press("prefs-setting", "trim", was.trim ? "off" : "on");
    assert.equal(saving("trim"), !was.trim);
    await press("prefs-setting", "vim-keys", was.vim ? "off" : "on");
    assert.equal(vimKeys(), !was.vim);
    await press("prefs-font", "code-size", "15");
    assert.equal(fonts()["--code-size"], "15px");
    assert.equal(prefsField("Code size").value, "15");
    await press("prefs-font", "code-size");
    assert.equal(fonts()["--code-size"], undefined, "the stylesheet's size again");
    await press("prefs-patterns", "search", "*.made", "made_*");
    assert.equal(patternsOf("search"), "*.made\nmade_*");
    await press("prefs-search-sets", "Made:", "src/**");
    assert.equal(setsText(), "Made:\nsrc/**");
    assert.equal(prefsField("Sets of files to search").value, "Made:\nsrc/**");
  } finally {
    await press("prefs-zoom", String(was.zoom));
    await press("prefs-language", was.language ?? "system");
    await press("prefs-cores", String(was.cores));
    await press("prefs-opacity", "menu", String(menuWas));
    await press("prefs-setting", "trim", was.trim ? "on" : "off");
    await press("prefs-setting", "vim-keys", was.vim ? "on" : "off");
    await press("prefs-patterns", "search", ...(was.search ? was.search.split("\n") : []));
    await press("prefs-search-sets", ...(was.sets ? was.sets.split("\n") : []));
    asFound();
  }
});

test("Preferences' color theme, from the command line: a theme made, named, colored, given an image and icons, copied out, read in, chosen and deleted", async () => {
  asFound();
  const base = scheme();
  const wasChosen = chosen(base);
  const clipped = [];
  calls.answer("clip_write", ({ text }) => clipped.push(text));
  calls.answer("image_data", () => "data:image/png;base64,iVBORw0KGgo=");
  try {
    await press("theme-new");
    assert.ok(chosen(base), "a theme of the reader's chosen");
    await press("theme-rename", "Made", "Theme");
    assert.equal(chosen(base), "Made Theme");
    await press("theme-color", "--t2", "#123456");
    assert.equal(themeNamed("Made Theme").colors["--t2"], "#123456");
    await press("theme-color", "--t2");
    assert.ok(!("--t2" in themeNamed("Made Theme").colors), "the scheme's own color again");
    await press("theme-image", "picture.png");
    assert.equal(themeNamed("Made Theme").image?.src, "data:image/png;base64,iVBORw0KGgo=");
    await press("theme-image-share", "50");
    assert.equal(themeNamed("Made Theme").image.opacity, 0.5);
    await press("theme-image-remove");
    assert.equal(themeNamed("Made Theme").image, undefined);
    await press("theme-icon", "*.made", "M", "--t2");
    assert.deepEqual(themeNamed("Made Theme").icons, [{ pattern: "*.made", mark: "M", color: "--t2" }]);
    await press("theme-icon", "*.made", "N");
    assert.equal(themeNamed("Made Theme").icons[0].mark, "N", "the pattern's icon changed");
    await press("theme-icon-remove", "*.made");
    assert.deepEqual(themeNamed("Made Theme").icons, []);
    await press("theme-export");
    assert.ok(clipped.at(-1)?.includes("Made Theme"), "the theme copied out");
    calls.answer("clip_read", () => clipped.at(-1).replace("Made Theme", "Read Theme"));
    await press("theme-import");
    assert.equal(chosen(base), "Read Theme", "read in and chosen");
    await press("theme-choose", "Made", "Theme");
    assert.equal(chosen(base), "Made Theme");
    await press("theme-delete");
    assert.equal(themeNamed("Made Theme"), null);
    await press("theme-choose", base);
    assert.equal(chosen(base), null, "the scheme's own theme");
  } finally {
    for (const name of ["Made Theme", "Read Theme"]) {
      if (themeNamed(name)) {
        deleteTheme(name);
      }
    }
    await press("theme-choose", ...(wasChosen ?? base).split(" "));
  }
});

test("Preferences' keys, from the command line: the set, a character's key, and a command's keys set, taken off and reset", async () => {
  const was = keySettings();
  try {
    await press("keys-from", "jetbrains");
    assert.equal(keySettings().set, "jetbrains");
    await press("keys-from", "orior");
    await press("keys-character", "place");
    assert.equal(keySettings().by, "place");
    await press("keys-character", "character");
    await press("key-set", "save-all", "Ctrl+Alt+8");
    assert.equal(keySettings().own["save-all"][0], "Ctrl+Alt+8");
    await press("key-off", "save-all", "Ctrl+Alt+8");
    assert.ok(!keySettings().own["save-all"].includes("Ctrl+Alt+8"), "taken off");
    await press("key-reset", "save-all");
    assert.equal(keySettings().own["save-all"], undefined, "its own keys again");
  } finally {
    setKeySettings(was);
  }
});

test("Preferences' notices, settings files and settings on every machine, from the command line", async () => {
  asFound();
  putAway("put away by the test");
  await press("notice-show-again", "put", "away", "by", "the", "test");
  assert.ok(!noticesAway().includes("put away by the test"), "displayed again");
  await press("settings-own");
  assert.ok(made("home_reveal").some((one) => one.args.what === "settings"), "your settings.json displayed");
  await press("settings-tree");
  await ui.waitFor(() => editing().active === ".orior/settings.json", 5000, "the tree's settings open");
  const wasPlace = carryPlace();
  const folder = window.__harness.servers.ORIOR_TEST_HOME.replace(/\\/g, "/");
  calls.answer("carry_send", ({ to }) => `Sent to ${to} by the test.`);
  calls.answer("carry_read", () => ({ differences: [{ place: "keys.json", how: "changed", settings: [] }, { place: "settings.json", how: "changed", settings: [{ key: "zoom", here: "1", there: "1.25" }] }], there: { "keys.json": "{}" } }));
  calls.answer("carry_take", ({ from, places }) => ({ id: "taken", from, files: places, at: 0 }));
  calls.answer("carry_undo", ({ id }) => ({ id, from: folder, files: [], at: 0 }));
  calls.answer("retained_now", () => ({ set: {}, gone: [] }));
  try {
    await press("carry-place", folder);
    assert.equal(carryPlace(), folder);
    await press("carry-send");
    assert.equal(made("carry_send").at(-1).args.to, folder);
    await press("carry-take", folder, "keys.json");
    assert.deepEqual([made("carry_take").at(-1).args.places, made("carry_take").at(-1).args.keys], [["keys.json"], []]);
    await press("carry-take");
    assert.deepEqual([made("carry_take").at(-1).args.places, made("carry_take").at(-1).args.keys], [["keys.json", "settings.json"], ["zoom"]]);
    await press("carry-undo", "taken");
    assert.equal(made("carry_undo").at(-1).args.id, "taken");
  } finally {
    setCarryPlace(wasPlace);
  }
});

test("the window is put back as the file found it", putBackAsFound);
