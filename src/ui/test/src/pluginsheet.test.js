// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Plugins and its catalog: a plugin found by name in a catalog the reader names and installed,
// told what it reads before it runs and held to that, an update installed only when the reader says
// so, and one that asks for more waiting to be let run again.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { pluginReadings } from "../../src/plugins.js";
import { running } from "../../src/scripts.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const CODE = `orior.on("open", async ({ path }) => {
  if (!path.endsWith("catalog_a.txt")) {
    return;
  }
  const first = (await orior.files.read(path)).split(" ")[0];
  let other = "read";
  try {
    await orior.files.read("catalog_secret.md");
  } catch (error) {
    other = error.message.includes("did not ask to read") ? "refused" : "failed";
  }
  await orior.tabs.label(path, first + " " + other);
});
`;

// Writes a catalog of one plugin at `version`, its package beside it, asking for `asks`.
async function publish(root, version, asks) {
  const plugin = { id: "catalog-reader", name: "Catalog Reader", version, kind: "script", api: 3, ...asks };
  const files = { "plugin.json": JSON.stringify(plugin), "plugin.js": CODE };
  await invoke("folder_create", { path: "plugin_catalog" }).catch(() => {});
  await invoke("file_write", { path: `plugin_catalog/reader-${version}.json`, text: JSON.stringify({ orior: "plugin", files }) });
  const entry = { id: plugin.id, name: plugin.name, version, about: "Reads the first word of a file.", package: `reader-${version}.json`, ...asks };
  await invoke("file_write", { path: "plugin_catalog/catalog.json", text: JSON.stringify({ orior: "plugin catalog", plugins: [entry] }) });
  return `${root}/plugin_catalog/catalog.json`;
}

const button = (scope, text) => [...document.querySelectorAll(`${scope} button`)].find((one) => one.textContent === text);
const tabNamed = (name) => [...document.querySelectorAll("#tabs .tab")].find((one) => one.textContent.includes(name));

test("a plugin found by name in a catalog is installed as it asks, held to what it asked, and updated only when the reader says so", async () => {
  const root = document.getElementById("tree-path").textContent.replace(/[\\/]+$/, "");
  const address = await publish(root, "1.0.0", { reads: ["catalog_*.txt"] });
  await invoke("file_write", { path: "catalog_a.txt", text: "first words\n" });
  await invoke("file_write", { path: "catalog_secret.md", text: "kept\n" });
  runCommand("plugins");
  await ui.waitFor(() => document.querySelector(".plugins-catalog-address"), 5000);
  const field = document.querySelector(".plugins-catalog-address");
  await ui.click(field);
  field.select();
  await ui.type(address);
  await ui.click(button(".sheet-plugins", "Read the Catalog"));
  await ui.waitFor(() => document.querySelector('.plugin-listed[data-id="catalog-reader"]'), 5000);
  const filter = document.querySelector(".sheet-plugins .filter");
  await ui.click(filter);
  await ui.type("Reader");
  const listed = () => document.querySelector('.plugin-listed[data-id="catalog-reader"]');
  await ui.waitFor(() => listed(), 2000);
  assert.equal(listed().querySelector(".plugin-asks").textContent, "reads catalog_*.txt", "what it asks for, before it is installed");
  await ui.click(button('.plugin-listed[data-id="catalog-reader"]', "Install"));
  await ui.waitFor(() => running().has("catalog-reader"), 5000);
  document.querySelector("dialog.sheet")?.close();
  showView("edit");
  await openFile("catalog_a.txt");
  await ui.waitFor(() => editing().active === "catalog_a.txt", 5000);
  await ui.waitFor(() => tabNamed("catalog_a.txt")?.querySelector(".tab-label")?.textContent === "first refused", 5000);

  await publish(root, "2.0.0", { reads: ["catalog_*.txt"], writes: ["notes/**"] });
  runCommand("plugins");
  await ui.waitFor(() => document.querySelector(".plugins-catalog-address")?.value === address, 5000);
  await ui.click(button(".sheet-plugins", "Read the Catalog"));
  await ui.waitFor(() => button('.plugin-listed[data-id="catalog-reader"]', "Update to 2.0.0"), 5000);
  assert.equal(pluginReadings().find((one) => one.id === "catalog-reader").version, "1.0.0", "nothing is updated unasked");
  await ui.click(button('.plugin-listed[data-id="catalog-reader"]', "Update to 2.0.0"));
  await ui.waitFor(() => pluginReadings().find((one) => one.id === "catalog-reader").version === "2.0.0", 5000);
  await ui.waitFor(() => !running().has("catalog-reader"), 5000);
  const waits = () => document.querySelector('.plugins-list .plugin-row[data-id="catalog-reader"] .plugin-waits');
  await ui.waitFor(() => waits(), 5000);
  assert.match(waits().textContent, /it writes notes\/\*\*/, "an update that asks for more asks the reader again");
  await ui.click(button('.plugins-list .plugin-row[data-id="catalog-reader"]', "Let It Run"));
  await ui.waitFor(() => running().has("catalog-reader"), 5000);
  document.querySelector("dialog.sheet")?.close();
});
