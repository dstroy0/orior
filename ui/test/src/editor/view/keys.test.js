// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's keys: a key the reader binds is told the editor it was pressed in, and a key offered first to a
// command is the command's where it takes it and the editor's own where it declines.

import { given, marked, putBackAsFound } from "../../../menus.js";

const { test, assert } = window.__harness;

const press = (view, key, code = key) => {
  const event = { key, code, ctrlKey: false, metaKey: false, altKey: false, shiftKey: false, prevented: false };
  event.preventDefault = () => {
    event.prevented = true;
  };
  view.onKey(event);
  return event;
};

// Runs `work` with the editor's key tables as they stood put back after it.
async function keepingKeys(view, work) {
  const kept = { keys: view.keys, first: view.first, native: view.native, bound: view.bound };
  try {
    return await work();
  } finally {
    Object.assign(view, kept);
  }
}

test("a key the reader binds runs with the editor it was pressed in, and the press goes no further", async () => {
  const view = await given("x|");
  await keepingKeys(view, () => {
    let told = null;
    view.bindKeys([{ keys: "F9", run: (editor) => (told = editor) }]);
    const event = press(view, "F9");
    assert.equal(told, view);
    assert.ok(event.prevented);
  });
});

test("a key offered first is the command's where it takes it, and the editor's own where it declines", async () => {
  const view = await given("ab|c");
  await keepingKeys(view, () => {
    let takes = true;
    view.addKeys([{ keys: "Left", first: true, run: () => takes }]);
    press(view, "ArrowLeft");
    assert.equal(marked(), "ab|c", "the command took it");
    takes = false;
    press(view, "ArrowLeft");
    assert.equal(marked(), "a|bc", "the editor moved left");
  });
});

test("the window is put back as the file found it", putBackAsFound);
