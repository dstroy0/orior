// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The servers a tab is handed to: a change told once the frame that draws it is drawn, with no wait of
// the window's own, and the keys typed while a change is on its way told together after it.

import { watchCalls } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { showView } from "../../src/views.js";

const { test, assert, ui, calls } = window.__harness;

const wait = (ms) => new Promise((done) => setTimeout(done, ms));

test("a change is told to the tab's server with no wait of the window's own, and keys typed while one is on its way are told together after it", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  const before = view.doc.text();
  const told = [];
  let release = null;
  watchCalls((name, args, send) => {
    if (name === "lsp_change" && args.path.endsWith("hello.py")) {
      told.push({ at: performance.now(), text: args.text });
      if (told.length === 1) {
        return new Promise((done) => (release = () => done(calls.watcher(name, args, send))));
      }
    }
    return calls.watcher(name, args, send);
  });
  try {
    view.goTo(0, 0);
    view.focus();
    await ui.type("a");
    const typed = performance.now();
    await ui.waitFor(() => told.length === 1, 2000);
    assert.ok(told[0].at - typed < 150, `told ${Math.round(told[0].at - typed)} ms after the key`);
    await ui.type("bc");
    await wait(300);
    assert.equal(told.length, 1, "nothing more is told while the change before it is on its way");
    release();
    await ui.waitFor(() => told.length === 2, 2000);
    assert.ok(told[1].text.startsWith("abc"), "the keys typed meanwhile are told together, after it");
  } finally {
    release?.();
    watchCalls(calls.watcher);
    for (let undo = 0; undo < 6 && view.doc.text() !== before; undo += 1) {
      await ui.key("Ctrl+Z");
    }
  }
  assert.equal(view.doc.text(), before, "the file is as it was");
});
