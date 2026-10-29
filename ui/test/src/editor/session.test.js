// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// One open file as the editor holds it: the indent read from its lines and from a mode line, a twin holding the same
// text with selections of its own, a region folded, and a large file's window grown and let go at either end.

import { Doc } from "../../../gui/src-js/editor/document.js";
import { modeIndent, Session } from "../../../gui/src-js/editor/session.js";

const { test, assert } = window.__harness;

const at = (line, col) => ({ line, col });

test("a mode line among the first and last lines sets the indent as Vim and Emacs read it, and holds only what it sets", () => {
  assert.deepEqual(modeIndent(new Doc("x\n# vim: set ts=8 sw=2 et:")), { tabs: false, size: 2 });
  assert.deepEqual(modeIndent(new Doc("/* vim: noet ts=3 */\nx")), { tabs: true, size: 3 }, "ts where no sw is set");
  assert.deepEqual(modeIndent(new Doc("# -*- indent-tabs-mode: nil; tab-width: 4 -*-\nx")), { tabs: false, size: 4 });
  assert.deepEqual(modeIndent(new Doc("one\ntwo")), {});
  const far = ["# vim: sw=2", ...Array.from({ length: 12 }, () => "x")].reverse().join("\n");
  assert.deepEqual(modeIndent(new Doc(far)), { size: 2 }, "the file's last lines are read too");
});

test("the indent is read from the lines: tabs where more open with one, else the step most often taken, and a mode line over it", () => {
  assert.deepEqual(new Session("a\n\tb\n\tc\n  d", null).indent, { tabs: true, size: 4 });
  assert.deepEqual(new Session("a:\n  b\n    c\n  d\ne", null).indent, { tabs: false, size: 2 });
  assert.deepEqual(new Session("a:\n  b\n# vim: sw=8 noet", null).indent, { tabs: true, size: 8 });
});

test("a twin holds the same text and language with selections of its own, and lets go of it", () => {
  const first = new Session("one\ntwo", null);
  first.selections = [{ anchor: at(1, 1), head: at(1, 2), goal: null }];
  const twin = first.twin();
  assert.equal(twin.doc, first.doc);
  assert.equal(twin.of, first);
  assert.ok(first.shared && twin.shared);
  assert.deepEqual(twin.selections, first.selections);
  twin.selections[0].head = at(0, 0);
  assert.deepEqual(first.selections[0].head, at(1, 2), "the twin's selections are its own");
  first.setLanguage({ id: "other", grammar: null });
  assert.equal(twin.language.id, "other", "and its language the first's");
  twin.drop();
  assert.ok(!first.shared);
});

test("a region orior's side folds is folded to the last line its fold hides, and shows the line it closes on", () => {
  const session = new Session("f {\n    x\n    g {\n        y\n    }\n}\nz", null);
  assert.ok(!session.opens(0), "a text reads no folds of its own");
  session.highlight.folded(0, [[0, 5, 1], [2, 4, 1]]);
  assert.ok(session.opens(0) && session.opens(2));
  assert.equal(session.endOf(0), 5);
  session.fold(0);
  assert.equal(session.folded.get(0), 5);
  assert.ok(session.shows(0), "the closing line drawn after what the fold hides");
  session.fold(6);
  assert.ok(!session.folded.has(6), "a line that opens nothing does not fold");
  assert.deepEqual(session.openersOf(3, 10), [0, 2], "the regions a line stands inside, the outermost first");
  assert.deepEqual(session.openersOf(3, 1), [2], "the innermost alone");
});

test("orior's side's folds go with their lines through an edit until it hands them on again", () => {
  const session = new Session("f {\n    x\n}\nz", null);
  session.highlight.folded(0, [[0, 2, 1]]);
  session.doc.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "a\n" }]);
  assert.deepEqual([...session.highlight.folds], [[1, [3, true]]]);
});

test("a large file's window grows above and below, and lets lines go, every place moving with the lines above it", () => {
  const session = new Session("c\nd", null, { base: 10, window: { start: 4, end: 8, size: 20 } });
  session.selections = [{ anchor: at(1, 0), head: at(1, 1), goal: null }];
  session.grow("a\nb\n", true);
  assert.deepEqual(session.doc.lines, ["a", "b", "c", "d"]);
  assert.equal(session.base, 8);
  assert.deepEqual(session.selections[0].head, at(3, 1));
  session.grow("e\nf", false);
  assert.deepEqual(session.doc.lines, ["a", "b", "c", "de", "f"], "a read below goes on from the last line");
  session.shrink(2, true);
  assert.deepEqual(session.doc.lines, ["c", "de", "f"]);
  assert.equal(session.base, 10);
  assert.deepEqual(session.selections[0].head, at(1, 1));
  session.shrink(2, false);
  assert.deepEqual(session.doc.lines, ["c"]);
  assert.deepEqual(session.selections[0].head, at(0, 1), "a place let go held at the window's last line");
  session.replaceWindow(["x", "y"], 100, 1);
  assert.deepEqual([session.doc.lines, session.base, session.top], [["x", "y"], 100, 1]);
  assert.deepEqual(session.selections[0].head, at(0, 0));
});
