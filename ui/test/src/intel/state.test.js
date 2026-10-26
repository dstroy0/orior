// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What every part of the language server's intel reads: the tab the editor shows and the place of its cursor, and
// the tab where a server serves it, said where none does.

import { element, here, served, state } from "../../../gui/src-js/intel/state.js";
import { given, putBackAsFound, told } from "../../menus.js";

const { test, assert } = window.__harness;

// Runs `work` with the intel's hooks giving `editor` and `tab`, and the window's own hooks put back after it.
async function hookedTo(editor, tab, work) {
  const kept = state.hooks;
  state.hooks = { ...kept, editor: () => editor, tab: () => tab };
  try {
    return await work();
  } finally {
    state.hooks = kept;
  }
}

test("an element is made with its properties and the children given, none of them null", () => {
  const made = element("p", { className: "said" }, "one", null, element("b", { textContent: "two" }), undefined);
  assert.equal(made.className, "said");
  assert.equal(made.outerHTML, '<p class="said">one<b>two</b></p>');
});

test("here is the tab the editor shows and its cursor's place from the file's first line, none for another's", async () => {
  const view = await given("one\ntw|o");
  await hookedTo(view, { session: view.s, file: "given.py", served: true }, () => {
    const found = here();
    assert.equal(found.editor, view);
    assert.deepEqual(found.at, { path: "given.py", line: view.s.base + 1, col: 2 });
    assert.equal(served()?.tab.file, "given.py", "a tab served is given");
  });
  await hookedTo(view, { session: {}, file: "other.py" }, () => assert.equal(here(), null, "a tab of another session"));
});

test("a tab no server serves is none, said with its language", async () => {
  const view = await given("x|");
  await hookedTo(view, { session: view.s, file: "given.py", served: false }, () => {
    assert.equal(served(), null);
    assert.ok(told(new RegExp(`No language server serves ${view.s.language?.name ?? "plain text"}: File, Toolchains lists those there are`)));
  });
});

test("the window is put back as the file found it", putBackAsFound);
