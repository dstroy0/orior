// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The three limits of a response. A call the reader's press or key makes, within ACTED of it, is
// watched until it answers. Past ANSWER the pointer shows the window at work; past WORKING the status
// bar says what the window waits on and for how long; past LONG it says how far along the work is
// where the work tells, and offers Stop, which stops the work where the call has a stop of its own,
// and the wait always.
//
// Within a tenth of a second a person feels they act on the thing itself; within a second their
// thought goes on unbroken; past ten seconds they turn to something else.

import { invoke, listen, onCalls } from "./bridge.js";
import { progressBar } from "./progress.js";

const ACTED = 300;
export const ANSWER = 100;
export const WORKING = 1000;
export const LONG = 10000;

// The calls whose work has a stop of its own, each stopped as its arguments say.
const STOPS = {
  db_run: (args) => invoke("db_stop", { name: args.name }),
  db_export: (args) => invoke("db_stop", { name: args.name }),
  tests_run: () => invoke("tests_stop"),
  profile_start: () => invoke("profile_stop"),
};

const state = { acted: -Infinity, waits: new Set(), progress: new Map(), timer: 0, node: null };

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
  return `Waiting on ${wait.command.replace(/_/g, " ")}: ${along}${seconds(age)}`;
}

// Draws the waits as they stand at `now`, and sets the next time to look again.
export function tick(now = performance.now()) {
  window.clearTimeout(state.timer);
  const oldest = [...state.waits].sort((a, b) => a.began - b.began)[0];
  const age = oldest ? now - oldest.began : 0;
  document.documentElement.classList.toggle("waiting", Boolean(oldest) && age >= ANSWER);
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
      if (age >= LONG) {
        const stop = Object.assign(document.createElement("button"), { type: "button", className: "status-wait-stop", textContent: "Stop" });
        stop.addEventListener("click", () => stopWait(oldest));
        parts.push(stop);
      }
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
  for (const kind of ["pointerdown", "keydown"]) {
    document.addEventListener(kind, () => (state.acted = performance.now()), true);
  }
  onCalls((command, args, promise, stop) => {
    if (performance.now() - state.acted <= ACTED) {
      watch(command, args, promise, stop);
    }
  });
  listen("call-progress", ({ payload }) => progressOf(payload.call, payload));
}
