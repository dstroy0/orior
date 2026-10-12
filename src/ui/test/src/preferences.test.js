// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences: every option it holds changed as a person changes it, and put back. Each box is
// pressed and pressed again, each slider stepped and stepped back by its keys, each list stepped and
// stepped back by its keys, and nothing the page does as they change throws.

import { runCommand } from "../../src/menubar.js";

const { test, assert, ui } = window.__harness;

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
    try {
      runCommand("preferences");
      const sheet = await ui.waitFor(sheetOf, 5000);
      const name = (node) => node.ariaLabel || node.closest("label")?.textContent?.trim() || node.className;

      for (const box of [...sheet.querySelectorAll("input[type=checkbox]")].filter(shown)) {
        changing = `the box ${name(box)}`;
        const was = box.checked;
        await ui.click(box);
        await wait(80);
        assert.equal(box.checked, !was, `${changing} turns`);
        await ui.click(box);
        await wait(80);
        assert.equal(box.checked, was, `${changing} turns back`);
        changed++;
      }

      for (const slider of [...sheet.querySelectorAll("input[type=range]")].filter(shown)) {
        changing = `the slider ${name(slider)}`;
        const was = slider.value;
        slider.focus();
        const up = Number(slider.value) < Number(slider.max);
        await ui.key(up ? "Right" : "Left");
        await wait(80);
        assert.ok(slider.value !== was, `${changing} moves`);
        await ui.key(up ? "Left" : "Right");
        await wait(80);
        assert.equal(slider.value, was, `${changing} moves back`);
        changed++;
      }

      for (const list of [...sheet.querySelectorAll("select")].filter(shown)) {
        changing = `the list ${name(list)}`;
        const was = list.selectedIndex;
        if (list.options.length < 2) {
          continue;
        }
        list.focus();
        const down = was < list.options.length - 1;
        await ui.key(down ? "Down" : "Up");
        await wait(80);
        assert.ok(list.selectedIndex !== was, `${changing} steps`);
        await ui.key(down ? "Up" : "Down");
        await wait(80);
        assert.equal(list.selectedIndex, was, `${changing} steps back`);
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
