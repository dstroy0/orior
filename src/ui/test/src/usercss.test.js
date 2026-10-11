// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { loadUserCss, openUserCss } from "../../src/usercss.js";
import { onScheme } from "../../src/scheme.js";
import { callsTo } from "../helpers.js";

const { test, assert, calls } = window.__harness;

const sheet = () => document.getElementById("user-css");

test("the reader's stylesheet is laid last, over the app's own", async () => {
  calls.answer("user_css_read", () => ":root { --harness-mark: 7px; }");
  await loadUserCss();
  assert.ok(sheet(), "the sheet is there");
  assert.equal(document.head.lastElementChild, sheet(), "it is the last of the head");
  assert.equal(getComputedStyle(document.documentElement).getPropertyValue("--harness-mark").trim(), "7px");
});

test("a change to it tells what reads the scheme's colors; the same text tells nothing", async () => {
  let told = 0;
  onScheme(() => (told += 1));
  calls.answer("user_css_read", () => ":root { --harness-mark: 8px; }");
  await loadUserCss();
  assert.equal(told, 1, "told once of the change");
  await loadUserCss();
  assert.equal(told, 1, "not told again of the same text");
});

test("a stylesheet that cannot be read is laid as nothing", async () => {
  calls.answer("user_css_read", () => {
    throw new Error("no such file");
  });
  await loadUserCss();
  assert.equal(sheet().textContent, "");
  assert.equal(getComputedStyle(document.documentElement).getPropertyValue("--harness-mark").trim(), "");
});

test("File, User Stylesheet opens it and reads it again", async () => {
  calls.answer("home_reveal", () => null);
  calls.answer("user_css_read", () => ":root { --harness-mark: 9px; }");
  await openUserCss();
  assert.deepEqual(callsTo("home_reveal").map((one) => one.args), [{ what: "user-css" }]);
  assert.equal(getComputedStyle(document.documentElement).getPropertyValue("--harness-mark").trim(), "9px");
  calls.answer("user_css_read", () => "");
  await loadUserCss();
});

test("the window coming back to the front reads it again", async () => {
  calls.answer("user_css_read", () => ":root { --harness-mark: 10px; }");
  window.dispatchEvent(new Event("focus"));
  await window.__harness.ui.waitFor(() => getComputedStyle(document.documentElement).getPropertyValue("--harness-mark").trim() === "10px");
  calls.answer("user_css_read", () => "");
  await loadUserCss();
});
