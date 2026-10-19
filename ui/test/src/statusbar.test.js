// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Notices at the reader's breaks: one that answers a press shows at once, any other waits for a save,
// a commit, a run's end or the keys at rest, and Focus holds every notice until it ends. Not again,
// beside a notice, puts its kind away for good, and Preferences brings it back.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { bringBack, focusOn, kindOf, noticesAway, noticesHeld, noticesSaid, noticesUnseen, say, setFocus } from "../../gui/src-js/statusbar.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const said = () => document.getElementById("status-said");
const shown = (text) => !said().hidden && said().textContent === text;

// Types in the editor, a press longer ago than a notice answers it.
async function typing() {
  await ui.rest(1600);
  await invoke("file_write", { path: "notices.txt", text: "one\n" });
  showView("edit");
  await openFile("notices.txt");
  await ui.waitFor(() => editing().active === "notices.txt", 5000);
  editing().editor.goTo(0, 3);
  editing().editor.focus();
  await ui.type(" two");
}

test("a notice that answers a press shows at once", async () => {
  await ui.click("#statusbar");
  say("Answered at once");
  assert.ok(shown("Answered at once"));
  assert.equal(noticesSaid().at(-1).text, "Answered at once", "the bell keeps it");
});

test("a notice that comes while the reader types waits for the keys to rest, and none takes the keys", async () => {
  await typing();
  const before = noticesSaid().length;
  say("Came while typing");
  assert.ok(!shown("Came while typing"), "not at once");
  assert.equal(noticesSaid().length, before, "the bell has not got it yet");
  assert.deepEqual(noticesHeld().map((one) => one.text), ["Came while typing"]);
  await ui.type(" three");
  await ui.rest(1200);
  assert.ok(!shown("Came while typing"), "the keys have not rested");
  await ui.waitFor(() => !noticesHeld().length && noticesSaid().some((one) => one.text === "Came while typing"), 4000);
  assert.ok(shown(noticesSaid().at(-1).text), "the last held shows on the bar");
  assert.ok(document.activeElement.classList.contains("ed-input"), "the editor keeps the keys");
});

test("a save is a break, and the notices held show at it", async () => {
  await typing();
  say("Held for the save");
  assert.ok(!shown("Held for the save"));
  await invoke("file_write", { path: "notices.txt", text: "saved\n" });
  await ui.waitFor(() => shown("Held for the save"), 500);
});

test("Focus holds every notice until it ends, even at a break, and then shows them", async () => {
  runCommand("focus");
  assert.equal(focusOn(), true);
  await ui.waitFor(() => !document.getElementById("status-focus").hidden, 1000);
  await ui.click("#statusbar");
  say("First while focused");
  say("Second while focused");
  await invoke("file_write", { path: "notices.txt", text: "saved again\n" });
  await ui.rest(2500);
  assert.ok(!shown("First while focused") && !shown("Second while focused"), "a press, a save and the keys at rest all wait");
  const held = noticesHeld().map((one) => one.text);
  assert.ok(held.includes("First while focused") && held.includes("Second while focused"), JSON.stringify(held));
  assert.equal(document.getElementById("status-focus").textContent, `Focus, ${held.length} held`);
  await ui.click("#status-focus");
  assert.equal(focusOn(), false);
  assert.equal(noticesHeld().length, 0);
  assert.ok(shown(held.at(-1)), "the last on the bar");
  const bell = noticesSaid().map((one) => one.text);
  assert.ok(bell.indexOf("First while focused") < bell.indexOf("Second while focused") && bell.indexOf("First while focused") >= 0, "each in the bell, in order");
  assert.equal(document.getElementById("status-focus").hidden, true);
  setFocus(false);
});

test("Not again beside a notice puts its kind away for good: said again, with another count, it shows no more", async () => {
  const kind = kindOf("3 files could not be read");
  bringBack(kind);
  try {
    await ui.click("#statusbar");
    say("3 files could not be read");
    assert.ok(shown("3 files could not be read"));
    await ui.click("#status-said-away");
    assert.ok(said().hidden && document.getElementById("status-said-away").hidden, "the notice and its press go");
    assert.ok(noticesAway().includes(kind), "its kind is put away");
    await ui.click("#statusbar");
    const unseen = noticesUnseen();
    say("7 files could not be read");
    assert.ok(!shown("7 files could not be read"), "the same kind, with another count, shows no more");
    assert.equal(noticesSaid().at(-1).text, "7 files could not be read", "the bell keeps it");
    assert.equal(noticesUnseen(), unseen, "and counts it as nothing unseen");
  } finally {
    bringBack(kind);
  }
});

test("Preferences lists each notice put away and shows its kind again on a press", async () => {
  const kind = kindOf("The tree has 12 plugins to update");
  bringBack(kind);
  await ui.click("#statusbar");
  say("The tree has 12 plugins to update");
  await ui.click("#status-said-away");
  try {
    runCommand("preferences");
    const row = await ui.waitFor(() => [...document.querySelectorAll("dialog[open] .prefs-notice-away")].find((one) => one.textContent.includes(kind)), 5000);
    await ui.click(row.querySelector("button"));
    assert.ok(!noticesAway().includes(kind), "its kind is back");
    document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
    await ui.click("#statusbar");
    say("The tree has 12 plugins to update");
    assert.ok(shown("The tree has 12 plugins to update"), "and shows again");
  } finally {
    bringBack(kind);
    document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  }
});
