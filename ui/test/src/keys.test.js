// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Keys: a letter bound by the character it types or by its place, the sets of keys, each key more than
// one command holds shown, and a button of the mouse bound as keys are.

import { letterOf, pressName, setKeySettings, sharedKeys } from "../../gui/src-js/keys.js";
import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { allCommands } from "../../gui/src-js/menubar.js";

const { test, assert, ui } = window.__harness;

const fresh = () => setKeySettings({ set: "orior", by: "character", own: {} });

const commandOf = (name) => allCommands().find((one) => one.command === name).item;

test("a letter is bound by the character it types, or by its place where it types none of the Latin alphabet", () => {
  const azertyZ = new KeyboardEvent("keydown", { key: "z", code: "KeyW", ctrlKey: true });
  assert.equal(letterOf(azertyZ, "character"), "Z", "the key that types z on AZERTY");
  assert.equal(letterOf(azertyZ, "place"), "W", "the key in z's place on QWERTY");
  assert.equal(letterOf(new KeyboardEvent("keydown", { key: "я", code: "KeyZ" }), "character"), "Z", "a Cyrillic layout's letter by its place");
  assert.equal(letterOf(new KeyboardEvent("keydown", { key: "Enter", code: "Enter" })), null);
  assert.equal(pressName(new KeyboardEvent("keydown", { key: "K", code: "KeyK", ctrlKey: true, shiftKey: true })), "Ctrl+Shift+K");
  assert.equal(pressName(new KeyboardEvent("keydown", { key: "~", code: "Backquote", ctrlKey: true, shiftKey: true })), "Ctrl+Shift+`");
  assert.equal(pressName(new KeyboardEvent("keydown", { key: "Control", code: "ControlLeft", ctrlKey: true })), null, "a modifier alone is no key");
});

test("each set of keys holds no key twice but where the commands act in different places", () => {
  for (const set of ["orior", "jetbrains"]) {
    setKeySettings({ set, own: {} });
    for (const [key, sharing] of sharedKeys(allCommands().map((one) => one.item))) {
      const places = sharing.map((item) => item.needs ?? "anywhere");
      assert.equal(new Set(places).size, sharing.length, `${set}: ${key} is blocked by ${sharing.map((item) => item.command).join(", ")} in the same place`);
    }
  }
  assert.equal(commandOf("format").keys, "Ctrl+Alt+L", "JetBrains' Reformat");
  fresh();
  assert.equal(commandOf("format").keys, "Shift+Alt+F");
});

test("the mouse's back button goes back as Alt+Left does", async () => {
  fresh();
  await invoke("file_write", { path: "keys_a.txt", text: "a\n" });
  await invoke("file_write", { path: "keys_b.txt", text: "b\n" });
  await openFile("keys_a.txt");
  await openFile("keys_b.txt");
  await ui.waitFor(() => editing().active === "keys_b.txt", 5000);
  assert.equal(commandOf("back").also, "Mouse4");
  await ui.click("#editor", { button: "back" });
  await ui.waitFor(() => editing().active === "keys_a.txt", 3000);
});
