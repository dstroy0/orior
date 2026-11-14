// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The system's session a window starts in, from the computer's start or the reader's log in to the next,
// as the window's side names it (session.rs): what plays once in a session, as the name's flight does,
// plays on the first start in it.

import { invoke } from "./bridge.js";

// The session last started in, as the system named it.
const SESSION = "orior.wordmark.session";

// Whether `now`, the session the system names, is one no start has been in: one whose log in is another,
// or whose computer's start is more than a minute from the retained one's, the start being read a few
// seconds either way from one start to the next. `now` is retained as the session started in either way.
export function firstIn(now) {
  let retained = null;
  try {
    retained = JSON.parse(localStorage.getItem(SESSION) ?? "null");
  } catch {
    retained = null;
  }
  const same = Boolean(retained) && retained.logon === now.logon && Math.abs(retained.booted - now.booted) < 60;
  localStorage.setItem(SESSION, JSON.stringify(now));
  return !same;
}

// Whether this start is the first in the system's session; a start the system names no session for is
// taken as the first. It is asked once a start, and every caller has the one answer.
let first = null;

export function firstStart() {
  first ??= invoke("session_key")
    .catch(() => null)
    .then((now) => !now || firstIn(now));
  return first;
}
