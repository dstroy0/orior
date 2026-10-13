// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Settings on every machine: the reader's settings sent to a file, what a taking would change shown
// first and taken a setting at a time, the taking undone, this machine's own settings staying, and a
// plugin's own settings kept apart and carried with it.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { loadPlugins } from "../../src/plugins.js";
import { ownItem } from "../../src/settings.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

// Long enough for the page's entries to be kept in orior's folder.
const kept = () => new Promise((done) => setTimeout(done, 700));

const differenceOf = (place) => document.querySelector(`.carry-difference[data-place="${place}"]`);

test("settings sent to a file are taken back a setting at a time, after what would change is shown, and the taking is undone", async () => {
  const root = document.getElementById("tree-path").textContent.replace(/[\\/]+$/, "");
  const file = `${root}/carried_settings.json`;
  localStorage.setItem("orior.crumbs", "false");
  localStorage.setItem("orior.memory-budget", "900");
  await kept();
  assert.match(await invoke("carry_send", { to: file }), /files written to/);
  const sent = JSON.parse(await invoke("file_read_any", { path: file }));
  assert.ok(!sent.files["settings.json"].includes("memory-budget"), "a setting of this machine alone stays here");
  localStorage.setItem("orior.crumbs", "true");
  localStorage.setItem("orior.past-ends", "true");
  localStorage.setItem("orior.memory-budget", "400");
  await kept();
  runCommand("preferences");
  await ui.waitFor(() => document.querySelector(".carry-place"), 5000);
  const place = document.querySelector(".carry-place");
  await ui.click(place);
  place.select();
  await ui.type(file);
  await ui.key("Tab");
  await ui.click([...document.querySelectorAll(".prefs-carry button")].find((one) => one.textContent === "Take…"));
  await ui.waitFor(() => differenceOf("settings.json"), 5000);
  const setting = (key) => differenceOf("settings.json").querySelector(`input[data-key="${key}"]`);
  assert.ok(setting("crumbs") && setting("past-ends"), "each setting that differs is shown");
  assert.ok(!setting("memory-budget"), "and none of this machine's own");
  assert.match(setting("crumbs").closest("label").textContent, /true→false/);
  assert.equal(ownItem("orior.crumbs"), "true", "nothing is written before it is taken");
  await ui.click(setting("past-ends"));
  assert.equal(setting("past-ends").checked, false);
  await ui.click([...document.querySelectorAll(".prefs-carry button")].find((one) => one.textContent === "Take the Chosen"));
  await ui.waitFor(() => ownItem("orior.crumbs") === "false", 5000);
  assert.equal(ownItem("orior.past-ends"), "true", "a setting not chosen stays as it was");
  assert.equal(ownItem("orior.memory-budget"), "400");
  await ui.waitFor(() => document.querySelectorAll(".carry-taking").length >= 1, 5000);
  await ui.click(document.querySelector(".carry-taking button"));
  await ui.waitFor(() => ownItem("orior.crumbs") === "true", 5000);
  document.querySelector("dialog.sheet")?.close();
  localStorage.removeItem("orior.past-ends");
  localStorage.removeItem("orior.memory-budget");
});

const KEEPS = `orior.on("open", async ({ path }) => {
  if (!path.endsWith("kept_count.txt")) {
    return;
  }
  const kept = await orior.settings.read();
  const count = (kept.count ?? 0) + 1;
  await orior.settings.write({ count });
  await orior.tabs.label(path, "count " + count);
});
`;

test("a plugin keeps settings of its own, and they travel with it", async () => {
  const home = await invoke("home_folder");
  const id = "carried-counter";
  await invoke("file_write_any", { path: `${home}/plugins/${id}/plugin.json`, text: JSON.stringify({ id, name: "Carried Counter", version: "1.0.0", kind: "script", api: 2 }) });
  await invoke("file_write_any", { path: `${home}/plugins/${id}/plugin.js`, text: KEEPS });
  await invoke("plugin_settings_write", { id, values: { count: 4 } });
  await loadPlugins();
  await invoke("file_write", { path: "kept_count.txt", text: "x\n" });
  showView("edit");
  await openFile("kept_count.txt");
  await ui.waitFor(() => editing().active === "kept_count.txt", 5000);
  await ui.waitFor(() => [...document.querySelectorAll("#tabs .tab")].some((one) => one.textContent.includes("count 5")), 5000);
  assert.deepEqual(await invoke("plugin_settings_read", { id }), { count: 5 });
  const root = document.getElementById("tree-path").textContent.replace(/[\\/]+$/, "");
  await invoke("carry_send", { to: `${root}/carried_plugin.json` });
  const sent = JSON.parse(await invoke("file_read_any", { path: `${root}/carried_plugin.json` }));
  assert.equal(JSON.parse(sent.files[`plugin_settings/${id}.json`]).count, 5);
  assert.ok(sent.files[`plugins/${id}/plugin.js`].includes("orior.settings.read"), "and the plugin with them");
});
