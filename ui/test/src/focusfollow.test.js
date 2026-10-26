// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Focus that follows the pointer: kept on or off, and where it is on, the editor takes the keys once the pointer has
// rested on it, and keeps them from no field being typed in.

import { focusFollows, setFocusFollows } from "../../gui/src-js/focusfollow.js";
import { keepingStorage } from "../helpers.js";
import { given, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

const KEY = "orior.focus-follows";

test("focus follows the pointer is off until it is set, and kept", async () => {
  await keepingStorage([KEY], () => {
    localStorage.removeItem(KEY);
    assert.equal(focusFollows(), false);
    setFocusFollows();
    assert.equal(focusFollows(), true);
    setFocusFollows(false);
    assert.equal(focusFollows(), false);
  });
});

test("with it on, the editor takes the keys once the pointer rests on it; with it off, it waits for a press", async () => {
  const view = await given("text|");
  const away = Object.assign(document.createElement("button"), { textContent: "away" });
  away.style.cssText = "position:fixed;right:40px;bottom:40px;z-index:9999";
  document.body.append(away);
  await keepingStorage([KEY], async () => {
    setFocusFollows(true);
    away.focus();
    await ui.hover(away);
    await ui.hover(view.host);
    await ui.waitFor(() => view.host.contains(document.activeElement), 2000, "the editor to take the keys");
    setFocusFollows(false);
    away.focus();
    await ui.hover(away);
    await ui.hover(view.host);
    await ui.rest(400);
    assert.equal(document.activeElement, away, "the keys stay where they were");
  }).finally(() => away.remove());
});

test("the window is put back as the file found it", putBackAsFound);
