// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sections of Preferences that show the window's settings, each drawn on its own: the settings with each one a
// menu turns on or off, the fonts, the files' patterns and sets, and the notices put away with a press that shows a
// type again.

import { coresKept, KEPT_LEAST } from "../../../gui/src-js/cores.js";
import { FONT_SETTINGS, fontDefault } from "../../../gui/src-js/fonts.js";
import { OPACITY_SETTINGS } from "../../../gui/src-js/opacity.js";
import { PATTERN_PARTS, patternsOf, setsText } from "../../../gui/src-js/patterns.js";
import { fontsSection, noticesSection, patternsSection, settingsSection } from "../../../gui/src-js/preferences/sections.js";
import { noticesAway, putAway } from "../../../gui/src-js/statusbar.js";
import { zoom } from "../../../gui/src-js/zoom.js";
import { putBackAsFound } from "../../menus.js";

const { test, assert } = window.__harness;

const boxNamed = (section, text) => [...section.querySelectorAll(".report-check")].find((one) => one.textContent === text).querySelector("input");

test("the settings: the zoom, the cores kept, each part's opacity, and a box for each setting a menu turns on or off", async () => {
  let wrapped = false;
  let more = true;
  const ran = [];
  const section = settingsSection({
    menus: [{ all: [{ command: "wrap", checks: "wrap", args: "[on|off]", label: "Word Wrap" }, { command: "save", label: "Save" }] }],
    runCommand: async (command, words) => {
      ran.push([command, ...words]);
      wrapped = words[0] === "on";
    },
    checks: { wrap: () => wrapped },
    more: [{ label: "A setting no menu lists", on: () => more, set: (on) => (more = on) }],
  });
  assert.equal(Number(section.querySelector('select[aria-label="Zoom"]').value), zoom());
  const cores = section.querySelector(".prefs-cores");
  assert.equal(Number(cores.options[0].value), KEPT_LEAST);
  assert.equal(Number(cores.value), coresKept());
  assert.equal(section.querySelectorAll(".prefs-opacity-slider").length, OPACITY_SETTINGS.length);
  assert.equal(section.querySelectorAll(".report-check").length, 2, "Save is no setting");
  const wrap = boxNamed(section, "Word Wrap");
  assert.ok(!wrap.checked);
  wrap.checked = true;
  wrap.dispatchEvent(new Event("change"));
  await new Promise((done) => setTimeout(done, 0));
  assert.deepEqual(ran, [["wrap", "on"]]);
  assert.ok(wrap.checked, "the box as the setting now stands");
  const other = boxNamed(section, "A setting no menu lists");
  other.checked = false;
  other.dispatchEvent(new Event("change"));
  assert.equal(more, false);
});

test("the fonts, each a field with the stylesheet's own font shown while it is empty, a size bounded", () => {
  const fields = [...fontsSection().querySelectorAll(".prefs-font")];
  assert.equal(fields.length, FONT_SETTINGS.length);
  FONT_SETTINGS.forEach(({ name, size }, at) => {
    assert.equal(fields[at].type, size ? "number" : "text");
    assert.equal(fields[at].placeholder, size ? String(Number.parseFloat(fontDefault(name))) : fontDefault(name));
    if (size) {
      assert.deepEqual([fields[at].min, fields[at].max], ["9", "32"]);
    }
  });
});

test("the files' patterns, one field for each part, and the sets of files to search last", () => {
  const fields = [...patternsSection().querySelectorAll("textarea")];
  assert.equal(fields.length, PATTERN_PARTS.length + 1);
  PATTERN_PARTS.forEach(({ part }, at) => assert.equal(fields[at].value, patternsOf(part)));
  assert.equal(fields.at(-1).value, setsText());
});

test("none put away is said, and each notice put away has a press that shows its kind again", () => {
  localStorage.setItem("orior.notices-away", "[]");
  assert.match(noticesSection({ draw: () => {} }).querySelector(".prefs-said").textContent, /^No notice is put away/);
  putAway("A kind of the test's");
  let drawn = 0;
  const section = noticesSection({ draw: () => (drawn += 1) });
  section.querySelector('[aria-label="Show again: A kind of the test\'s"]').click();
  assert.deepEqual(noticesAway(), []);
  assert.equal(drawn, 1, "the sheet drawn again");
});

test("the window is put back as the file found it", putBackAsFound);
