// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The status block: one record every process in the page reads from and writes to.
//
//   scroll   the view's speed, which way it heads, and the level of work a frame drops
//   frame    how long the last frame took and the strain the throttle holds
//   reads    each file still being read, by path: the bytes in and the bytes in all
//   colors   how many lines ahead of the view the idle coloring has reached
//   runs     each run still going, by number: its job
//   bridge   the coherence index: how many keys, maps and rulesets it holds, and when it was read
//   input    whether a key or the pointer is working the editor now
//
// A process writes only its own part and reads any. The throttle writes `scroll` and `frame`, and
// a file being read and the idle coloring read them and stand back while a frame is under strain.
// While `input` is active everything but the rows on screen waits, and a key costs only its own
// rows.

export const status = {
  input: { active: false },
  scroll: { speed: 0, level: 0, heading: 1 },
  frame: { spent: 0, strain: 0 },
  reads: new Map(),
  colors: { ahead: 0 },
  runs: new Map(),
  bridge: { keys: 0, maps: 0, rulesets: 0, at: 0 },
};

const watchers = new Set();
let pending = 0;

// Writes values into one part. For a part that is a map, `values` is [key, value], and a value of
// null takes the key out.
export function write(part, values) {
  const held = status[part];
  if (held instanceof Map) {
    const [key, value] = values;
    if (value === null) {
      held.delete(key);
    } else {
      held.set(key, value);
    }
  } else {
    Object.assign(held, values);
  }
  // Watchers hear once a frame, whatever was written in it.
  if (!pending) {
    pending = requestAnimationFrame(() => {
      pending = 0;
      watchers.forEach((watcher) => watcher(status));
    });
  }
}

export function watch(watcher) {
  watchers.add(watcher);
  return () => watchers.delete(watcher);
}

// Whether the view is taking input, moving or straining, the time for background work to stand back.
export function pressed() {
  return status.input.active || status.scroll.level > 0 || status.frame.strain > 0;
}

// Waits for an idle moment the view is not pressed in, checking each idle moment.
export function calm() {
  return new Promise((resolve) => {
    const check = () => {
      if (!pressed()) {
        resolve();
        return;
      }
      wait(check);
    };
    wait(check);
  });
}

function wait(run) {
  if (window.requestIdleCallback) {
    window.requestIdleCallback(run, { timeout: 250 });
  } else {
    window.setTimeout(run, 16);
  }
}
