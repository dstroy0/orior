// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window's own trace: each function traced is timed at its entry and its exit, a line a call,
// indented two spaces for each call it was made inside, as the measuring stick's trace is: its name,
// when it was entered and how long it ran, in milliseconds to the microsecond from the time the trace
// began. The lines are gathered in the trace's own buffer, and while the window is joined to a run,
// the one QRY names as the window starts or one it is joined to, they are queued to the backend's
// primary channel every HAND_EVERY ms as the blob `window.trace`, and the window goes on at once: the
// backend hands them to the run's buffer and the run's writer writes them. While the window is joined
// to no run nothing is gathered, and a traced function costs one test.

import { invoke } from "./bridge.js";

const HAND_EVERY = 1000;
export const TRACE_BLOB = "window.trace";

const state = { on: false, depth: 0, began: 0, calls: [], timer: 0 };

// Enters the call `name`: what `leave` takes, or null where nothing is traced.
export function enter(name) {
  if (!state.on) {
    return null;
  }
  const call = [state.depth, name, performance.now() - state.began, -1];
  state.calls.push(call);
  state.depth += 1;
  return call;
}

export function leave(call) {
  if (call) {
    call[3] = performance.now() - state.began - call[2];
    state.depth -= 1;
  }
}

// Runs `run` as the call `name`, its value given back.
export function traced(name, run) {
  const call = enter(name);
  try {
    return run();
  } finally {
    leave(call);
  }
}

// The calls gathered since the last were taken, a line each, and the buffer emptied.
export function traceLines() {
  const lines = state.calls.map(([depth, name, at, spent]) => `${"  ".repeat(depth)}${name} ${at.toFixed(3)} ${spent.toFixed(3)}\n`).join("");
  state.calls = [];
  return lines;
}

export function tracing() {
  return state.on;
}

// The trace's own clock: milliseconds from the time the trace began, as its lines give them.
export function traceTime() {
  return performance.now() - state.began;
}

function hand() {
  const lines = traceLines();
  if (lines) {
    invoke("qry_hand", { name: TRACE_BLOB, text: lines, channel: "primary" }).catch(() => {});
  }
}

function turned(on) {
  if (on === state.on) {
    return;
  }
  if (on) {
    Object.assign(state, { on: true, depth: 0, began: performance.now(), calls: [] });
    state.timer = window.setInterval(hand, HAND_EVERY);
  } else {
    window.clearInterval(state.timer);
    hand();
    state.on = false;
  }
}

// The window joined to the run at `path`, from the top of the tree or whole, or to none where it is
// null: what was gathered for the run before is handed to it first.
export async function joinRun(path) {
  turned(false);
  const joined = await invoke("qry_join", { path });
  turned(Boolean(joined));
  return joined;
}

// What the trace's queue has done: the run joined, and the blobs handed, dumped and failed.
export function traceJoined() {
  return invoke("qry_window");
}

// The trace started where the window was started inside a run.
export async function startTrace() {
  const joined = await invoke("qry_window").catch(() => null);
  turned(Boolean(joined?.path));
}
