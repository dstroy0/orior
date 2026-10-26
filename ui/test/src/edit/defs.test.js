// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The definitions beside the editor, and the bridge's key.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { editing } from "../../../gui/src-js/edit.js";
import { calls, putBackAsFound } from "../../menus.js";

const { test, ui } = window.__harness;

test("Go, Bridge Key opens Lstar.klq at the key's line", async () => {
  calls.answer("bridge_read", () => ({ klq: { path: "hello.py", keys: { greet: { line: 4 } } } }));
  await runCommand("bridge", ["greet"]);
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor.head().line === 4, 3000, "hello.py at the key's line");
});

test("the window is put back as the file found it", putBackAsFound);
