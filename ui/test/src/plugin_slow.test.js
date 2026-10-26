// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find the Slow Plugin: the plugins halved until one is named, each set back as it was.

import { finding, findSlowPlugin } from "../../gui/src-js/plugin_slow.js";
import { frame, press, putBackAsFound, told } from "../menus.js";
import { pluginReadings, pluginsOff } from "../../gui/src-js/plugins.js";

const { test, assert, ui } = window.__harness;

test("View, Find the Slow Plugin halves the plugins on until one is named, or says none is on, and sets each back", async () => {
  const off = localStorage.getItem("orior.plugins.off");
  await press("find-slow-plugin");
  // Begun again before it ends, it starts over from the plugins off before the first began.
  await press("find-slow-plugin");
  const finder = document.getElementById("slow-finder");
  while (!finder.hidden) {
    await ui.click([...finder.querySelectorAll(".slow-finder-button")].find((button) => button.textContent === "No Longer Slow"));
    await frame();
  }
  assert.ok(told(/No plugin is on|is the plugin that slows the window/), "the finding ends with what it found");
  assert.equal(localStorage.getItem("orior.plugins.off"), off ?? "[]", "each plugin set back as it was");
});

test("Find the Slow Plugin halves the plugins on until one is left, and sets each back as it was", async () => {
  const before = [...pluginsOff()].sort();
  const on = pluginReadings().filter((reading) => !pluginsOff().has(reading.id));
  assert.ok(on.length > 2, "more than two plugins on to halve");
  const culprit = on.at(-1).id;
  findSlowPlugin();
  for (let step = 0; step < 12 && finding(); step += 1) {
    await ui.waitFor(() => !document.getElementById("slow-finder").hidden, 2000);
    const off = finding().half.includes(culprit);
    await ui.click([...document.querySelectorAll(".slow-finder-button")].find((one) => one.textContent === (off ? "No Longer Slow" : "Still Slow")));
  }
  assert.equal(finding(), null, "it ended");
  assert.ok(document.getElementById("status-said").textContent.startsWith(`${on.at(-1).name} is the plugin that slows the window`));
  assert.deepEqual([...pluginsOff()].sort(), before, "each set back as it was");
});

test("the window is put back as the file found it", putBackAsFound);
