// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { PATTERN_PARTS, onPatterns, patternsOf, searchSets, setPatterns, setSets, setsText, tellPatterns } from "../../src/patterns.js";
import { callsTo, keepingStorage } from "../helpers.js";

const { test, assert, calls } = window.__harness;

const KEYS = [...PATTERN_PARTS.map(({ part }) => `orior.patterns.${part}`), "orior.search.sets"];

test("each part has no patterns until some are set", () =>
  keepingStorage(KEYS, () => {
    for (const { part } of PATTERN_PARTS) {
      localStorage.removeItem(`orior.patterns.${part}`);
      assert.equal(patternsOf(part), "");
    }
  }));

test("setting a part's patterns keeps them, hands them to the Rust a line each, and tells each listener", () =>
  keepingStorage(KEYS, async () => {
    const told = [];
    onPatterns((part) => told.push(part));
    await setPatterns("explorer", "*.log\nbuild/\n!keep.log");
    assert.equal(patternsOf("explorer"), "*.log\nbuild/\n!keep.log");
    assert.deepEqual(callsTo("patterns_set").at(-1).args, { part: "explorer", lines: ["*.log", "build/", "!keep.log"] });
    assert.deepEqual(told, ["explorer"]);
    await setPatterns("explorer", "");
  }));

test("patterns the Rust refuses are kept but tell no listener", () =>
  keepingStorage(KEYS, async () => {
    const told = [];
    onPatterns((part) => told.push(part));
    calls.answer("patterns_set", () => {
      throw new Error("refused");
    });
    await setPatterns("search", "*.tmp");
    assert.equal(patternsOf("search"), "*.tmp");
    assert.deepEqual(told, []);
  }));

test("as the window starts every part's patterns are handed over", async () => {
  await tellPatterns();
  assert.deepEqual(
    callsTo("patterns_set").map((one) => one.args.part),
    PATTERN_PARTS.map(({ part }) => part),
  );
});

test("the sets of Find in Files are read from their names and patterns", () =>
  keepingStorage(KEYS, () => {
    const told = [];
    onPatterns((part) => told.push(part));
    setSets("stray line\nsources:\n  src/**\n!src/gen/**\n\ndocs:\n*.md\n:\nempty:\n");
    assert.equal(setsText().startsWith("stray line"), true);
    assert.deepEqual(searchSets(), [
      { name: "sources", lines: ["src/**", "!src/gen/**"] },
      { name: "docs", lines: ["*.md", ":"] },
      { name: "empty", lines: [] },
    ]);
    assert.deepEqual(told, ["sets"]);
  }));
