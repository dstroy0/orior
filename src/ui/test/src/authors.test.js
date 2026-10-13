// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { authorsShown, onAuthors, setAuthorsShown } from "../../src/authors.js";
import { runCommand } from "../../src/menubar.js";
import { keepingStorage } from "../helpers.js";

const { test, assert } = window.__harness;

const KEY = "orior.git.authors";

test("authors are off until the reader turns them on", () =>
  keepingStorage([KEY], () => {
    localStorage.removeItem(KEY);
    assert.equal(authorsShown(), false);
    localStorage.setItem(KEY, "anything");
    assert.equal(authorsShown(), false);
  }));

test("turning authors on and off keeps the choice and tells each listener", () =>
  keepingStorage([KEY], () => {
    const told = [];
    onAuthors((on) => told.push(on));
    setAuthorsShown(true);
    assert.equal(authorsShown(), true);
    assert.equal(localStorage.getItem(KEY), "true");
    setAuthorsShown();
    assert.equal(authorsShown(), false);
    assert.deepEqual(told, [true, false]);
  }));

test("Git, Authors turns them on and off, and on or off as asked", () =>
  keepingStorage([KEY], () => {
    localStorage.removeItem(KEY);
    runCommand("git-authors");
    assert.equal(authorsShown(), true);
    runCommand("git-authors");
    assert.equal(authorsShown(), false);
    runCommand("git-authors", ["on"]);
    runCommand("git-authors", ["on"]);
    assert.equal(authorsShown(), true);
    runCommand("git-authors", ["off"]);
    assert.equal(authorsShown(), false);
  }));
