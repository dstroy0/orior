// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences: every option it holds changed as a person changes it, and put back. Each box is
// pressed and pressed again, each slider stepped and stepped back by its keys, each list stepped and
// stepped back by its keys, and nothing the page does as they change throws.

import { runCommand } from "../../src/menubar.js";

const { test, assert, ui, calls } = window.__harness;

const wait = (ms) => new Promise((done) => setTimeout(done, ms));
const sheetOf = () => document.querySelector("dialog[open] .sheet-prefs");
const shown = (node) => node.isConnected && node.getClientRects().length > 0 && !node.disabled;

test(
  "every option of Preferences is changed as a person changes it, put back, and none throws",
  async () => {
    const threw = [];
    let changing = "";
    const heard = (event) => threw.push(`${changing}: ${event.error?.message ?? event.reason?.message ?? event.reason ?? event.message}`);
    addEventListener("error", heard);
    addEventListener("unhandledrejection", heard);
    let changed = 0;
    // The setting of reports sent on their own is kept by the tree's side, which the harness holds
    // from the network; here it is kept as the tree's side keeps it.
    calls.answer("report_auto_set", () => null);
    try {
      runCommand("preferences");
      const sheet = await ui.waitFor(sheetOf, 5000);
      const name = (node) => node.ariaLabel || node.closest("label")?.textContent?.trim() || node.className;

      // The field at `at` of those `selector` finds, found again each time it is read: a change
      // can draw the sheet again, and the field with it.
      const nth = (selector, at) => [...sheet.querySelectorAll(selector)].filter(shown)[at];
      const count = (selector) => [...sheet.querySelectorAll(selector)].filter(shown).length;

      for (let at = 0, boxes = count("input[type=checkbox]"); at < boxes; at++) {
        const box = () => nth("input[type=checkbox]", at);
        changing = `the box ${name(box())}`;
        const was = box().checked;
        await ui.click(box());
        await wait(80);
        assert.equal(box().checked, !was, `${changing} turns`);
        await ui.click(box());
        await wait(80);
        assert.equal(box().checked, was, `${changing} turns back`);
        changed++;
      }

      for (let at = 0, sliders = count("input[type=range]"); at < sliders; at++) {
        const slider = () => nth("input[type=range]", at);
        changing = `the slider ${name(slider())}`;
        const was = slider().value;
        const up = Number(slider().value) < Number(slider().max);
        slider().focus();
        await ui.key(up ? "Right" : "Left");
        await wait(80);
        assert.ok(slider().value !== was, `${changing} moves`);
        await ui.key(up ? "Left" : "Right");
        await wait(80);
        assert.equal(slider().value, was, `${changing} moves back`);
        changed++;
      }

      for (let at = 0, lists = count("select"); at < lists; at++) {
        const list = () => nth("select", at);
        changing = `the list ${name(list())}`;
        const was = list().selectedIndex;
        if (list().options.length < 2) {
          continue;
        }
        const down = was < list().options.length - 1;
        list().focus();
        await ui.key(down ? "Down" : "Up");
        await wait(80);
        assert.ok(list().selectedIndex !== was, `${changing} steps`);
        await ui.key(down ? "Up" : "Down");
        await wait(80);
        assert.equal(list().selectedIndex, was, `${changing} steps back`);
        assert.equal(document.activeElement, list(), `${changing} keeps the keys`);
        changed++;
      }
    } finally {
      removeEventListener("error", heard);
      removeEventListener("unhandledrejection", heard);
      document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
    }
    assert.ok(changed > 0, "Preferences holds options to change");
    assert.deepEqual(threw, [], "what changing the options threw");
  },
  { timeout: 600_000 },
);
