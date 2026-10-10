// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Tests: the tree's Python tests in a window of their own, by file and by test, each marked passed,
// failed, skipped or running as its run says, each result coming in as its test ends. A failed test
// opens where it failed, any other where it is written, and Run Failed runs again those that failed.
// A run spreads its files over the machine's cores; one run with coverage records which lines ran,
// each file's share listed under the tests and its lines marked in the gutter. Beside each test in
// the editor a mark opens its menu, which runs it or debugs it.

import { invoke, listen } from "./bridge.js";
import { debugTest } from "./debug.js";
import { iconOf, paneOpen } from "./explorer.js";
import { showMenu } from "./menu.js";
import { say } from "./statusbar.js";

// The kept choice of spreading a run over the machine's cores.
const PARALLEL_KEY = "orior.tests.parallel";

const state = {
  hooks: null,
  // The tests found, by their names as pytest gives them.
  found: [],
  loaded: false,
  // Each test's last result by its name; the run going, the tests or files it was given, and the
  // end of the last.
  results: new Map(),
  run: null,
  runGiven: [],
  last: null,
  // The files whose rows are closed, and each file's coverage of the last run that recorded it.
  closed: new Set(),
  coverage: new Map(),
};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const fileOf = (id) => id.split("::")[0];

// Whether a run spreads its files over the machine's cores.
export function testsParallel() {
  return localStorage.getItem(PARALLEL_KEY) !== "false";
}

export function setTestsParallel(on) {
  localStorage.setItem(PARALLEL_KEY, String(Boolean(on)));
  say(on ? "A run of tests spreads its files over the machine's cores." : "A run of tests runs its files in one process.");
  draw();
}

// Reads the tree's tests again, keeping each one's last result.
export async function loadTests() {
  try {
    state.found = await invoke("tests_found");
    state.loaded = true;
  } catch {
    state.found = [];
  }
  draw();
  state.hooks?.repaint();
  return state.found;
}

// Forgets what the tree open before held.
export function forgetTests() {
  Object.assign(state, { found: [], loaded: false, results: new Map(), run: null, last: null, coverage: new Map() });
  draw();
}

// Runs `given`, tests by their names or files by their paths, recording coverage where `cover`.
export async function runTests(given, { cover = false } = {}) {
  if (!given.length) {
    say("No test is found to run: Run, Find Tests says what is taken as one.", { failed: true });
    return;
  }
  await state.hooks?.saveAll();
  // The tests a run is given are marked running as it starts.
  state.runGiven = given;
  try {
    await invoke("tests_run", { given, parallel: testsParallel(), cover });
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Run All Tests: every file of tests the tree holds.
export async function runAllTests({ cover = false } = {}) {
  if (!state.loaded) {
    await loadTests();
  }
  await runTests([...new Set(state.found.map((test) => test.file))], { cover });
}

// Run, Run Failed Tests: the tests whose last result failed.
export async function runFailedTests() {
  const failed = [...state.results.values()].filter((one) => one.outcome === "failed").map((one) => one.id);
  if (!failed.length) {
    say("No test failed in the last run.");
    return;
  }
  await runTests(failed);
}

export async function stopTests() {
  await invoke("tests_stop").catch(() => {});
}

// What each test of `file` last did, by its line, for the gutter: "passed", "failed", "skipped",
// "running", or "none" for a test not run.
export function testsOf(file) {
  const found = new Map();
  for (const test of state.found) {
    if (test.file === file) {
      found.set(test.line, state.results.get(test.id)?.outcome ?? "none");
    }
  }
  return found.size ? found : null;
}

// The lines of `file` the last run with coverage ran, and those it could run that did not.
export function coverageOf(file) {
  return state.coverage.get(file) ?? null;
}

// The menu of the test written at `line` of `file`: run it, run it recording coverage, or debug it.
export function testMenu(file, line, x, y) {
  const test = state.found.find((one) => one.file === file && one.line === line);
  if (!test) {
    return;
  }
  const label = test.class ? `${test.class}.${test.name}` : test.name;
  showMenu(x, y, [
    { label: `Run ${label}`, run: () => runTests([test.id]) },
    { label: `Run ${label} with Coverage`, run: () => runTests([test.id], { cover: true }) },
    { label: `Debug ${label}`, run: () => debugTest(test.id) },
  ]);
}

// What a run tells as it goes.
function heard(told) {
  if (told.kind === "started") {
    state.run = { number: told.run, given: told.given, workers: told.workers, runner: told.runner, started: performance.now() };
    for (const test of state.found) {
      if (state.runGiven.includes(test.id) || state.runGiven.includes(test.file)) {
        state.results.set(test.id, { id: test.id, outcome: "running", message: "", file: null, line: null, seconds: 0 });
      }
    }
  } else if (told.kind === "result") {
    const before = state.results.get(told.outcome.id);
    // A failure's details come after its result, and keep what the result said.
    state.results.set(told.outcome.id, { ...before, ...told.outcome, message: told.outcome.message || before?.message || "" });
  } else if (told.kind === "coverage") {
    state.coverage = new Map(told.files.map((one) => [one.file, { ran: new Set(one.ran), missed: new Set(one.missed) }]));
  } else if (told.kind === "done") {
    for (const [id, one] of state.results) {
      if (one.outcome === "running") {
        state.results.set(id, { ...one, outcome: "none" });
      }
    }
    state.last = told;
    state.run = null;
    const summary = summaryOf(told);
    say(told.said && !told.passed && !told.failed && !told.skipped ? `No test ran: ${told.said}` : summary, { failed: told.failed > 0 || Boolean(told.said) });
  }
  draw();
  state.hooks?.repaint();
}

function summaryOf(done) {
  const parts = [`${done.passed} passed`, `${done.failed} failed`];
  if (done.skipped) {
    parts.push(`${done.skipped} skipped`);
  }
  return `${parts.join(", ")} in ${done.seconds.toFixed(1)} s`;
}

// The tests to draw, each file's in its order: those found, and those a run named that were not
// found, as a test pytest makes from one written once for each of its parameters.
function testsByFile() {
  const files = new Map();
  for (const test of state.found) {
    if (!files.has(test.file)) {
      files.set(test.file, []);
    }
    files.get(test.file).push({ id: test.id, label: test.class ? `${test.class}.${test.name}` : test.name, line: test.line });
  }
  for (const id of state.results.keys()) {
    const file = fileOf(id);
    if (!files.get(file)?.some((one) => one.id === id)) {
      if (!files.has(file)) {
        files.set(file, []);
      }
      files.get(file).push({ id, label: id.split("::").slice(1).join("."), line: null });
    }
  }
  return files;
}

function button(label, title, run, disabled = false) {
  const made = element("button", { className: "tests-button", type: "button", textContent: label, title, disabled });
  made.addEventListener("click", run);
  return made;
}

function draw() {
  const body = document.getElementById("tests");
  if (!body || !paneOpen("tests")) {
    return;
  }
  const failed = [...state.results.values()].some((one) => one.outcome === "failed");
  const bar = element(
    "div",
    { className: "tests-bar" },
    button("Run All", "Run every test of the tree", () => runAllTests()),
    button("Run Failed", "Run again the tests that failed", () => runFailedTests(), !failed),
    button("Coverage", "Run every test, recording which lines run", () => runAllTests({ cover: true })),
    button("Stop", "Stop the run", () => stopTests(), !state.run),
  );
  const parallel = element("label", { className: "tests-parallel", title: "Spread a run's files over the machine's cores" }, element("input", { type: "checkbox", checked: testsParallel() }), "Parallel");
  parallel.querySelector("input").addEventListener("change", (event) => setTestsParallel(event.target.checked));
  bar.append(parallel);
  const rows = [bar];
  if (state.run) {
    const done = [...state.results.values()].filter((one) => !["running", "none"].includes(one.outcome)).length;
    rows.push(element("p", { className: "pane-empty tests-said", textContent: `Running with ${state.run.runner} over ${state.run.workers} process${state.run.workers === 1 ? "" : "es"}: ${done} done.` }));
  } else if (state.last) {
    rows.push(element("p", { className: `pane-empty tests-said${state.last.failed ? " failed" : ""}`, textContent: state.last.said && !state.last.passed && !state.last.failed ? `No test ran: ${state.last.said}` : summaryOf(state.last) }));
  }
  const files = testsByFile();
  if (!files.size) {
    rows.push(element("p", { className: "pane-empty", textContent: state.loaded ? "No test is found: pytest and unittest take files named test_*.py, *_test.py and test*.py, and in them the functions and methods whose names start with test." : "Reading the tree's tests…" }));
  }
  for (const [file, tests] of files) {
    const outcomes = tests.map((test) => state.results.get(test.id)?.outcome ?? "none");
    const open = !state.closed.has(file);
    const cut = file.lastIndexOf("/");
    const counts = element("span", { className: "count" });
    for (const outcome of ["failed", "passed", "skipped"]) {
      const count = outcomes.filter((one) => one === outcome).length;
      if (count) {
        counts.append(element("span", { className: `tests-count ${outcome}`, textContent: String(count) }));
      }
    }
    const worst = ["running", "failed", "passed", "skipped"].find((one) => outcomes.includes(one)) ?? "none";
    const row = element("button", { className: "node dir problem-file tests-file", type: "button" }, element("span", { className: "twisty" }), element("span", { className: `test-mark ${worst}` }), iconOf(file.slice(cut + 1)), element("span", { className: "name", textContent: file.slice(cut + 1) }), element("span", { className: "where", textContent: file.slice(0, Math.max(0, cut)) }), counts);
    row.dataset.depth = "0";
    row.setAttribute("aria-expanded", String(open));
    row.addEventListener("click", () => {
      state.closed[open ? "add" : "delete"](file);
      draw();
    });
    row.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      showMenu(event.clientX, event.clientY, [
        { label: `Run ${file.slice(cut + 1)}`, run: () => runTests([file]) },
        { label: `Run ${file.slice(cut + 1)} with Coverage`, run: () => runTests([file], { cover: true }) },
      ]);
    });
    rows.push(row);
    if (!open) {
      continue;
    }
    for (const test of tests) {
      const result = state.results.get(test.id);
      const outcome = result?.outcome ?? "none";
      const seconds = result?.seconds ? element("span", { className: "where", textContent: result.seconds < 1 ? `${Math.round(result.seconds * 1000)} ms` : `${result.seconds.toFixed(1)} s` }) : null;
      const testRow = element("button", { className: `problem test-row ${outcome}`, type: "button", title: result?.message ? `${test.id}\n${result.message}` : test.id }, element("span", { className: "guide" }), element("span", { className: `test-mark ${outcome}` }), element("span", { className: "name", textContent: test.label }), seconds);
      testRow.dataset.test = test.id;
      testRow.addEventListener("click", () => {
        if (outcome === "failed" && result?.line !== null && result?.line !== undefined) {
          state.hooks?.openAt(result.file ?? file, result.line, 0);
        } else if (test.line !== null) {
          state.hooks?.openAt(file, test.line, 0);
        }
      });
      testRow.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        showMenu(event.clientX, event.clientY, [
          { label: "Run", run: () => runTests([test.id]) },
          { label: "Run with Coverage", run: () => runTests([test.id], { cover: true }) },
          { label: "Debug", run: () => debugTest(test.id) },
        ]);
      });
      rows.push(testRow);
      if (outcome === "failed" && result?.message) {
        rows.push(element("div", { className: "test-said", textContent: result.message.split("\n")[0] }));
      }
    }
  }
  if (state.coverage.size) {
    let ran = 0;
    let could = 0;
    const covered = [...state.coverage].map(([file, lines]) => {
      ran += lines.ran.size;
      could += lines.ran.size + lines.missed.size;
      return [file, lines];
    });
    rows.push(element("p", { className: "tests-cover-head", textContent: `Coverage: ${share(ran, could)} of ${could} lines` }));
    for (const [file, lines] of covered.sort(([a], [b]) => a.localeCompare(b))) {
      const total = lines.ran.size + lines.missed.size;
      const bar = element("span", { className: "tests-cover-bar" }, element("span", { className: "tests-cover-ran" }));
      bar.firstChild.style.width = `${total ? (100 * lines.ran.size) / total : 0}%`;
      const row = element("button", { className: "problem tests-cover", type: "button", title: `${file}: ${lines.ran.size} of ${total} lines ran` }, element("span", { className: "name", textContent: file }), bar, element("span", { className: "where", textContent: share(lines.ran.size, total) }));
      row.addEventListener("click", () => state.hooks?.openAt(file, [...lines.missed].sort((a, b) => a - b)[0] ?? 0, 0));
      rows.push(row);
    }
  }
  body.replaceChildren(...rows);
}

const share = (part, whole) => (whole ? `${Math.round((100 * part) / whole)}%` : "0%");

// `hooks` gives the window what it asks of the editor: openAt(path, line, col), saveAll() and
// repaint().
export function startTests(hooks) {
  state.hooks = hooks;
  listen("tests-run", (event) => heard(event.payload));
  window.addEventListener("panes-changed", () => {
    if (paneOpen("tests") && !state.loaded) {
      loadTests();
    } else {
      draw();
    }
  });
}
