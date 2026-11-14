// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The View, Terminal, Help and Go menus and Edit's find, each command run as its menu runs it and as a command line
// runs it, and what it does looked for.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { editing } from "../../../gui/src-js/edit.js";
import { paletteOpen } from "../../../gui/src-js/palette.js";
import { focusOn } from "../../../gui/src-js/statusbar.js";
import { previewMode } from "../../../gui/src-js/preview.js";
import { scheme } from "../../../gui/src-js/scheme.js";
import { zoom } from "../../../gui/src-js/zoom.js";
import { boardShown, tasks } from "../../../gui/src-js/gather.js";
import { calls, GIVEN, given, made, marked, PANELS, press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("each panel of View opens and closes from the command line", async () => {
  for (const [command, id] of Object.entries(PANELS)) {
    const panel = document.getElementById(id);
    const was = panel.hidden;
    await press(command);
    await ui.waitFor(() => panel.hidden !== was, 3000, `${command} to ${was ? "open" : "close"} #${id}`);
    await press(command);
    await ui.waitFor(() => panel.hidden === was, 3000, `${command} to put #${id} back`);
  }
});

test("View, Edit and View, Run show their views", async () => {
  await press("run-view");
  assert.equal(document.body.dataset.view, "run");
  await press("edit-view");
  assert.equal(document.body.dataset.view, "edit");
});

test("View, Command Palette opens the quick open on commands, with the words given", async () => {
  await press("palette", "zoom");
  try {
    await ui.waitFor(() => paletteOpen(), 3000, "the quick open");
    assert.equal(document.querySelector(".quick-input").value, ">zoom");
  } finally {
    await ui.key("Escape");
  }
});

test("View, Focus holds the notices and lets them go", async () => {
  const was = focusOn();
  await press("focus");
  assert.equal(focusOn(), !was);
  await press("focus");
  assert.equal(focusOn(), was);
});

test("View, Preview shows the preview beside and alone, and off", async () => {
  const was = previewMode();
  try {
    await press("preview", "beside");
    assert.equal(previewMode(), "beside");
    await press("preview", "off");
    assert.equal(previewMode(), "off");
    await press("preview-alone");
    assert.equal(previewMode(), "alone");
    await press("preview-alone");
    assert.equal(previewMode(), "off");
  } finally {
    await runCommand("preview", [was]);
  }
});

test("View, Open Tool Window in a Window opens one showing the tool alone, and Show Only shows it here alone", async () => {
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args.words));
  await press("float", "terminal");
  assert.deepEqual(opened.at(-1)?.slice(0, 3), ["view", "solo", "terminal"], "a window opened to show the terminal alone");
  await press("solo", "explorer");
  assert.equal(document.body.dataset.solo, "explorer");
  await press("solo");
  assert.equal(document.body.dataset.solo, undefined, "everything displayed again");
});

test("View, Move to Next Display asks the window to move", async () => {
  calls.answer("window_act", () => false);
  await press("next-display");
  assert.ok(made("window_act").some((one) => one.args.act === "next-display"));
});

test("View, Scheme turns the window light and dark", async () => {
  const was = scheme();
  try {
    await press("scheme", "light");
    assert.equal(scheme(), "light");
    await press("scheme", "dark");
    assert.equal(scheme(), "dark");
  } finally {
    await runCommand("scheme", [was]);
  }
});

test("View, Zoom In, Zoom Out and Reset Zoom", async () => {
  await press("zoom-reset");
  assert.equal(zoom(), 1);
  await press("zoom-in");
  assert.ok(zoom() > 1, `zoomed in to ${zoom()}`);
  await press("zoom-out");
  assert.equal(zoom(), 1);
  await press("zoom-out");
  assert.ok(zoom() < 1, `zoomed out to ${zoom()}`);
  await press("zoom-reset");
  assert.equal(zoom(), 1);
});

test("Go, Search Everywhere, Recent Files, Symbol and Symbol in the Tree open the quick open as each asks", async () => {
  await given("a|b");
  const input = () => document.querySelector(".quick-input");
  const quick = () => document.querySelector(".quick");
  for (const [command, words, value, everywhere] of [["search-everywhere", [], "", true], ["recent-files", [], "", false], ["symbol", ["greet"], "@greet", false], ["symbol-tree", ["greet"], "#greet", false]]) {
    await press(command, ...words);
    try {
      await ui.waitFor(() => paletteOpen(), 3000, `${command}'s quick open`);
      assert.equal(input().value, value, command);
      assert.equal(quick().classList.contains("everywhere"), everywhere, `${command} everywhere`);
    } finally {
      await ui.key("Escape");
    }
  }
});

test("Go, Jump marks the places to jump to", async () => {
  await given("ab cd ef|");
  await press("jump");
  assert.equal(editing().editor.host.dataset.jump, "true");
  editing().editor.endJump();
});

test("Go, Gather Lines gathers the cursor's line and Gathered Lines shows and hides them", async () => {
  await given("ab\nc|d");
  await press("gather-lines");
  const retained = tasks();
  assert.ok((retained.tasks[retained.current] ?? []).some((piece) => piece.path === GIVEN && piece.from === 1), JSON.stringify(retained));
  const was = Boolean(boardShown());
  await press("gathered-lines");
  assert.equal(Boolean(boardShown()), !was);
  await press("gathered-lines");
  assert.equal(Boolean(boardShown()), was);
});

test("Edit, Find and Replace open the find bar, and Next Match and Previous Match step through its matches", async () => {
  const view = await given("[xy] ab xy");
  await press("find");
  assert.ok(view.find.displayed, "the find bar open");
  assert.equal(view.find.query.value, "xy");
  assert.ok(view.find.replaceRow.hidden, "without the replace row");
  await ui.waitFor(() => view.find.matches.length === 2, 3000, "both matches found");
  await press("next-match");
  assert.equal(marked(), "xy ab [xy]");
  await press("previous-match");
  assert.equal(marked(), "[xy] ab xy");
  await press("replace");
  assert.ok(!view.find.replaceRow.hidden, "the replace row displayed");
  view.find.close();
});

test("the window is put back as the file found it", putBackAsFound);
