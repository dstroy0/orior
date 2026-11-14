// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A file's changes side by side: the lines the two texts share level, a line changed, added or taken out marked,
// F7 and Shift+F7 stepping from change to change, Escape closing it, and white space set aside.

import { closeDiff, showDiff } from "../../gui/src-js/diffview.js";
import { retentionStorage } from "../helpers.js";

const { test, assert } = window.__harness;

// Runs `work` with a host on the page for the view, the view's choices retained, and the view closed after it.
async function withHost(work) {
  const host = document.createElement("div");
  document.body.append(host);
  try {
    await retentionStorage(["orior.diff-space", "orior.diff-structure"], async () => {
      localStorage.removeItem("orior.diff-structure");
      localStorage.setItem("orior.diff-space", "displayed");
      await work(host);
    });
  } finally {
    closeDiff();
    host.remove();
  }
}

const rowsOf = (host) => [...host.querySelectorAll(".diff-row")].map((row) => [row.className.replace("diff-row ", ""), ...[...row.querySelectorAll(".diff-text")].map((one) => one.textContent)]);
const key = (host, key, shiftKey = false) => host.querySelector(".diff-view").dispatchEvent(new KeyboardEvent("keydown", { key, shiftKey, bubbles: true, cancelable: true }));

test("the two texts side by side, the lines they share level, a line changed and one added marked", async () => {
  await withHost(async (host) => {
    await showDiff(host, "a.txt", "one\ntwo\nthree", "one\n2\nthree\nfour");
    assert.equal(host.querySelector(".diff-name").textContent, "a.txt");
    assert.equal(host.querySelector(".diff-sides").textContent, "last commit, then as it is");
    assert.deepEqual(rowsOf(host), [["same", "one", "one"], ["changed", "two", "2"], ["same", "three", "three"], ["added", "", "four"]]);
    assert.equal(host.querySelector(".diff-where").textContent, "change 1 of 2");
    key(host, "F7");
    assert.equal(host.querySelector(".diff-where").textContent, "change 2 of 2");
    key(host, "F7", true);
    assert.equal(host.querySelector(".diff-where").textContent, "change 1 of 2");
    key(host, "Escape");
    assert.equal(host.querySelector(".diff-view"), null, "Escape closes it");
  });
});

test("a file the last commit does not hold is all added, and two texts that do not differ say so", async () => {
  await withHost(async (host) => {
    await showDiff(host, "new.txt", null, "a\nb");
    assert.equal(host.querySelector(".diff-sides").textContent, "not in the last commit");
    assert.deepEqual(rowsOf(host).map(([type]) => type), ["added", "added"]);
    await showDiff(host, "same.txt", "a", "a", { sides: "one, then the other", same: "The two are the same." });
    assert.equal(host.querySelector(".diff-sides").textContent, "one, then the other");
    assert.equal(host.querySelector(".diff-empty").textContent, "The two are the same.");
    assert.equal(host.querySelectorAll(".diff-view").length, 1, "one view at a time");
  });
});

test("with white space set aside, lines that differ only in it are level as the same, and the choice is retained", async () => {
  await withHost(async (host) => {
    await showDiff(host, "spaced.txt", "a  b\nc", "a b\nc");
    assert.deepEqual(rowsOf(host).map(([type]) => type), ["changed", "same"]);
    const box = host.querySelector(".diff-space:not(.diff-structure) input");
    box.checked = true;
    box.dispatchEvent(new Event("change"));
    assert.equal(localStorage.getItem("orior.diff-space"), "aside");
    await new Promise((done) => setTimeout(done, 50));
    assert.equal(host.querySelector(".diff-empty")?.textContent, "No change from the last commit.");
  });
});
