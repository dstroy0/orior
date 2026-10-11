// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { addComment, anchor, comments, onReview, removeComment, resolveComment } from "../../src/review.js";
import { keepingStorage } from "../helpers.js";

const { test, assert } = window.__harness;

const KEY = `orior.review.${document.getElementById("tree-path").textContent}`;

test("a tree with no comments has none, and a kept list that is no list reads as none", () =>
  keepingStorage([KEY], () => {
    localStorage.removeItem(KEY);
    assert.deepEqual(comments(), []);
    localStorage.setItem(KEY, "{ broken");
    assert.deepEqual(comments(), []);
    localStorage.setItem(KEY, '{"a": 1}');
    assert.deepEqual(comments(), []);
  }));

test("a comment is kept with its file, line, text and body, open, and each change is told", () =>
  keepingStorage([KEY], () => {
    localStorage.removeItem(KEY);
    let told = 0;
    onReview(() => (told += 1));
    addComment("src/main.rs", 1, "    a + b", "check the overflow");
    addComment("hello.py", 0, "def greet(name):", "name it better");
    const [first, second] = comments();
    assert.deepEqual([first.path, first.line, first.text, first.body, first.done], ["src/main.rs", 1, "    a + b", "check the overflow", false]);
    assert.notEqual(first.id, second.id);
    resolveComment(first.id);
    assert.equal(comments()[0].done, true);
    resolveComment(first.id, false);
    assert.equal(comments()[0].done, false);
    removeComment(second.id);
    assert.deepEqual(comments().map((one) => one.id), [first.id]);
    assert.equal(told, 5);
  }));

test("a comment stands at its line where the line holds its text", () => {
  assert.deepEqual(anchor({ line: 1, text: "b" }, ["a", "b", "c"]), { line: 1, gone: false });
});

test("a comment whose line moved stands at the line holding its text nearest its number", () => {
  assert.deepEqual(anchor({ line: 2, text: "x" }, ["x", "a", "b", "c", "x"]), { line: 0, gone: false });
  assert.deepEqual(anchor({ line: 3, text: "x" }, ["x", "a", "b", "c", "x"]), { line: 4, gone: false });
});

test("a comment whose text is gone stands at its number, or the last line, marked gone", () => {
  assert.deepEqual(anchor({ line: 1, text: "gone" }, ["a", "b", "c"]), { line: 1, gone: true });
  assert.deepEqual(anchor({ line: 9, text: "gone" }, ["a", "b"]), { line: 1, gone: true });
  assert.deepEqual(anchor({ line: 3, text: "gone" }, []), { line: 0, gone: true });
});
