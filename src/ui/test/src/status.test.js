// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { calm, pressed, status, watch, write } from "../../src/status.js";

const { test, assert, ui } = window.__harness;

test("writing a part sets only the values given", () => {
  const kept = { ...status.colors };
  try {
    write("colors", { ahead: 42 });
    assert.equal(status.colors.ahead, 42);
  } finally {
    write("colors", kept);
  }
});

test("writing a part that is a map sets a key, and null takes it out", () => {
  write("reads", ["harness/a.txt", { read: 1, size: 2 }]);
  assert.deepEqual(status.reads.get("harness/a.txt"), { read: 1, size: 2 });
  write("reads", ["harness/a.txt", null]);
  assert.equal(status.reads.has("harness/a.txt"), false);
});

test("watchers hear once a frame, whatever was written in it, until they stop", async () => {
  let heard = 0;
  const stop = watch(() => (heard += 1));
  try {
    write("colors", { ahead: 1 });
    write("colors", { ahead: 2 });
    write("colors", { ahead: 3 });
    assert.equal(heard, 0, "nothing heard before the frame");
    await ui.frame();
    await ui.frame();
    assert.equal(heard, 1);
  } finally {
    stop();
  }
  write("colors", { ahead: 0 });
  await ui.frame();
  await ui.frame();
  assert.equal(heard, 1, "a stopped watcher hears nothing");
});

test("the view is pressed while it takes input, moves or strains", () => {
  const kept = [status.input.active, status.scroll.level, status.frame.strain];
  try {
    write("input", { active: false });
    write("scroll", { level: 0 });
    write("frame", { strain: 0 });
    assert.equal(pressed(), false);
    write("input", { active: true });
    assert.equal(pressed(), true);
    write("input", { active: false });
    write("scroll", { level: 2 });
    assert.equal(pressed(), true);
    write("scroll", { level: 0 });
    write("frame", { strain: 1 });
    assert.equal(pressed(), true);
  } finally {
    write("input", { active: kept[0] });
    write("scroll", { level: kept[1] });
    write("frame", { strain: kept[2] });
  }
});

test("calm waits while the view is pressed and comes once it is not", async () => {
  write("input", { active: true });
  let came = false;
  const waiting = calm().then(() => (came = true));
  await ui.rest(300);
  assert.equal(came, false, "it waits while pressed");
  write("input", { active: false });
  await waiting;
  assert.equal(came, true);
});
