// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { clock, idle, preempt, still, whenMoving } from "../../src/motion.js";

const { test, assert, ui } = window.__harness;

// Whether the lowest work may run, asked in each frame for `ms`: the answers, in order.
async function idleOver(ms) {
  const answers = [];
  const until = performance.now() + ms;
  while (performance.now() < until) {
    answers.push(idle(await new Promise((next) => requestAnimationFrame(next))));
  }
  return answers;
}

test("the lowest work waits after a key is pressed", async () => {
  await ui.rest(400);
  await ui.key("Shift");
  const answers = await idleOver(150);
  assert.ok(answers.length > 0, "frames came");
  assert.ok(answers.every((answer) => !answer), `no frame was free right after the key: ${answers}`);
});

test("the lowest work waits after the pointer moves", async () => {
  await ui.rest(400);
  await ui.hover({ x: 40, y: 200 });
  assert.ok((await idleOver(150)).every((answer) => !answer), "no frame was free right after the move");
});

test("the lowest work runs again once nothing has come for a while", async () => {
  await ui.key("Shift");
  await ui.rest(400);
  const answers = await idleOver(300);
  assert.ok(answers.some((answer) => answer), `a frame was free 400 ms after the key: ${answers}`);
});

test("preempting takes the frames for the time asked", async () => {
  await ui.rest(400);
  preempt(250);
  assert.ok((await idleOver(200)).every((answer) => !answer), "no frame was free while preempted");
  await ui.rest(100);
  assert.ok((await idleOver(300)).some((answer) => answer), "frames were free once the time passed");
});

test("a running animation that ends takes the frames from the lowest work", async () => {
  await ui.rest(400);
  const box = document.body.appendChild(document.createElement("div"));
  const moving = box.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 300 });
  try {
    assert.ok((await idleOver(150)).every((answer) => !answer), "no frame was free while it ran");
  } finally {
    moving.cancel();
    box.remove();
  }
});

test("an animation that loops without end leaves the frames free", async () => {
  await ui.rest(400);
  const box = document.body.appendChild(document.createElement("div"));
  const looping = box.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 300, iterations: Infinity });
  try {
    assert.ok((await idleOver(300)).some((answer) => answer), "a frame was free beside the loop");
  } finally {
    looping.cancel();
    box.remove();
  }
});

test("the page shown, motion runs and its clock goes on", async () => {
  assert.equal(still(), false);
  const before = clock();
  await ui.rest(50);
  assert.ok(clock() > before, "the clock went on");
  assert.ok(!document.documentElement.classList.contains("still"));
});

// The page hidden and shown again, as the system hides it when the window is minimized.
function setHidden(hidden) {
  Object.defineProperty(document, "hidden", { configurable: true, get: () => hidden });
  document.dispatchEvent(new Event("visibilitychange"));
}

test("the page hidden, motion stops, its clock stands, and a waiting start runs once it shows", async () => {
  let started = 0;
  try {
    setHidden(true);
    assert.equal(still(), true);
    assert.ok(document.documentElement.classList.contains("still"), "the root has the still class");
    const stood = clock();
    whenMoving(() => (started += 1));
    await ui.rest(80);
    assert.equal(clock(), stood, "the clock stands while hidden");
    assert.equal(started, 0, "nothing starts while hidden");
  } finally {
    setHidden(false);
    delete document.hidden;
  }
  assert.equal(still(), false);
  assert.equal(started, 1, "the waiting start ran once");
  const after = clock();
  await ui.rest(30);
  assert.ok(clock() > after, "the clock goes on");
});
