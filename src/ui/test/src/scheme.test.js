// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { followsSystem, notifyScheme, onScheme, scheme, setFollowSystem, setScheme, toggleScheme } from "../../src/scheme.js";
import { keepingStorage } from "../helpers.js";

const { test, assert } = window.__harness;

const KEY = "orior.scheme";

test("the window opens dark", () => {
  assert.equal(scheme(), "dark");
  assert.equal(document.documentElement.dataset.scheme, "dark");
});

test("choosing a scheme shows it, keeps it and tells each listener", () =>
  keepingStorage([KEY], () => {
    const told = [];
    onScheme((name) => told.push(name));
    try {
      setScheme("light");
      assert.equal(scheme(), "light");
      assert.equal(localStorage.getItem(KEY), "light");
      toggleScheme();
      assert.equal(scheme(), "dark");
      assert.deepEqual(told, ["light", "dark"]);
    } finally {
      setScheme("dark");
    }
  }));

test("following the system shows the system's scheme and keeps that it follows", () =>
  keepingStorage([KEY], () => {
    try {
      setFollowSystem(true);
      assert.equal(followsSystem(), true);
      assert.equal(localStorage.getItem(KEY), "system");
      assert.equal(scheme(), matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
      setFollowSystem(false);
      assert.equal(followsSystem(), false);
      assert.equal(localStorage.getItem(KEY), scheme());
    } finally {
      setScheme("dark");
    }
  }));

test("a theme's change tells each listener with the scheme unchanged", () => {
  const told = [];
  onScheme((name) => told.push(name));
  notifyScheme();
  assert.deepEqual(told, [scheme()]);
});
