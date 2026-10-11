// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { OPACITY_SETTINGS, STEPS, opacity, setOpacity } from "../../src/opacity.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { showView } from "../../src/views.js";
import { cssVariable, keepingStorage } from "../helpers.js";

const { test, assert, ui } = window.__harness;

const KEYS = OPACITY_SETTINGS.map((setting) => setting.key);

test("each setting stands at its fallback where nothing is kept", () =>
  keepingStorage(KEYS, () => {
    for (const setting of OPACITY_SETTINGS) {
      localStorage.removeItem(setting.key);
      assert.equal(opacity(setting), setting.fallback, setting.label);
    }
  }));

test("a kept share that is no step reads as the fallback", () =>
  keepingStorage(KEYS, () => {
    const [bar] = OPACITY_SETTINGS;
    for (const wrong of ["35", "-10", "110", "half", ""]) {
      localStorage.setItem(bar.key, wrong);
      assert.equal(opacity(bar), bar.fallback, `kept ${JSON.stringify(wrong)}`);
    }
  }));

test("setting a share keeps it and sets its variable on the root", () =>
  keepingStorage(KEYS, () => {
    const kept = OPACITY_SETTINGS.map((setting) => [setting, cssVariable(setting.name)]);
    try {
      for (const setting of OPACITY_SETTINGS) {
        for (const step of [0, 50, 100]) {
          setOpacity(setting, step);
          assert.equal(opacity(setting), step);
          assert.equal(localStorage.getItem(setting.key), String(step));
          assert.equal(cssVariable(setting.name), `${step}%`);
        }
      }
    } finally {
      for (const [setting, value] of kept) {
        document.documentElement.style.setProperty(setting.name, value);
      }
    }
  }));

test("the steps run from 0 to 100 by tens", () => {
  assert.deepEqual(STEPS, [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]);
});

test("the root has each share as the page started with it", () => {
  for (const setting of OPACITY_SETTINGS) {
    assert.equal(cssVariable(setting.name), `${opacity(setting)}%`, setting.label);
  }
});

// The alpha of a color as the page computes it.
const alphaOf = (color) => {
  const found = color.match(/^rgba\(.*,\s*([\d.]+)\)$/) ?? color.match(/\/\s*([\d.]+)\)$/);
  return color === "transparent" ? 0 : Number(found?.[1] ?? 1);
};

test("each of the panes, the tabs, the editor, the panels and the sheets lays its ground as thick as its own share, and the others stay as they were", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().editor?.s, 5000);
  await keepingStorage(KEYS, () => {
    const sheet = document.body.appendChild(Object.assign(document.createElement("div"), { className: "sheet" }));
    const parts = [
      ["--pane-opacity", document.querySelector(".side")],
      ["--tab-opacity", document.querySelector(".tabs")],
      ["--editor-opacity", document.querySelector(".editor.ed")],
      ["--panel-opacity", document.querySelector(".term")],
      ["--sheet-opacity", sheet],
    ];
    const kept = OPACITY_SETTINGS.map((setting) => [setting, cssVariable(setting.name)]);
    try {
      for (const setting of OPACITY_SETTINGS) {
        setOpacity(setting, 100);
      }
      // Each part's ground at 100%, which the theme may make clear in part on its own.
      const whole = new Map(parts.map(([name, part]) => [name, alphaOf(getComputedStyle(part).backgroundColor)]));
      for (const [name, part] of parts) {
        assert.ok(part, `a part for ${name}`);
        assert.ok(whole.get(name) > 0, `${name}'s part has a ground`);
        const setting = OPACITY_SETTINGS.find((one) => one.name === name);
        setOpacity(setting, 40);
        assert.ok(Math.abs(alphaOf(getComputedStyle(part).backgroundColor) - 0.4 * whole.get(name)) < 0.01, `${name} at 40%: ${getComputedStyle(part).backgroundColor}`);
        for (const [otherName, other] of parts.filter(([one]) => one !== name)) {
          assert.equal(alphaOf(getComputedStyle(other).backgroundColor), whole.get(otherName), `${otherName} stays as it was as ${name} moves`);
        }
        setOpacity(setting, 100);
      }
    } finally {
      sheet.remove();
      for (const [setting, value] of kept) {
        document.documentElement.style.setProperty(setting.name, value);
      }
    }
  });
});

test("in Preferences each part's opacity is a slider that moves by the steps, its share said beside it and the window changing as it moves", async () =>
  keepingStorage(KEYS, async () => {
    const kept = OPACITY_SETTINGS.map((setting) => [setting, cssVariable(setting.name)]);
    runCommand("preferences");
    try {
      await ui.waitFor(() => document.querySelectorAll(".prefs-opacity-slider").length === OPACITY_SETTINGS.length, 5000);
      const sliders = [...document.querySelectorAll(".prefs-opacity-slider")];
      assert.deepEqual(sliders.map((slider) => slider.getAttribute("aria-label")), OPACITY_SETTINGS.map((setting) => setting.label));
      const editor = OPACITY_SETTINGS.find((setting) => setting.name === "--editor-opacity");
      const slider = sliders[OPACITY_SETTINGS.indexOf(editor)];
      const said = slider.parentElement.querySelector(".prefs-opacity-said");
      await ui.click(slider);
      await ui.key("End");
      assert.equal(opacity(editor), 100);
      await ui.key("Left");
      await ui.key("Left");
      assert.equal(opacity(editor), 80, "two steps down");
      assert.equal(said.textContent, "80%");
      assert.equal(cssVariable("--editor-opacity"), "80%", "the window takes it as it moves");
      await ui.key("Home");
      assert.equal(opacity(editor), 0);
      assert.equal(said.textContent, "0%");
    } finally {
      document.querySelector("dialog.sheet")?.close();
      for (const [setting, value] of kept) {
        document.documentElement.style.setProperty(setting.name, value);
      }
    }
  }));
