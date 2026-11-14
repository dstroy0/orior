// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The system's session a window starts in, from the computer's start or a log in to the next: what plays
// once in a session plays on the first start in it.

import { invoke } from "../../gui/src-js/bridge.js";
import { firstIn } from "../../gui/src-js/session.js";

const { test, assert } = window.__harness;

test("a start is the first in the system's session only once, from the computer's start or a log in to the next", async () => {
  const RETAINED = "orior.wordmark.session";
  const retained = localStorage.getItem(RETAINED);
  try {
    const session = await invoke("session_key");
    assert.ok(Number.isFinite(session.booted) && session.booted > 0, `the computer's start: ${JSON.stringify(session)}`);
    localStorage.removeItem(RETAINED);
    assert.equal(firstIn(session), true, "the first start in a session");
    assert.equal(firstIn(session), false, "a start after it in the same session");
    assert.equal(firstIn({ ...session, booted: session.booted + 20 }), false, "the computer's start read a few seconds off is the same session");
    assert.equal(firstIn({ ...session, booted: session.booted + 3600 }), true, "a start after the computer restarts");
    assert.equal(firstIn({ ...session, logon: `${session.logon}0` }), true, "a start after a log in");
  } finally {
    if (retained === null) {
      localStorage.removeItem(RETAINED);
    } else {
      localStorage.setItem(RETAINED, retained);
    }
  }
});
