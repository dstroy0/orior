// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Whether the app's animation may run. It stops while the page is hidden and starts again when the
// page shows. A window without focus goes on animating, and so does one being moved. A loop asks
// `still()` before each frame; while it is still the loop ends, and `whenMoving` starts it again once
// motion comes back. The page's own animations pause with the root's `still` class.
//
// Animation keeps its time on `clock()`, which stands still while motion is stopped: an animation
// that comes back takes up where it stood, and none of the stopped time passes in it.

const state = { hidden: document.hidden, waiting: new Set(), lost: 0, stoppedAt: null };

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
