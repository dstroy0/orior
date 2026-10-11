// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A tree on another machine. File, Open Folder on Another Machine opens a window on a tree reached
// over ssh, in a running container or in a WSL distribution, and File, Open in Dev Container on the
// tree's own container, built and started from its .devcontainer as it says. The status bar names the
// machine and how the link to it stands: joined, joining, or dropped and joining again. Once it is
// joined again, the runs it has not seen end are read again from their first line, and each open tab
// is handed to its server again, the servers having ended with the link.

import { invoke, listen } from "./bridge.js";
import { copyText, menuOn } from "./menu.js";

const RECENT = "orior.remote.recent";

// How many addresses File, Open Folder on Another Machine offers again.
const RECENT_MOST = 8;

const state = { remote: null, link: "", said: "", dropped: false, hooks: {} };

// The tree's machine and address where it is on another machine, or null.
export function remoteTree() {
  return state.remote;
}

// What waits for the link to be joined.
const waiting = [];

// Answers once the link to the tree's machine is joined, at once for a tree on this machine.
export function linkJoined() {
  if (!state.remote || state.link === "joined") {
    return Promise.resolve();
  }
  return new Promise((resolve) => waiting.push(resolve));
}

// Whether an error is the link's own: the machine not reached, or the link dropped while a call
// waited on it. The status bar says the link's state; such an error is no fault to report.
export function linkError(message) {
  const machine = state.remote?.machine;
  return Boolean(machine) && (message.includes(`${machine} is not reached`) || message.includes(`the link to ${machine} dropped`));
}

function recent() {
  try {
    const read = JSON.parse(localStorage.getItem(RECENT) ?? "[]");
    return Array.isArray(read) ? read.filter((one) => typeof one === "string") : [];
  } catch {
    return [];
  }
}

function keepRecent(address) {
  localStorage.setItem(RECENT, JSON.stringify([address, ...recent().filter((one) => one !== address)].slice(0, RECENT_MOST)));
}

// Opens a window on the tree at `address`, user@host:folder, docker:container:folder or
// wsl:distribution:folder, asking for it where it is not given.
export async function openRemote(given) {
  const { askFor } = await import("./menubar.js");
  const address = (given ?? (await askFor("The machine and the tree's folder there: user@host:folder, docker:container:folder or wsl:distribution:folder", recent()[0] ?? "")))?.trim();
  if (!address) {
    return;
  }
  await invoke("window_open", { remote: address, words: [] });
  keepRecent(address);
}

const LABELS = {
  joining: (machine) => `Joining ${machine}`,
  preparing: (machine, said) => said || `Joining ${machine}`,
  joined: (machine) => machine,
  dropped: (machine) => `${machine}: joining again`,
  "joining-again": (machine) => `${machine}: joining again`,
  failed: (machine) => `${machine} is not reached`,
};

function draw() {
  const button = document.getElementById("status-machine");
  if (!button) {
    return;
  }
  button.hidden = !state.remote;
  if (!state.remote) {
    return;
  }
  const label = LABELS[state.link] ?? LABELS.joining;
  button.textContent = label(state.remote.machine, state.said);
  button.dataset.link = state.link || "joining";
  button.title = [state.remote.address, state.said].filter(Boolean).join("\n");
}

function onLink({ payload }) {
  state.link = payload.state;
  state.said = payload.said ?? "";
  draw();
  if (payload.state === "dropped" || payload.state === "failed") {
    state.dropped = state.dropped || payload.state === "dropped";
    state.hooks.say?.(payload.said ? `${state.remote?.machine}: ${payload.said}` : `The link to ${state.remote?.machine} dropped`);
  }
  if (payload.state === "joined") {
    waiting.splice(0).forEach((resolve) => resolve());
    const again = state.dropped;
    state.dropped = false;
    state.hooks.joined?.(again);
  }
}

function items() {
  if (!state.remote) {
    return null;
  }
  return [
    { label: "Join Again", run: () => invoke("remote_rejoin").catch(() => {}) },
    { label: "Copy Address", run: () => copyText(state.remote.address) },
  ];
}

// `hooks` says a word on the status bar, and is told when the link is joined, and whether it had
// dropped before.
export async function startRemote(hooks) {
  state.hooks = hooks;
  state.remote = await invoke("remote_get").catch(() => null);
  await listen("link", onLink);
  if (state.remote) {
    state.link = state.remote.state;
    state.said = state.remote.said ?? "";
    if (state.link === "joined") {
      hooks.joined?.(false);
    }
  }
  const button = document.getElementById("status-machine");
  if (button) {
    menuOn(button, items);
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      button.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, clientX: event.clientX, clientY: event.clientY }));
    });
  }
  draw();
}
