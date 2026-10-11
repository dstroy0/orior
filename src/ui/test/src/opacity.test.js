// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { OPACITY_SETTINGS, STEPS, opacity, setOpacity } from "../../src/opacity.js";
import { cssVariable, keepingStorage } from "../helpers.js";

const { test, assert } = window.__harness;

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
