// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the window's tests share: each test leaves the window as it found it, and these keep and give
// back what a test changes.

const { calls, ui } = window.__harness;

// Runs `work` with the page's keys in `keys` kept, and puts each back as it stood once it ends.
export async function keepingStorage(keys, work) {
  const kept = keys.map((key) => [key, localStorage.getItem(key)]);
  try {
    return await work();
  } finally {
    for (const [key, value] of kept) {
      if (value === null) {
        localStorage.removeItem(key);
      } else {
        localStorage.setItem(key, value);
      }
    }
  }
}

// The calls named `name` the page made since the test started.
export const callsTo = (name) => calls.made.filter((one) => one.name === name);

// Waits for the page to make a call named `name`, and gives the last one's arguments.
export async function waitForCall(name, ms = 5000) {
  await ui.waitFor(() => callsTo(name).length > 0, ms);
  return callsTo(name).at(-1).args;
}

// The value of a CSS variable on the root, trimmed.
export const cssVariable = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
