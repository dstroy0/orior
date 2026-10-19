// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Bars of progress: steady, never back, and faster toward their end.

import { drawnShare, progressBar, SPEEDS_UP, timeLeft } from "../../gui/src-js/progress.js";
import { write } from "../../gui/src-js/status.js";

const { test, assert, ui } = window.__harness;

test("a bar draws the share done to a power: slower at first and faster toward its end", () => {
  assert.equal(drawnShare(0), 0);
  assert.equal(drawnShare(1), 1);
  assert.ok(SPEEDS_UP > 1);
  assert.ok(Math.abs(drawnShare(0.5) - 0.5 ** SPEEDS_UP) < 1e-9);
  assert.ok(drawnShare(1) - drawnShare(0.5) > drawnShare(0.5) - drawnShare(0), "the second half moves farther than the first");
  assert.equal(drawnShare(-1), 0);
  assert.equal(drawnShare(2), 1);
});

test("the time a piece of work has left is read from its pace so far, and from nothing less than a second of it", () => {
  assert.equal(timeLeft(500, 0.5), null, "under a second gone");
  assert.equal(timeLeft(5000, 0), null, "nothing done");
  assert.equal(timeLeft(5000, 1), null, "all done");
  assert.equal(timeLeft(4000, 0.5), "about 4 s left");
  assert.equal(timeLeft(10000, 0.1), "about 2 min left", "past a minute, in minutes");
  assert.equal(timeLeft(2000, 0.99), "about 1 s left", "never less than a second said");
});

test("a bar moves at one pace toward a new share, and never back", () => {
  const bar = progressBar();
  const done = bar.firstChild;
  assert.match(done.style.transition, /linear/, "one pace between reports");
  bar.set(0.64);
  assert.equal(done.style.transform, `scaleX(${drawnShare(0.64)})`);
  assert.equal(bar.getAttribute("aria-valuenow"), "64");
  bar.set(0.3);
  assert.equal(done.style.transform, `scaleX(${drawnShare(0.64)})`, "a report of less leaves it where it was");
});

test("a file being read shows on the status bar with its bar and its share as it is", async () => {
  const pulse = document.getElementById("pulse");
  // A large file the tree's tabs reopen is read first, and its own share shows until it is read.
  await ui.waitFor(() => !pulse.querySelector(".pulse-read"), 60000);
  try {
    write("reads", ["big/log.txt", { read: 250, size: 1000 }]);
    await ui.waitFor(() => pulse.querySelector(".pulse-read"), 2000);
    assert.match(pulse.textContent, /log\.txt\s+25%/);
    const bar = pulse.querySelector(".progress");
    assert.equal(bar.getAttribute("aria-valuenow"), "25");
    write("reads", ["big/log.txt", { read: 750, size: 1000 }]);
    await ui.waitFor(() => /75%/.test(pulse.textContent), 2000);
    assert.equal(pulse.querySelector(".progress"), bar, "the same bar moves on");
  } finally {
    write("reads", ["big/log.txt", null]);
  }
  await ui.waitFor(() => !pulse.querySelector(".pulse-read"), 2000);
});
