// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bridge beside the editor: the key a line of each bridge file names, the files that are the bridge's, and the
// panel for a key, its pairs and verdicts, its names in each language's map and its entries in each ruleset, each a
// click from its line; with no key, the keys listed and searched.

import { drawBridge, inBridge, keyAt, loadBridge } from "../../gui/src-js/bridge_panel.js";

const { test, assert, calls } = window.__harness;

const INDEX = {
  dir: "table",
  klq: {
    path: "table/Lstar.klq",
    keys: {
      add_rr: { line: 2, pairs: [{ line: 3, first: "add", second: "iadd", verdict: { word: "open", rest: "3 asks" }, notes: [{ line: 4, word: "note", rest: "seen twice" }] }] },
      mov: { line: 9, pairs: [] },
    },
  },
  maps: [{ path: "maps/sass.klm", language: "sass", names: { IADD: { key: "add_rr", line: 5, kind: { line: 6, rest: "kind arithmetic" }, breaks: [] } } }],
  rulesets: [{ path: "rules/sm_86.krs", entries: { add_rr: [{ line: 7, kind: "form" }] } }],
};

const docOf = (...lines) => ({ line: (at) => lines[at] });

// Runs `work` with the bridge read as `index`, and the tree's own read again after it.
async function readingBridge(index, work) {
  calls.answer("bridge_read", () => index);
  try {
    await loadBridge();
    await work();
  } finally {
    calls.answers.delete("bridge_read");
    await loadBridge();
  }
}

test("the key a line names: Lstar.klq's key above it or its pair's, a map's, a ruleset's, and a word that is a key", async () => {
  await readingBridge(INDEX, () => {
    const klq = docOf("key add_rr", "pair add_rr x", "note seen", "pair nothing y");
    assert.equal(keyAt("t/Lstar.klq", klq, 1, null), "add_rr");
    assert.equal(keyAt("t/Lstar.klq", klq, 2, null), "add_rr", "the key above the line");
    assert.equal(keyAt("t/Lstar.klq", klq, 3, null), null, "a pair of no key");
    assert.equal(keyAt("maps/sass.klm", docOf("key IADD add_rr"), 0, null), "add_rr");
    assert.equal(keyAt("maps/sass.klm", docOf("kind IADD arithmetic"), 0, null), "add_rr", "a kind line by the map's name");
    assert.equal(keyAt("rules/sm_86.krs", docOf("form add_rr = x"), 0, null), "add_rr");
    assert.equal(keyAt("a.py", docOf("mov"), 0, "mov"), "mov");
    assert.equal(keyAt("a.py", docOf("other"), 0, "other"), null);
    assert.ok(inBridge("table/forms.tsv") && inBridge("x/a.klm") && inBridge("x/b.KLQ"));
    assert.ok(!inBridge("x/a.py") && !inBridge(""));
  });
});

test("a key's panel: its pairs with their verdicts and notes, its names in each map, its ruleset entries, each opening its line", async () => {
  await readingBridge(INDEX, () => {
    const opened = [];
    const parts = drawBridge("add_rr", (path, line) => opened.push([path, line]), () => {});
    const panel = document.createElement("div");
    panel.append(...parts);
    assert.equal(panel.querySelector(".source").textContent, "table/Lstar.klq");
    const pair = panel.querySelector(".bridge-pair");
    assert.equal(pair.querySelector(".bridge-line").textContent, "add ~ iadd");
    assert.equal(pair.querySelector(".verdict-open").textContent, "open 3 asks");
    assert.equal(pair.querySelector(".bridge-note").textContent, "note seen twice");
    const rows = [...panel.querySelectorAll(".bridge-map")].map((row) => row.textContent);
    assert.deepEqual(rows, ["sassIADD → add_rrkind arithmetic", "sm_86.krsform"]);
    pair.querySelector(".bridge-line").click();
    panel.querySelector(".bridge-map:last-child .bridge-line").click();
    assert.deepEqual(opened, [["table/Lstar.klq", 3], ["rules/sm_86.krs", 7]]);
  });
});

test("with no key the keys are listed with their pairs counted, searched, and one chosen", async () => {
  await readingBridge(INDEX, () => {
    const chosen = [];
    const panel = document.createElement("div");
    panel.append(...drawBridge(null, () => {}, (name) => chosen.push(name)));
    const items = () => [...panel.querySelectorAll(".bridge-item")];
    assert.deepEqual(items().map((one) => one.textContent), ["add_rr1 pairs, 1 open, 0 closed", "mov"]);
    const filter = panel.querySelector(".filter");
    filter.value = "MO";
    filter.dispatchEvent(new Event("input"));
    assert.deepEqual(items().map((one) => one.querySelector("code").textContent), ["mov"]);
    items()[0].click();
    assert.deepEqual(chosen, ["mov"]);
    filter.value = "";
    filter.dispatchEvent(new Event("input"));
  });
});

test("with Lstar.klq not read the panel says so", async () => {
  await readingBridge({ ...INDEX, klq: null }, () => {
    const said = drawBridge("add_rr", () => {}, () => {}).map((part) => part.textContent);
    assert.deepEqual(said, ["Bridge", "Lstar.klq not read"]);
  });
});
