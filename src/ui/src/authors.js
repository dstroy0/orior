// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Who made each commit: shown in the commit graph, matched by its search and shown in Line History
// only where the reader turns it on from Git, Authors. It is off until they do, and stays as they
// leave it.

const KEY = "orior.git.authors";
const listeners = [];

export const authorsShown = () => localStorage.getItem(KEY) === "true";

// Tells `listener` each time authors are shown or hidden.
export function onAuthors(listener) {
  listeners.push(listener);
}

export function setAuthorsShown(on = !authorsShown()) {
  localStorage.setItem(KEY, String(on));
  listeners.forEach((listener) => listener(on));
}
