// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sheets over the app: Help's sheets, the keys every menu lists, and what is new.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { press, putBackAsFound, sheetShown } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Help, Network and What's New show their sheets", async () => {
  await press("network");
  await sheetShown(".sheet-network");
  await press("whats-new");
  await sheetShown(".sheet-changes");
});

test("Help, Keyboard Shortcuts and About, from the command line and from the menu", async () => {
  await press("keys");
  await press("about");
  await runCommand("keys");
  const keys = await sheetShown(".sheet-keys");
  assert.ok(keys.querySelector("kbd"), "the keys listed");
  await runCommand("about");
  const about = await sheetShown(".sheet-about");
  assert.equal(about.querySelector("p")?.textContent, await invoke("app_version"));
});

test("Help, What's New shows what each release changed, the running version's own among them", async () => {
  const help = [...document.getElementById("menubar").children].find((one) => one.textContent.trim() === "Help");
  await ui.click(help);
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 3000);
  await ui.click([...menu.querySelectorAll(".menu-item")].find((one) => one.querySelector("span")?.textContent === "What's New"));
  const shown = await ui.waitFor(() => document.querySelector("dialog[open] .sheet-changes"), 3000);
  try {
    const version = await invoke("app_version");
    assert.equal(shown.querySelector("h1")?.textContent, "What's new in orior");
    assert.ok([...shown.querySelectorAll("h2")].some((one) => one.textContent === version), `a section for ${version}`);
  } finally {
    document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  }
});

test("the window is put back as the file found it", putBackAsFound);
