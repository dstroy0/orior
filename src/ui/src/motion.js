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
//
// What no eye can see is not animated. As a transition starts, its whole change is measured in the
// display's own units: device pixels for a move or a size, levels of 255 for an opacity or a color.
// One that changes nothing by a whole unit, or that would end within SEEN frames, ends at once, its
// end drawn in the frame it began. One that changes less than a unit in each of its frames is drawn
// in whole units: a frame shows it only where something moves one, and the frames between show
// nothing new. A change this does not measure runs as it is written.

// How long the lowest work waits after the reader's last action, in milliseconds.
const YIELD = 250;

// The fewest frames an animation lasts to be seen.
const SEEN = 2;

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

// The display's frame, in milliseconds.
export const frameMs = () => state.frame;

// The farthest a corner of `element`, or of its pseudo-element `pseudo`, moves between the
// transforms `from` and `to`, in CSS pixels, or null where a transform is not one a matrix reads, as
// one in percents.
function moved(element, pseudo, from, to) {
  let start;
  let end;
  try {
    start = from === "none" ? new DOMMatrix() : new DOMMatrix(from);
    end = to === "none" ? new DOMMatrix() : new DOMMatrix(to);
  } catch {
    return null;
  }
  const style = getComputedStyle(element, pseudo);
  const width = pseudo ? parseFloat(style.width) || 0 : element.offsetWidth ?? element.getBoundingClientRect().width;
  const height = pseudo ? parseFloat(style.height) || 0 : element.offsetHeight ?? element.getBoundingClientRect().height;
  const [originX, originY] = style.transformOrigin.split(" ").map(parseFloat);
  let most = 0;
  for (const [x, y] of [[0, 0], [width, 0], [0, height], [width, height]]) {
    const corner = new DOMPoint(x - originX, y - originY);
    const was = start.transformPoint(corner);
    const is = end.transformPoint(corner);
    most = Math.max(most, Math.hypot(is.x - was.x, is.y - was.y));
  }
  return most;
}

// The most a channel of a color changes between `from` and `to`, as the page writes colors,
// rgb() or rgba(), in levels of 255, or null where either is another form.
function recolored(from, to) {
  const channels = (text) => text.match(/^rgba?\(([^)]*)\)$/)?.[1].split(/[,\s/]+/).filter(Boolean).map(Number);
  const start = channels(from);
  const end = channels(to);
  if (!start || !end || start.some(Number.isNaN) || end.some(Number.isNaN)) {
    return null;
  }
  const alpha = (list) => (list.length > 3 ? list[3] * 255 : 255);
  return Math.max(...[0, 1, 2].map((at) => Math.abs(end[at] - start[at])), Math.abs(alpha(end) - alpha(start)));
}

// The change `animation`, a transition, makes in the display's units, or null where it is a change
// this does not measure.
export function changeOf(animation) {
  const keyframes = animation.effect?.getKeyframes?.() ?? [];
  const property = animation.transitionProperty;
  if (keyframes.length < 2 || !property) {
    return null;
  }
  const key = property.replace(/-([a-z])/g, (_, letter) => letter.toUpperCase());
  const from = keyframes[0][key];
  const to = keyframes[keyframes.length - 1][key];
  if (from === undefined || to === undefined) {
    return null;
  }
  const scale = window.devicePixelRatio || 1;
  if (property === "opacity") {
    return Math.abs(Number(to) - Number(from)) * 255;
  }
  if (property === "transform") {
    const most = moved(animation.effect.target, animation.effect.pseudoElement, from, to);
    return most === null ? null : most * scale;
  }
  if (property.endsWith("color")) {
    return recolored(from, to);
  }
  if (/^-?[\d.]+px$/.test(from) && /^-?[\d.]+px$/.test(to)) {
    return Math.abs(parseFloat(to) - parseFloat(from)) * scale;
  }
  return null;
}

// Ends, steps or keeps the transition `animation` as what can be seen of it asks, and says which:
// "ended", "stepped" or "kept".
export function judge(animation) {
  const frames = animation.effect.getComputedTiming().duration / state.frame;
  const change = changeOf(animation);
  if (frames < SEEN || (change !== null && change < 1)) {
    animation.finish();
    return "ended";
  }
  if (change !== null && change < frames) {
    animation.effect.updateTiming({ easing: `steps(${Math.ceil(change)}, jump-end)` });
    return "stepped";
  }
  return "kept";
}

// Judges the transition `event` says has begun, of an element or of one of its pseudo-elements.
function judgeTransitions(event) {
  const pseudo = event.pseudoElement || null;
  for (const animation of event.target.getAnimations?.({ subtree: Boolean(pseudo) }) ?? []) {
    const own = animation.effect?.target === event.target && (animation.effect.pseudoElement ?? null) === pseudo;
    if (own && globalThis.CSSTransition && animation instanceof CSSTransition && animation.transitionProperty === event.propertyName) {
      judge(animation);
    }
  }
}

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
  document.addEventListener("transitionrun", judgeTransitions, true);
  // The display's frame, from the shortest of a few gaps between frames as the page starts.
  let last = 0;
  let left = 12;
  const sample = (now) => {
    if (last && now - last > 0 && now - last < 100) {
      state.frame = Math.min(state.frame, now - last);
    }
    last = now;
    if ((left -= 1) > 0) {
      requestAnimationFrame(sample);
    }
  };
  requestAnimationFrame(sample);
  document.addEventListener("visibilitychange", () => {
    state.hidden = document.hidden;
    settle();
  });
  settle();
}
