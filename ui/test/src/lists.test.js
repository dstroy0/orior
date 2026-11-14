// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys of a list beside a view: Up and Down from row to row and out to the search field, Home and End, Right and
// Left opening and closing a group or stepping out to the row that holds one, characters typed finding a row, and the
// focus retained on its row as the list is drawn again.

import { focusedKey, retainListKeys, refocus } from "../../gui/src-js/lists.js";

const { test, assert } = window.__harness;

const row = (key, text) => {
  const made = Object.assign(document.createElement("button"), { type: "button", textContent: text });
  made.dataset.key = key;
  return made;
};

// A list of a row, an open group of one row, and a row after it, with its search field, on the page while `work`
// runs.
async function withList(work) {
  const list = document.createElement("div");
  const search = document.createElement("input");
  const group = document.createElement("details");
  const summary = Object.assign(document.createElement("summary"), { textContent: "group" });
  summary.dataset.key = "g";
  group.open = true;
  group.append(summary, row("b", "beta"));
  list.append(row("a", "alpha"), group, row("c", "gamma"));
  const host = document.createElement("div");
  host.append(search, list);
  document.body.append(host);
  retainListKeys(list, search);
  try {
    return await work({ list, search, group, at: (key) => list.querySelector(`[data-key="${key}"]`) });
  } finally {
    host.remove();
  }
}

const press = (key) => document.activeElement.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true, cancelable: true }));

test("Up and Down step from row to row past a closed group's rows, Home and End to the ends, and Up from the first to the search field", async () => {
  await withList(({ list, search, group, at }) => {
    at("a").focus();
    press("ArrowDown");
    assert.equal(focusedKey(list), "g");
    press("ArrowDown");
    assert.equal(focusedKey(list), "b");
    press("ArrowLeft");
    assert.equal(focusedKey(list), "g", "Left steps out to the row that holds it");
    press("ArrowLeft");
    assert.ok(!group.open, "and closes an open group");
    press("ArrowDown");
    assert.equal(focusedKey(list), "c", "a closed group's rows are passed");
    press("Home");
    assert.equal(focusedKey(list), "a");
    press("ArrowUp");
    assert.equal(document.activeElement, search);
    press("ArrowDown");
    assert.equal(focusedKey(list), "a", "Down in the search field steps into the list");
    press("End");
    assert.equal(focusedKey(list), "c");
    at("g").focus();
    press("ArrowRight");
    assert.ok(group.open, "Right opens a closed group");
  });
});

test("characters typed find the row whose name matches from the row on, and Escape ends the search", async () => {
  await withList(({ list, at }) => {
    at("a").focus();
    press("g");
    press("a");
    press("m");
    assert.equal(focusedKey(list), "c");
    press("Escape");
    press("ArrowUp");
    assert.equal(focusedKey(list), "b", "Up steps again once the search ends");
  });
});

test("the row with the focus is found by its key, and focused again after the list is drawn anew", async () => {
  await withList(({ list, at }) => {
    assert.equal(focusedKey(list), null, "no row of the list has the focus");
    at("b").focus();
    const key = focusedKey(list);
    list.replaceChildren(row("a", "alpha"), row("b", "beta"));
    refocus(list, key);
    assert.equal(document.activeElement, list.querySelector('[data-key="b"]'));
    refocus(list, null);
    assert.equal(focusedKey(list), "b", "no key leaves the focus where it is");
  });
});
