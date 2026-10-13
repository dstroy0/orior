// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The quick open: Go to File while the tree's files are still being read, answered from the files
// opened lately, saying so, and asked again until the tree is read.

import { watchCalls } from "../../src/bridge.js";
import { openPalette, paletteOpen } from "../../src/palette.js";

const { test, assert, ui, calls } = window.__harness;

const READING = "Reading the tree's files: more may be found.";
const labels = () => [...document.querySelectorAll("#quick-list .quick-row .quick-label")].map((label) => label.textContent);

test("Go to File answers from the files opened lately while the tree's files are read, says so, and asks again until they are read", async () => {
  let unread = 2;
  let asked = 0;
  watchCalls((name, args, send) => {
    if (name === "files_find") {
      asked += 1;
      if (unread > 0) {
        unread -= 1;
        return Promise.resolve({ found: [{ path: "hello.py", score: 1, hits: [0] }], reading: true });
      }
    }
    return calls.watcher(name, args, send);
  });
  try {
    openPalette("hello");
    await ui.waitFor(() => labels().includes(READING), 3000);
    assert.ok(labels().includes("hello.py"), `the file opened lately answers: ${labels().join(", ")}`);
    await ui.waitFor(() => !labels().includes(READING), 3000);
    assert.ok(labels().includes("hello.py"), `the tree's file answers once it is read: ${labels().join(", ")}`);
    assert.ok(asked >= 3, `asked again while the tree was read: ${asked}`);
  } finally {
    watchCalls(calls.watcher);
    if (paletteOpen()) {
      await ui.key("Escape");
    }
  }
  assert.ok(!paletteOpen(), "Escape closes it");
});
