// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the page sets before it draws: each setting retained under orior. written to orior's own folder once changes rest,
// none of those taken from the folder written back, and the scheme, the theme's colors and the fonts in force from the
// first frame.

import { invoke } from "../../gui/src-js/bridge.js";
import { retentionStorage } from "../helpers.js";
import { putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

// A setting of the environment's, which the folder carries, that no part of the window reads.
const RETAINED = "orior.environment.early-test";

// What orior's folder retains of the page's keys once the changes waiting have rested and been written: the settings
// it holds, by key without orior., and those of `keys` it no longer retains.
async function retainedAfterRest(...keys) {
  await ui.rest(800);
  return invoke("retained_now", { keys });
}

test("the scheme, the theme's colors and the fonts are before the page draws", () => {
  assert.ok(["dark", "light"].includes(document.documentElement.dataset.scheme));
  assert.ok(document.head.querySelector("style#theme-colors") && document.head.querySelector("style#fonts"));
  assert.equal(typeof window.oriorKeptTaken, "function");
});

test("a setting retained under orior. is written to orior's folder once changes rest, the last of a rest alone, and its removal", async () => {
  await retentionStorage([RETAINED], async () => {
    localStorage.setItem(RETAINED, "one");
    localStorage.setItem(RETAINED, "two");
    const retained = await retainedAfterRest(RETAINED);
    assert.equal(retained.set["environment.early-test"], "two");
    localStorage.removeItem(RETAINED);
    const gone = await retainedAfterRest(RETAINED);
    assert.ok(!("environment.early-test" in gone.set) && gone.gone.includes("environment.early-test"), "its removal written");
  });
});

test("a setting taken from orior's folder is not written back, and a change of the key waiting is dropped", async () => {
  await retentionStorage([RETAINED], async () => {
    localStorage.setItem(RETAINED, "the page's");
    window.oriorKeptTaken(() => localStorage.setItem(RETAINED, "the folder's"));
    const retained = await retainedAfterRest(RETAINED);
    assert.ok(!("environment.early-test" in retained.set), "nothing older lands over what the folder holds");
    window.oriorKeptTaken(() => localStorage.removeItem(RETAINED));
  });
});

test("the window is put back as the file found it", putBackAsFound);
