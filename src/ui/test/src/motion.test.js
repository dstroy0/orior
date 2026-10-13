// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { clock, frameMs, idle, judge, preempt, still, whenMoving } from "../../src/motion.js";

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

// A box of the page with a transition of `property` over `ms`, moved from `from` to `to` once the
// page has a style for it at `from`; gives the box and its transition, before the page's own judging.
async function transition(property, ms, from, to) {
  const box = document.body.appendChild(document.createElement("div"));
  Object.assign(box.style, { position: "fixed", left: "-400px", top: "0", width: "40px", height: "40px", background: "rgb(0, 0, 0)", [property]: from });
  getComputedStyle(box)[property];
  box.style.transition = `${property} ${ms}ms ease`;
  box.style[property] = to;
  const drawn = getComputedStyle(box)[property];
  const animation = box.getAnimations().find((one) => one.transitionProperty === property);
  if (!animation) {
    throw new Error(`no transition of ${property} from ${from} to ${to}, drawn as ${drawn}: ${box.getAnimations().map((one) => one.transitionProperty)}`);
  }
  return { box, animation };
}

// One device pixel in CSS pixels.
const pixel = () => 1 / (window.devicePixelRatio || 1);

test("a move of less than a device pixel is not animated: its end is drawn at once", async () => {
  const { box, animation } = await transition("transform", 300, "translateX(0px)", `translateX(${pixel() * 0.4}px)`);
  try {
    assert.ok(animation, "the page made a transition");
    assert.equal(judge(animation), "ended");
    assert.equal(animation.playState, "finished");
  } finally {
    box.remove();
  }
});

test("a transition shorter than two frames is not animated", async () => {
  const { box, animation } = await transition("transform", Math.floor(frameMs()), "translateX(0px)", "translateX(200px)");
  try {
    assert.equal(judge(animation), "ended");
  } finally {
    box.remove();
  }
});

test("a move of fewer pixels than frames is drawn a whole pixel at a time", async () => {
  const { box, animation } = await transition("transform", 600, "translateX(0px)", `translateX(${pixel() * 3}px)`);
  try {
    assert.equal(judge(animation), "stepped");
    assert.match(animation.effect.getComputedTiming().easing, /^steps\(3/);
  } finally {
    box.remove();
  }
});

test("a move the eye follows runs as it is written", async () => {
  const { box, animation } = await transition("transform", 300, "translateX(0px)", "translateX(300px)");
  try {
    assert.equal(judge(animation), "kept");
    assert.equal(animation.effect.getComputedTiming().easing, "ease");
  } finally {
    box.remove();
  }
});

test("a fade or a color is measured in levels: too faint a change ends at once, a whole fade runs", async () => {
  const faint = await transition("opacity", 300, "1", "0.999");
  const whole = await transition("opacity", 300, "1", "0");
  const tint = await transition("background-color", 300, "rgb(0, 0, 0)", "rgb(0, 0, 1)");
  try {
    assert.equal(judge(faint.animation), "ended");
    assert.equal(judge(whole.animation), "kept");
    assert.equal(judge(tint.animation), "stepped", "one level over many frames is drawn once");
  } finally {
    [faint, whole, tint].forEach((one) => one.box.remove());
  }
});

test("the page ends an unseen transition by itself as it starts", async () => {
  const { box, animation } = await transition("transform", 400, "translateX(0px)", `translateX(${pixel() * 0.3}px)`);
  try {
    await ui.waitFor(() => animation.playState === "finished", 200);
  } finally {
    box.remove();
  }
});

test("a twisty's turn, on a pseudo-element, is judged as it starts", async () => {
  const rule = document.head.appendChild(document.createElement("style"));
  rule.textContent = ".motion-twisty::before { content: ''; display: inline-block; width: 6px; height: 6px; transform: rotate(0deg); transition: transform 300ms ease; } .motion-twisty.open::before { transform: rotate(90deg); }";
  const box = document.body.appendChild(document.createElement("span"));
  box.className = "motion-twisty";
  Object.assign(box.style, { position: "fixed", left: "-400px", top: "0" });
  getComputedStyle(box, "::before").transform;
  try {
    box.classList.add("open");
    getComputedStyle(box, "::before").transform;
    const turning = () => box.getAnimations({ subtree: true }).find((one) => one.effect.pseudoElement === "::before");
    assert.ok(turning(), "the pseudo-element turns");
    await ui.waitFor(() => /^steps/.test(turning()?.effect.getComputedTiming().easing ?? ""), 200);
  } finally {
    box.remove();
    rule.remove();
  }
});
