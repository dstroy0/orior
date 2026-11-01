// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The language server's places: definitions, the peeks, usages and the call hierarchy.

import { editing } from "../../../gui/src-js/edit.js";
import { noticesWaiting, noticesSaid } from "../../../gui/src-js/statusbar.js";
import { closePeek, peekShown } from "../../../gui/src-js/peek.js";
import { made, PLUGINS_OFF, press, putBackAsFound, served } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("Go, Definition, the peeks, Find Usages and Call Hierarchy ask the language server, here clangd", async () => {
  await served();
  await ui.waitFor(async () => {
    editing().editor.goTo(3, 10);
    await press("peek-definition");
    return peekShown()?.places.length > 0;
  }, 30000, "clangd to find the definition").catch((error) => {
    throw new Error(`${error.message}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesWaiting()].slice(-4).map((one) => one.text))}; calls: ${JSON.stringify(made("lsp_definition").slice(-2))}; the language: ${editing().editor.s.language?.name}; plugins off: ${localStorage.getItem(PLUGINS_OFF)}`);
  });
  assert.equal(peekShown().title, "Definition of helper");
  closePeek();
  editing().editor.goTo(3, 10);
  await press("peek-usages");
  await ui.waitFor(() => peekShown()?.places.length >= 2, 10000, "the usages");
  closePeek();
  editing().editor.goTo(0, 6);
  await press("peek-callers");
  await ui.waitFor(() => peekShown()?.title === "Callers of helper", 10000, "the callers");
  closePeek();
  editing().editor.goTo(3, 10);
  await press("definition");
  await ui.waitFor(() => editing().editor.head().line === 0, 10000, "the cursor at the definition");
  editing().editor.goTo(3, 10);
  await press("usages");
  await ui.waitFor(() => document.getElementById("usages-hits").children.length >= 2, 10000, "the usages listed");
  editing().editor.goTo(0, 6);
  await press("calls");
  await ui.waitFor(() => /helper/.test(document.getElementById("calls-said").textContent), 10000, "the call hierarchy of helper");
}, { timeout: 90000 });

test("the window is put back as the file found it", putBackAsFound);
