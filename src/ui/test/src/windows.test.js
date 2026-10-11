// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { onTold, openWindow, showWindow, tell, windowId } from "../../src/windows.js";
import { callsTo } from "../helpers.js";

const { test, assert, calls } = window.__harness;

// A message as another window's tell leaves it, which the browser hands this one as a storage event.
const arrive = (value) => window.dispatchEvent(new StorageEvent("storage", { key: "orior.tell", newValue: value }));

test("this window has a name of its own", () => {
  assert.equal(typeof windowId, "string");
  assert.ok(windowId.length >= 8);
});

test("a message for this window runs what its kind does, with its body", () => {
  const heard = [];
  onTold("harness-kind", (body) => heard.push(body));
  arrive(JSON.stringify({ to: windowId, kind: "harness-kind", body: { path: "a.txt" } }));
  assert.deepEqual(heard, [{ path: "a.txt" }]);
});

test("a message for another window, of a kind nothing does, or unreadable, does nothing", () => {
  const heard = [];
  onTold("harness-kind", (body) => heard.push(body));
  arrive(JSON.stringify({ to: "another window", kind: "harness-kind", body: 1 }));
  arrive(JSON.stringify({ to: windowId, kind: "no such kind", body: 2 }));
  arrive("{ not json");
  arrive(null);
  window.dispatchEvent(new StorageEvent("storage", { key: "another key", newValue: JSON.stringify({ to: windowId, kind: "harness-kind", body: 3 }) }));
  assert.deepEqual(heard, []);
});

test("telling a window leaves the message for the others and takes it away at once", () => {
  tell("someone", "open", { path: "x" });
  assert.equal(localStorage.getItem("orior.tell"), null);
});

test("a new window is asked for on this tree, or the one given, to run the words given", async () => {
  calls.answer("window_open", () => true);
  await openWindow();
  await openWindow(["edit", "open", "a.txt"], "C:/other/tree");
  assert.deepEqual(
    callsTo("window_open").map((one) => one.args),
    [
      { root: null, words: [] },
      { root: "C:/other/tree", words: ["edit", "open", "a.txt"] },
    ],
  );
});

test("showing the window asks for it in front, and a refusal answers false", async () => {
  assert.equal(await showWindow(), false);
  assert.deepEqual(callsTo("window_act").at(-1).args, { act: "focus" });
  calls.answer("window_act", () => true);
  assert.equal(await showWindow(), true);
});
