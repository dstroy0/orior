// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Tests window with Rust's tests: a Cargo package's tests found where they are written, run with
// cargo test, each marked passed, failed or skipped beside its line, and a failure told with the line
// it panicked at.

import { invoke, listen } from "../../src/bridge.js";
import { loadTests, runTests, testsOf } from "../../src/tests.js";

const { test, assert, ui } = window.__harness;

const FILE = "rusty/src/lib.rs";

const LIB = "#[cfg(test)]\nmod tests {\n    #[test]\n    fn passes() {}\n\n    #[test]\n    fn fails() {\n        assert_eq!(1, 2);\n    }\n\n    #[test]\n    #[ignore]\n    fn waits() {}\n}\n";

test("a Cargo package's tests are found where they are written, run with cargo test, and each marked as it ended", async () => {
  await invoke("folder_create", { path: "rusty" }).catch(() => {});
  await invoke("folder_create", { path: "rusty/src" }).catch(() => {});
  await invoke("file_write", { path: "rusty/Cargo.toml", text: '[package]\nname = "rusty"\nversion = "0.1.0"\nedition = "2021"\n\n[workspace]\n' });
  await invoke("file_write", { path: FILE, text: LIB });
  let found = [];
  await ui.waitFor(async () => (found = (await loadTests()).filter((one) => one.file === FILE)).length === 3, 10000, "the package's tests found");
  assert.deepEqual(
    found.map((one) => [one.id, one.class, one.name, one.line]),
    [
      [`${FILE}::tests::passes`, "tests", "passes", 3],
      [`${FILE}::tests::fails`, "tests", "fails", 6],
      [`${FILE}::tests::waits`, "tests", "waits", 12],
    ],
  );
  const told = [];
  await listen("tests-run", ({ payload }) => told.push(payload));
  await runTests([FILE]);
  await ui.waitFor(() => told.some((one) => one.kind === "done"), 180000, "the run's end");
  const marks = testsOf(FILE);
  assert.deepEqual([marks.get(3), marks.get(6), marks.get(12)], ["passed", "failed", "skipped"], JSON.stringify(told.slice(-4)));
  const failure = told.filter((one) => one.kind === "result" && one.outcome.id === `${FILE}::tests::fails`).at(-1).outcome;
  assert.equal(failure.file, FILE);
  assert.equal(failure.line, 7, "the line of the assert that panicked");
  assert.match(failure.message, /left: 1/);
  assert.equal(told.find((one) => one.kind === "started").runner, "cargo test");
}, { timeout: 200000 });
