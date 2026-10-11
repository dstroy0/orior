// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, listen, pick, watchCalls } from "../../src/bridge.js";
import { callsTo } from "../helpers.js";

const { test, assert, ui, calls } = window.__harness;

test("a command of the window's own is answered by the window", async () => {
  const version = await invoke("app_version");
  assert.match(version, /^\d+\.\d+\.\d+/);
});

test("a command of the tree's side is answered through call", async () => {
  const root = await invoke("root_get");
  assert.equal(typeof root, "string");
  assert.equal(root, document.getElementById("tree-path").textContent);
});

test("a command neither side knows fails with its name", async () => {
  await assert.rejects(() => invoke("no_such_command_of_orior"), /no_such_command_of_orior|unknown|not found/i);
});

test("the picker answers the path chosen, or null where it is closed", async () => {
  assert.equal(await pick("dir"), null);
  assert.deepEqual(callsTo("pick").map((one) => one.args), [{ kind: "dir" }]);
  calls.answer("pick", ({ kind }) => (kind === "save" ? "C:/chosen/file.txt" : null));
  assert.equal(await pick("save"), "C:/chosen/file.txt");
});

test("an event reaches the handler listening for it, with its payload", async () => {
  const heard = [];
  await listen("harness-bridge-event", (event) => heard.push(event.payload));
  await invoke("plugin:event|emit", { event: "harness-bridge-event", payload: { n: 7 } });
  await ui.waitFor(() => heard.length > 0);
  assert.deepEqual(heard, [{ n: 7 }]);
});

test("each call goes through the watch where one is set, and straight out where none is", async () => {
  const seen = [];
  try {
    assert.equal(
      watchCalls((name, args, send) => {
        seen.push(name);
        return name === "app_version" ? Promise.resolve("9.9.9") : send(name, args);
      }),
      true,
    );
    assert.equal(await invoke("app_version"), "9.9.9");
    assert.equal(typeof (await invoke("root_get")), "string");
    assert.deepEqual(seen, ["app_version", "root_get"]);
    assert.equal(watchCalls(null), false);
    assert.match(await invoke("app_version"), /^\d+\.\d+\.\d+/);
    assert.deepEqual(seen, ["app_version", "root_get"], "no watch, no call seen");
  } finally {
    watchCalls(calls.watcher);
  }
});
