// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's one door to the app: the IPC bridge the Rust side puts in every window. A command is
// called by name and answers with a promise, and an event calls its handler with { event, payload }.
// Nothing else in the page reaches the bridge.

const bridge = () => window.__TAURI_INTERNALS__;

export function invoke(command, args = {}) {
  return bridge().invoke(command, args);
}

// The events that came while the page is held, in the order they came, each with its handler; null
// while the page is not held.
let held = null;

// Holds every event from now on, or hands each one held to its handler, in order, and holds no more.
// The page is held while the window's frame is: a run's lines, the terminal and the language
// servers wait for the drag to end instead of drawing during it.
export function holdEvents(hold) {
  if (hold) {
    held ??= [];
    return;
  }
  const waiting = held ?? [];
  held = null;
  for (const [handler, event] of waiting) {
    handler(event);
  }
}

// `always` is for the event that holds and lets go of the others, which is never held itself.
export async function listen(event, handler, { always = false } = {}) {
  const id = bridge().transformCallback((said) => (held && !always ? held.push([handler, said]) : handler(said)));
  await invoke("plugin:event|listen", { event, target: { kind: "Any" }, handler: id });
}

// The system's picker: "dir" and "save-dir" a folder, "save" a file to write, anything else a file
// to read. Answers the path chosen, or null.
export function pick(kind) {
  return invoke("pick", { kind });
}
