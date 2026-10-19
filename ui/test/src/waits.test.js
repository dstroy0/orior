// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The three limits of a response: a press's call late past a tenth of a second, a second and ten.

import { invoke } from "../../gui/src-js/bridge.js";
import { ANSWER, progressOf, tick, watch, watches, WORKING } from "../../gui/src-js/waits.js";

const { test, assert, ui } = window.__harness;

const node = () => document.getElementById("status-wait");
// The element under the pointer that shows the window at work, where one does.
const marked = () => [...document.querySelectorAll("[style]")].find((one) => one.style.getPropertyValue("cursor") === "progress") ?? null;
const waiting = () => marked() !== null;

// A call that never answers until it is let go, and how its wait was stopped.
function hanging() {
  let answer;
  let stopped = null;
  const promise = new Promise((done) => (answer = done));
  return { promise, answer: () => answer(), stop: (why) => (stopped = why), stopped: () => stopped };
}

test("a press's call answered at once shows nothing", async () => {
  await ui.click("#statusbar");
  await invoke("app_version");
  assert.equal(waiting(), false);
  assert.equal(node().hidden, true);
});

test("past a tenth of a second the pointer says the window is at work, and past a second the bar says what it waits on and offers Stop", async () => {
  const call = hanging();
  const now = performance.now();
  watch("tree_search", { query: "x" }, call.promise, call.stop, now - ANSWER - 10);
  try {
    tick(now);
    assert.equal(waiting(), true, "the pointer shows work");
    assert.equal(getComputedStyle(marked()).cursor, "progress", "on the element under the pointer");
    assert.equal(document.querySelectorAll("[style*='progress']").length, 1, "and on that element alone, not every element of the page");
    assert.equal(node().hidden, true, "the bar says nothing yet");
    tick(now + WORKING);
    assert.equal(node().hidden, false);
    assert.match(node().textContent, /^Waiting on tree search: 1\.\d sStop$/);
    assert.ok(node().querySelector("button.status-wait-stop"), "Stop offered as the wait is told");
  } finally {
    call.answer();
  }
  await ui.waitFor(() => !waiting() && node().hidden, 1000);
});

test("a wait whose work tells its whole shows how far along it is and the time it has left, in words and a bar", async () => {
  const call = hanging();
  watch("db_export", { name: "x" }, call.promise, call.stop, performance.now() - 10500);
  try {
    progressOf("db_export", { done: 300, of: 1200, unit: "rows written" });
    assert.match(node().textContent, /Waiting on db export: 300 of 1200 rows written, 1\d s, about 3\d s left/);
    assert.equal(node().querySelector(".progress").getAttribute("aria-valuenow"), "25");
  } finally {
    call.answer();
  }
  await ui.waitFor(() => node().hidden, 1000);
});

test("the bar's Stop stops the wait, one button as the words beside it are drawn again", async () => {
  const call = hanging();
  watch("tree_search", { query: "x" }, call.promise, call.stop, performance.now() - 10500);
  try {
    tick();
    const stop = node().querySelector("button");
    assert.ok(stop, "Stop is offered");
    tick(performance.now() + 1000);
    assert.equal(node().querySelector("button"), stop, "the same Stop as the words are drawn again");
    assert.match(node().textContent, /Waiting on tree search: 1\d s/);
    await ui.click(stop);
    assert.equal(call.stopped(), "stopped waiting");
    await ui.waitFor(() => node().hidden && !waiting(), 1000);
  } finally {
    call.answer();
  }
});

test("a call the window makes on its own as the reader types on, a LaTeX file's make, is not watched", async () => {
  await ui.click("#statusbar");
  assert.equal(watches("tree_search"), true, "a press's call is watched");
  assert.equal(watches("latex_make"), false);
});
