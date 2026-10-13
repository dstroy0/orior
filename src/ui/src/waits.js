// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The three limits of a response. A call the reader's press or key makes, within ACTED of it, is
// watched until it answers. Past ANSWER the pointer shows the window at work; past WORKING the status
// bar says what the window waits on and for how long, how far along the work is and the time it has
// left where the work tells, and offers Stop, which stops the work where the call has a stop of its
// own, and the wait always.
//
// Within a tenth of a second a person feels they act on the thing itself; within a second their
// thought goes on unbroken; past ten seconds they turn to something else.

import { invoke, listen, onCalls } from "./bridge.js";
import { progressBar, timeLeft } from "./progress.js";

const ACTED = 300;
export const ANSWER = 100;
export const WORKING = 1000;

// The calls whose work has a stop of its own, each stopped as its arguments say.
const STOPS = {
  db_run: (args) => invoke("db_stop", { name: args.name }),
  db_export: (args) => invoke("db_stop", { name: args.name }),
  tests_run: () => invoke("tests_stop"),
  profile_start: () => invoke("profile_stop"),
};

// The calls the window makes on its own as the reader types on, which no one waits on: the live
// preview's make of a LaTeX file, whose last pages stay while it runs.
export const UNWAITED = new Set(["latex_make"]);

// Whether the call `command`, made now, is watched: one a press or a key made, and not one of those.
export const watches = (command, now = performance.now()) => now - state.acted <= ACTED && !UNWAITED.has(command);

const state = { acted: -Infinity, waits: new Set(), progress: new Map(), timer: 0, node: null, under: null, at: null, marked: null, showing: false };

// The pointer shows the window at work on the element under it alone, the one whose pointer shows,
// that element's own cursor put back as the pointer leaves it or the wait ends. A mark on the page's
// root over every element would have the page work out every element's style again each time a wait
// starts and ends.
function markUnder() {
  // An element drawn again since the pointer came onto it is found again where the pointer stands.
  if (state.showing && state.under && !state.under.isConnected && state.at) {
    state.under = document.elementFromPoint(state.at.x, state.at.y);
  }
  const target = state.showing ? state.under : null;
  if (state.marked?.element === target) {
    return;
  }
  if (state.marked) {
    const { element, value, priority } = state.marked;
    element.style.setProperty("cursor", value, priority);
    state.marked = null;
  }
  if (target?.style) {
    state.marked = { element: target, value: target.style.getPropertyValue("cursor"), priority: target.style.getPropertyPriority("cursor") };
    target.style.setProperty("cursor", "progress", "important");
  }
}

// Takes how far along the work of the call `command` is, as its own events tell it: { done, of, unit }.
export const progressOf = (command, progress) => {
  state.progress.set(command, { call: command, ...progress });
  tick();
};

// Watches the call `command`, made with `args`, whose answer is `promise`; `stop` ends the wait on it.
// `began` is when it was made.
export function watch(command, args, promise, stop, began = performance.now()) {
  const wait = { command, args, began, stop };
  state.waits.add(wait);
  const done = () => {
    state.waits.delete(wait);
    state.progress.delete(command);
    tick();
  };
  promise.then(done, done);
  tick();
  return wait;
}

// Stops the work of `wait` where it has a stop, and the wait.
function stopWait(wait) {
  STOPS[wait.command]?.(wait.args).catch(() => {});
  wait.stop("stopped waiting");
  state.waits.delete(wait);
  tick();
}

const seconds = (ms) => `${(ms / 1000).toFixed(ms < 10000 ? 1 : 0)} s`;

// What the status bar says of `wait`, `age` milliseconds old.
export function told(wait, age) {
  const progress = state.progress.get(wait.command);
  const along = progress ? `${progress.done}${progress.of ? ` of ${progress.of}` : ""} ${progress.unit ?? "done"}, ` : "";
  const left = progress?.of ? timeLeft(age, progress.done / progress.of) : null;
  return `Waiting on ${wait.command.replace(/_/g, " ")}: ${along}${seconds(age)}${left ? `, ${left}` : ""}`;
}

// Draws the waits as they stand at `now`, and sets the next time to look again.
export function tick(now = performance.now()) {
  window.clearTimeout(state.timer);
  const oldest = [...state.waits].sort((a, b) => a.began - b.began)[0];
  const age = oldest ? now - oldest.began : 0;
  state.showing = Boolean(oldest) && age >= ANSWER;
  markUnder();
  const node = state.node;
  if (node) {
    node.hidden = !oldest || age < WORKING;
    if (!node.hidden) {
      const text = Object.assign(document.createElement("span"), { textContent: told(oldest, age) });
      const parts = [text];
      // Where the work tells its whole, a bar shows how far along it is.
      const progress = state.progress.get(oldest.command);
      if (progress?.of) {
        oldest.bar ??= progressBar();
        oldest.bar.set(progress.done / progress.of);
        parts.push(oldest.bar);
      }
      // One Stop for each wait, kept as the words beside it are drawn again each second: a press
      // begun on it ends on it.
      if (!oldest.stopper) {
        const wait = oldest;
        wait.stopper = Object.assign(document.createElement("button"), { type: "button", className: "status-wait-stop", textContent: "Stop" });
        wait.stopper.addEventListener("click", () => stopWait(wait));
      }
      parts.push(oldest.stopper);
      node.replaceChildren(...parts);
    }
  }
  if (oldest) {
    const next = age < ANSWER ? ANSWER - age : age < WORKING ? WORKING - age : 1000 - ((age - WORKING) % 1000);
    state.timer = window.setTimeout(() => tick(), Math.max(16, next));
  }
}

export function startWaits() {
  state.node = document.getElementById("status-wait");
  // Where the pointer stands, from each pointer event: its coming onto an element, its moving and its
  // press.
  for (const kind of ["pointerover", "pointermove", "pointerdown"]) {
    document.addEventListener(
      kind,
      (event) => {
        state.under = event.target;
        state.at = { x: event.clientX, y: event.clientY };
        markUnder();
      },
      { capture: true, passive: true },
    );
  }
  for (const kind of ["pointerdown", "keydown"]) {
    document.addEventListener(kind, () => (state.acted = performance.now()), true);
  }
  onCalls((command, args, promise, stop) => {
    if (watches(command)) {
      watch(command, args, promise, stop);
    }
  });
  listen("call-progress", ({ payload }) => progressOf(payload.call, payload));
}

