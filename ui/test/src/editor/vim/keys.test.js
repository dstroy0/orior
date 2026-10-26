// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys Vim reads, and how the editor's Vim reads a run of them through them.

import { ARROWS, CHANGES, COMMANDS, MOTIONS, NAMES, OPERATORS, WAIT } from "../../../../gui/src-js/editor/vim/keys.js";
import { Vim } from "../../../../gui/src-js/editor/vim.js";

const { test, assert } = window.__harness;

test("the operators, motions and commands are apart, and what `.` does again is a change", () => {
  for (const one of OPERATORS) {
    assert.ok(!MOTIONS.has(one) && !COMMANDS.has(one), `${one} is an operator alone`);
  }
  assert.ok(!CHANGES.has("y"), "a yank changes nothing");
  assert.ok(["d", "c", "x", "p", "i"].every((one) => CHANGES.has(one)));
  assert.equal(ARROWS.ArrowLeft, "h");
  assert.equal(NAMES.line, "VISUAL LINE");
});

test("a command still waiting for keys answers WAIT, a whole one says what it is, and keys that are none answer null", () => {
  const parse = (keys, mode = "normal") => Vim.prototype.parse.call({ mode, parseMotion: Vim.prototype.parseMotion }, keys);
  assert.equal(parse(["d"]), WAIT);
  assert.equal(parse(["2"]), WAIT);
  assert.deepEqual(parse(["2", "d", "3", "w"]), { register: null, name: "d", count: 6, motion: { name: "w" } });
  assert.deepEqual(parse(["d", "d"]), { register: null, name: "d", count: null, lines: true });
  assert.deepEqual(parse(['"', "a", "y", "i", "w"]), { register: "a", name: "y", count: null, object: { kind: "i", of: "w" } });
  assert.deepEqual(parse(["f", "x"]), { count: null, motion: { name: "f", letter: "x" } });
  assert.equal(parse(["q"]), null);
  assert.deepEqual(parse(["d"], "visual"), { register: null, count: null, name: "d", chosen: true });
});
