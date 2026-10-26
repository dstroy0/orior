// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The terminal's shells: what is chosen copied, text pasted, and the panel closed.

import { asFound, calls, press, putBackAsFound } from "../../menus.js";

const { test, assert, ui } = window.__harness;

test("the terminal's menu, from the command line: what is chosen copied, text pasted, and the panel closed", async () => {
  asFound();
  await press("new");
  const view = () => document.querySelector("#term-views .term-view:not([hidden])");
  await ui.waitFor(() => view()?.textContent.trim(), 15000, "the shell's prompt");
  await press("terminal-paste", "echo pasted-by-the-test");
  await ui.waitFor(() => /pasted-by-the-test/.test(view().textContent), 5000, "the text pasted into the shell");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  window.getSelection().selectAllChildren(view());
  await press("terminal-copy");
  assert.match(copied.at(-1) ?? "", /pasted-by-the-test/, "the screen's chosen text copied");
  window.getSelection().removeAllRanges();
  await press("kill");
  await press("terminal-close");
  assert.ok(document.getElementById("term").hidden, "the panel closed");
  asFound();
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
