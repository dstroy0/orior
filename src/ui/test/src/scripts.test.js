// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Plugins that run code of their own, each in a worker apart from the editor: what one draws through
// orior's calls, one that stops answering named and stopped while the keys go on, Find the Slow
// Plugin, and a write the system refuses told to the plugin after the reader is asked.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { loadPlugins, pluginReadings, pluginsOff } from "../../src/plugins.js";
import { allowPlugin, findSlowPlugin, finding, running } from "../../src/scripts.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const DRAWS = `orior.on("open", async ({ path }) => {
  if (!path.endsWith("drawn.py")) {
    return;
  }
  await orior.marks.set(path, [{ line: 1, text: "two words", kind: "warn", title: "a note" }]);
  await orior.tabs.label(path, "checked");
  await orior.panels.show("main", { title: "Drawn", html: "<p id='said'>from the plugin</p>" });
  await orior.overlays.show("first", { path, line: 0, text: "over the first line" });
  const colors = await orior.theme.colors();
  await orior.chrome.set({ badge: "drawn " + (colors.signal ? "in the theme's colors" : "without"), accent: "rgb(1, 2, 3)" });
  try {
    await orior.files.write("locked_by_plugin.txt", "x");
  } catch (error) {
    await orior.tabs.label(path, error.refused ? "refused" : "failed");
  }
});
`;

const STUCK = `orior.on("open", ({ path }) => {
  if (path.endsWith("stuck.txt")) {
    const until = Date.now() + 60000;
    while (Date.now() < until) {}
  }
});
`;

// Writes a script plugin of the reader's, asking for what `asks` names: reads, writes and reaches.
async function plugin(id, name, code, asks = {}) {
  const home = await invoke("home_folder");
  await invoke("file_write_any", { path: `${home}/plugins/${id}/plugin.json`, text: JSON.stringify({ id, name, version: "1.0.0", kind: "script", api: 1, ...asks }) });
  await invoke("file_write_any", { path: `${home}/plugins/${id}/plugin.js`, text: code });
}

const tabNamed = (name) => [...document.querySelectorAll("#tabs .tab")].find((one) => one.textContent.includes(name));

test("a plugin draws marks, a tab's label, a panel, an overlay and the chrome through orior's calls", async () => {
  await plugin("draws", "Draws", DRAWS, { writes: ["locked_by_plugin.txt"] });
  await invoke("folder_create", { path: "locked_by_plugin.txt" }).catch(() => {});
  await loadPlugins();
  assert.ok(!running().has("draws"), "a plugin that asks to write runs only once it is let");
  await allowPlugin("draws");
  await ui.waitFor(() => running().has("draws"), 5000);
  await invoke("file_write", { path: "drawn.py", text: "first = 1\nsecond = 2\nthird = 3\n" });
  showView("edit");
  await openFile("drawn.py");
  await ui.waitFor(() => document.querySelector(".ed-plugin-text"), 5000);
  assert.ok(document.querySelector(".ed-plugin.warn"), "a mark in the gutter in the color of its kind");
  assert.equal(document.querySelector(".ed-plugin-text").textContent, "two words");
  assert.equal(document.querySelector(".plugin-overlay").textContent, "over the first line");
  const panel = document.querySelector(".plugin-panel");
  assert.equal(panel.querySelector("header span").textContent, "Drawn");
  assert.equal(panel.querySelector("iframe").getAttribute("sandbox"), "", "a panel runs none of its scripts");
  assert.match(panel.querySelector("iframe").srcdoc, /from the plugin/);
  assert.equal(document.getElementById("plugin-badge").textContent, "drawn in the theme's colors");
  assert.equal(getComputedStyle(document.documentElement).getPropertyValue("--signal").trim(), "rgb(1, 2, 3)");
  await ui.waitFor(() => !document.getElementById("rights-ask").hidden, 5000);
  assert.match(document.querySelector(".rights-said").textContent, /^Draws asks to write locked_by_plugin\.txt, and the system refuses it the rights/);
  await ui.click([...document.querySelectorAll(".rights-button")].find((one) => one.textContent === "Leave It"));
  await ui.waitFor(() => tabNamed("drawn.py")?.querySelector(".tab-label")?.textContent === "refused", 5000);
});

test("a plugin that stops answering is named with how long and stopped, and the keys go on meanwhile", async () => {
  await plugin("stuck", "Stuck", STUCK);
  await loadPlugins();
  await ui.waitFor(() => running().has("stuck"), 5000);
  await invoke("file_write", { path: "stuck.txt", text: "one\n" });
  await openFile("stuck.txt");
  await ui.waitFor(() => /^Stuck has not answered for \d+ s/.test(document.getElementById("status-plugin").textContent), 8000);
  const { editor } = editing();
  editor.goTo(0, 3);
  editor.focus();
  await ui.type(" two");
  assert.equal(editor.s.doc.line(0), "one two", "the keys are not held up");
  await ui.click(document.querySelector(".status-plugin-stop"));
  await ui.waitFor(() => !running().has("stuck") && document.getElementById("status-plugin").hidden, 3000);
  assert.ok(running().has("draws"), "the others run on");
});

test("Find the Slow Plugin halves the plugins on until one is left, and sets each back as it was", async () => {
  const before = [...pluginsOff()].sort();
  const culprit = "stuck";
  findSlowPlugin();
  for (let step = 0; step < 12 && finding(); step += 1) {
    await ui.waitFor(() => !document.getElementById("slow-finder").hidden, 2000);
    const off = finding().half.includes(culprit);
    await ui.click([...document.querySelectorAll(".slow-finder-button")].find((one) => one.textContent === (off ? "No Longer Slow" : "Still Slow")));
  }
  assert.equal(finding(), null, "it ended");
  assert.match(document.getElementById("status-said").textContent, /^Stuck is the plugin that slows the window/);
  assert.deepEqual([...pluginsOff()].sort(), before, "each set back as it was");
  assert.ok(pluginReadings().length > 2);
});
