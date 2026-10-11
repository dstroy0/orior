// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Whether the app's animation may run. It stops while the page is hidden and starts again when the
// page shows. A window without focus goes on animating, and so does one being moved. A loop asks
// `still()` before each frame; while it is still the loop ends, and `whenMoving` starts it again once
// motion comes back. The page's own animations pause with the root's `still` class.
//
// Animation keeps its time on `clock()`, which stands still while motion is stopped: an animation
// that comes back takes up where it stood, and none of the stopped time passes in it.
//
// The lowest work, the lattices' motion, runs only in a frame nothing else wants: `idle()` says so
// once no key, press, pointer move, wheel or scroll has come for YIELD, no animation that ends is
// running, the last frame came within its time, and nothing has asked for the frames by `preempt`.
// Anything else the window does goes first, and the lowest work skips its frames until it is done.

// How long the lowest work waits after the reader's last action, in milliseconds.
const YIELD = 250;

const state = { hidden: document.hidden, waiting: new Set(), lost: 0, stoppedAt: null, acted: 0, heldUntil: 0, lastFrame: 0, frame: 1000 / 60 };

for (const kind of ["keydown", "pointerdown", "pointermove", "wheel", "scroll"]) {
  document.addEventListener(kind, () => (state.acted = performance.now()), { capture: true, passive: true });
}

// Takes the next `ms` of frames from the lowest work, for motion the page drives itself.
export function preempt(ms) {
  state.heldUntil = Math.max(state.heldUntil, performance.now() + ms);
}

// Whether the frame at `now` is free for the lowest work: asked once a frame by that work's loop.
export function idle(now) {
  const gap = now - state.lastFrame;
  state.lastFrame = now;
  // The display's frame: the shortest gap lately, which grows back slowly on a slower display.
  if (gap > 0 && gap < 100) {
    state.frame = Math.min(gap, state.frame * 1.01);
  }
  if (now - state.acted < YIELD || now < state.heldUntil || gap > state.frame * 2.5) {
    return false;
  }
  return !document.getAnimations().some((one) => one.playState === "running" && one.effect?.getComputedTiming?.().iterations !== Infinity);
}

export const still = () => state.hidden;

// The animation's time in milliseconds: `now`, on the page's clock, less all the time motion has
// stood still, and while it stands still, the moment it stopped.
export const clock = (now = performance.now()) => (state.stoppedAt ?? now) - state.lost;

// Runs `start` once, as soon as motion comes back.
export function whenMoving(start) {
  state.waiting.add(start);
}

function settle() {
  document.documentElement.classList.toggle("still", still());
  if (still() && state.stoppedAt === null) {
    state.stoppedAt = performance.now();
  } else if (!still() && state.stoppedAt !== null) {
    state.lost += performance.now() - state.stoppedAt;
    state.stoppedAt = null;
  }
  if (!still()) {
    const starts = [...state.waiting];
    state.waiting.clear();
    starts.forEach((start) => start());
  }
}

export async function startMotion() {
  document.addEventListener("visibilitychange", () => {
    state.hidden = document.hidden;
    settle();
  });
  settle();
}
