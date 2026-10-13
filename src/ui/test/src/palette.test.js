// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The quick open: Go to File while the tree's files are still being read, answered from the files
// opened lately, saying so, and asked again until the tree is read; and Go, Fuzzy Matching, off for
// the text as typed alone.

import { watchCalls } from "../../src/bridge.js";
import { fuzzyFiles, runCommand } from "../../src/menubar.js";
import { openPalette, paletteOpen } from "../../src/palette.js";
import { fuzzy } from "../../src/fuzzy.js";

const { test, assert, ui, calls } = window.__harness;

const READING = "Reading the tree's files: more may be found.";
const labels = () => [...document.querySelectorAll("#quick-list .quick-row .quick-label")].map((label) => label.textContent);

test("Go to File answers from the files opened lately while the tree's files are read, says so, and asks again until they are read", async () => {
  let unread = 2;
  let asked = 0;
  watchCalls((name, args, send) => {
    if (name === "files_find") {
      asked += 1;
      if (unread > 0) {
        unread -= 1;
        return Promise.resolve({ found: [{ path: "hello.py", score: 1, hits: [0] }], reading: true });
      }
    }
    return calls.watcher(name, args, send);
  });
  try {
    openPalette("hello");
    await ui.waitFor(() => labels().includes(READING), 3000);
    assert.ok(labels().includes("hello.py"), `the file opened lately answers: ${labels().join(", ")}`);
    await ui.waitFor(() => !labels().includes(READING), 3000);
    assert.ok(labels().includes("hello.py"), `the tree's file answers once it is read: ${labels().join(", ")}`);
    assert.ok(asked >= 3, `asked again while the tree was read: ${asked}`);
  } finally {
    watchCalls(calls.watcher);
    if (paletteOpen()) {
      await ui.key("Escape");
    }
  }
  assert.ok(!paletteOpen(), "Escape closes it");
});

test("with Fuzzy Matching off, Go to File answers only the files whose paths hold the text as typed", async () => {
  const kept = fuzzyFiles();
  try {
    runCommand("fuzzy-files", ["on"]);
    openPalette("hlo");
    await ui.waitFor(() => labels().includes("hello.py"), 5000, "hello.py for the letters in order");
    await ui.key("Escape");
    runCommand("fuzzy-files", ["off"]);
    assert.equal(fuzzyFiles(), false);
    openPalette("hlo");
    await ui.waitFor(() => labels().length > 0 && !labels().includes("hello.py"), 5000, `not hello.py for letters apart: ${labels().join(", ")}`);
    await ui.key("Escape");
    openPalette("ello");
    await ui.waitFor(() => labels().includes("hello.py"), 5000, "hello.py for the text as typed");
    await ui.key("Escape");
  } finally {
    runCommand("fuzzy-files", [kept ? "on" : "off"]);
  }
});

test("a list of no rows, held on the Rust side, says it holds none", async () => {
  openPalette("", { custom: { placeholder: "No rows", empty: "Nothing here.", items: [] } });
  try {
    await ui.waitFor(() => labels()[0] === "Nothing here.", 5000, "the list's word that it holds none");
    assert.equal(labels().length, 1);
  } finally {
    await ui.key("Escape");
  }
});

test("a list of thirty thousand rows, held on the Rust side, answers each letter typed fast with its best rows and no frame held", async () => {
  const many = Array.from({ length: 30000 }, (_, at) => ({ name: `test_case_${at}_${(at * 7919) % 30000}`, file: `tests/test_part_${at % 300}.py` }));
  // The row the page's own matcher ranks first for what is typed, which the Rust side ranks first too.
  let best = null;
  for (const one of many) {
    const hit = fuzzy("case12", `${one.name} ${one.file}`);
    if (hit && (!best || hit.score > best.score)) {
      best = { label: one.name, score: hit.score };
    }
  }
  openPalette("", { custom: { placeholder: "Thirty thousand rows", items: many.map((one) => ({ text: `${one.name} ${one.file}`, label: one.name, detail: one.file, run: () => {} })) } });
  const frames = (ms) =>
    new Promise((done) => {
      const times = [];
      const step = (now) => {
        times.push(now);
        if (now - times[0] < ms) {
          requestAnimationFrame(step);
        } else {
          done(times);
        }
      };
      requestAnimationFrame(step);
    });
  try {
    await ui.waitFor(() => document.querySelectorAll("#quick-list .quick-row").length > 0, 10000, "the rows");
    // The list held, and what its opening made let go, before the letters come.
    await frames(1500);
    const counting = frames(1500);
    const began = performance.now();
    await ui.type("case12");
    await ui.waitFor(() => labels()[0] === best.label, 10000, "the rows for what was typed");
    const settled = performance.now() - began;
    const times = await counting;
    const gaps = times.slice(1).map((time, at) => time - times[at]);
    const said = `${document.querySelectorAll("#quick-list .quick-row").length} rows on the page, settled ${settled.toFixed(0)} ms after the first letter, the longest frame ${Math.max(...gaps).toFixed(1)} ms`;
    assert.ok(document.querySelectorAll("#quick-list .quick-row").length <= 200, `the best rows alone on the page: ${said}`);
    assert.ok(Math.max(...gaps) < 50, `no frame held: ${said}`);
    assert.ok(settled < 1000, `the rows for what was typed within a second: ${said}`);
  } finally {
    await ui.key("Escape");
  }
});
