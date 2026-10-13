// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A bar of progress, where the work tells its size. It moves steadily toward the share done,
// at one pace between one report and the next and never back, and faster toward its end: the share
// it draws is the share done to the power SPEEDS_UP. A bar that slows or stops near its end reads as
// a longer wait than the same time spent steadily, and one that speeds up toward its end as a
// shorter one. The words beside it say the share done as it is.

export const SPEEDS_UP = 1.5;

// How long the bar takes to reach a new share, in milliseconds, at one pace.
const STEADY = 250;

// The share a bar draws for `share` done, from 0 to 1.
export const drawnShare = (share) => Math.min(1, Math.max(0, share)) ** SPEEDS_UP;

// The time a piece of work has left, as its pace so far gives it, `ms` gone for `share` done: none
// before a second has gone or anything is done, a pace read from less saying little.
export function timeLeft(ms, share) {
  if (ms < 1000 || !(share > 0) || share >= 1) {
    return null;
  }
  const left = (ms * (1 - share)) / share / 1000;
  return left < 60 ? `about ${Math.max(1, Math.round(left))} s left` : `about ${Math.round(left / 60)} min left`;
}

// A bar, empty, with `set(share)` to move it toward a share done.
export function progressBar() {
  const done = Object.assign(document.createElement("span"), { className: "progress-done" });
  const bar = Object.assign(document.createElement("span"), { className: "progress" });
  bar.setAttribute("role", "progressbar");
  bar.setAttribute("aria-valuemin", "0");
  bar.setAttribute("aria-valuemax", "100");
  bar.append(done);
  done.style.transition = `transform ${STEADY}ms linear`;
  let shown = 0;
  bar.set = (share) => {
    shown = Math.max(shown, Math.min(1, Math.max(0, share)));
    bar.setAttribute("aria-valuenow", String(Math.round(shown * 100)));
    done.style.transform = `scaleX(${drawnShare(shown)})`;
  };
  bar.set(0);
  return bar;
}
