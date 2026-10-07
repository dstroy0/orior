// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's one door to the app: the IPC bridge the Rust side puts in every window. A command is
// called by name and answers with a promise, and an event calls its handler with { event, payload }.
// Nothing else in the page reaches the bridge.

const bridge = () => window.__TAURI_INTERNALS__;

export function invoke(command, args = {}) {
  return bridge().invoke(command, args);
}

export async function listen(event, handler) {
  const id = bridge().transformCallback(handler);
  await invoke("plugin:event|listen", { event, target: { kind: "Any" }, handler: id });
}

// The system's picker: "dir" and "save-dir" a folder, "save" a file to write, anything else a file
// to read. Answers the path chosen, or null.
export function pick(kind) {
  return invoke("pick", { kind });
}
