// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Macros: what is typed in the editor recorded and played back once or many times, the last one kept under a name,
// the kept ones in order with keys of their own, and each change told.

import { forgetMacro, keepMacro, keptMacros, lastMacro, onMacros, playMacro, recording, setMacroKeys, toggleRecording } from "../../gui/src-js/macros.js";
import { keepingStorage } from "../helpers.js";
import { given, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

const KEPT = "orior.macros";

test("what is typed between two presses of record is the last macro, and plays back as many times as asked", async () => {
  const view = await given("|");
  assert.equal(toggleRecording(view), null);
  assert.ok(recording());
  view.focus();
  await ui.type("ab");
  await ui.waitFor(() => view.doc.text() === "ab", 3000, "the letters typed");
  const steps = toggleRecording(view);
  assert.ok(!recording());
  assert.ok(steps.length > 0);
  assert.equal(lastMacro(), steps);
  assert.equal(view.doc.text(), "ab");
  assert.equal(await playMacro(view, steps, 3), true);
  assert.equal(view.doc.text(), "abababab");
  assert.equal(await playMacro(view, [], 1), false, "nothing to play");
});

test("macros kept by name stay in order, a name kept again takes the new steps, and keys go to one macro alone", async () => {
  await keepingStorage([KEPT], () => {
    localStorage.removeItem(KEPT);
    const told = [];
    onMacros(() => told.push(keptMacros().length));
    keepMacro("zeta", [{ text: "z" }]);
    keepMacro("alpha", [{ text: "a" }]);
    assert.deepEqual(keptMacros().map((one) => one.name), ["alpha", "zeta"]);
    keepMacro("zeta", [{ text: "Z" }]);
    assert.deepEqual(keptMacros().find((one) => one.name === "zeta").steps, [{ text: "Z" }]);
    setMacroKeys("alpha", "Ctrl+Alt+1");
    setMacroKeys("zeta", "Ctrl+Alt+1");
    assert.deepEqual(keptMacros().map((one) => one.keys), ["", "Ctrl+Alt+1"], "the keys moved to the last given them");
    forgetMacro("alpha");
    assert.deepEqual(keptMacros().map((one) => one.name), ["zeta"]);
    assert.ok(told.length >= 5, "each change told");
  });
});

test("a kept list that cannot be read, or that holds what is no macro, is read as what macros it holds", async () => {
  await keepingStorage([KEPT], () => {
    localStorage.setItem(KEPT, "{not json");
    assert.deepEqual(keptMacros(), []);
    localStorage.setItem(KEPT, JSON.stringify([{ name: "ok", steps: [] }, { name: 3 }, null, { name: "no steps" }]));
    assert.deepEqual(keptMacros().map((one) => one.name), ["ok"]);
  });
});

test("the window is put back as the file found it", putBackAsFound);
