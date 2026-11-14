// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The run view: every job the catalog reads from the tree, the values each takes, and the output of
// each run as it arrives.

import { invoke, listen } from "./bridge.js";

import { makeFuse } from "./fuse.js";
import { telemetryLine, valuesOfWords } from "./protocol.js";
import { makeRuler } from "./ruler.js";
import { focusedKey, retainListKeys, refocus } from "./lists.js";
import { copyText, menuOn } from "./menu.js";
import { field, remember, remembered, valuesFrom } from "./run/form.js";
import { chosenIn, console_, lineNode, outputItems } from "./run/output.js";
import { CONSOLE, fusePlace, panelRedrawn, planNoted, runRead, runRecord } from "./run/panel.js";
import { write } from "./status.js";
import { wordmark } from "./wordmark.js";
import { showView } from "./views.js";

// The groups in the order the engine's own steps run, and then what reads its results.
export const ORDER = ["build", "executable", "deploy", "protocol", "ingest", "run", "render", "sim", "view", "pipeline", "stage", "test"];

// The lines a run retains. Past this the oldest go. A run that prints without end cannot fill memory.
const RETAINED_LINES = 20000;

// The runs a job retains, the newest. Past this the oldest that have ended go, lines and all. A job
// run again and again cannot fill memory either.
const RETAINED_RUNS = 10;

export const state = {
  jobs: [],
  chosen: null,
  // The one group of jobs the list shows, chosen from the strip; null before any is.
  category: null,
  runs: new Map(),
  displayed: new Map(),
  openFile: () => {},
  // The fuse at the foot of the output, retained from one drawing of the stage to the next: a protocol
  // run burns from objective to objective, every other run by the ruler.
  fuse: makeFuse(() => fusePlace()),
  // Which groups of the list the reader opened or closed, by key.
  opened: new Map(),
};

// The time ruler under the fuse, retained as the fuse is.
state.ruler = makeRuler(state.fuse.canvas);

// The protocol jobs, whose runs show in the panel.
export const PROTOCOL = "protocol";

// A run's start, said before its first line: its record made here, so that every line finds it.
function onStart({ payload }) {
  if (!state.runs.has(payload.run)) {
    state.runs.set(payload.run, runRecord(payload.run, payload.job, [], [], performance.now()));
    drawList();
  }
}

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function runsOf(id) {
  return [...state.runs.values()].filter((run) => run.job === id);
}

function liveOf(id) {
  return runsOf(id).some((run) => !run.done);
}

// The run on the stage: the one the reader chose of the chosen job's runs, else its newest, or null
// where no job is chosen or it has not run.
export function displayedRun() {
  if (state.chosen === null) {
    return null;
  }
  const chosen = state.displayed.get(state.chosen);
  const run = chosen !== undefined ? state.runs.get(chosen) : undefined;
  return run ?? runsOf(state.chosen).at(-1) ?? null;
}

// What a stage job works on, which the stage group is split by.
export function subject(job) {
  return job.file.split("/")[1] || job.file;
}

// A group of the list, open as the reader left it, else open where a search is under way or where
// it is not the stage group, which is long.
function groupOf(key, label, count, depth, query, open) {
  const block = element("details", { className: "group", open: Boolean(query) || (state.opened.get(key) ?? open) });
  block.dataset.depth = String(depth);
  const summary = element("summary", {}, label, element("span", { className: "count", textContent: String(count) }));
  summary.dataset.key = key;
  block.append(summary);
  block.addEventListener("toggle", () => !query && state.opened.set(key, block.open));
  return block;
}

// The protocol's one job: the course, the whole protocol in order. Its other commands stay on the command
// line and in the menus, and the list shows none of them.
const COURSE = "protocol/course";

// The jobs the list shows: the category chosen from the strip, every group where none is yet, and of the
// protocol group the course alone.
function listed(group) {
  return state.jobs.filter((job) => job.group === group && (group !== PROTOCOL || job.id === COURSE));
}

// Shows the run view with `group` alone in the list, its first job chosen where the chosen one is not its.
export function showCategory(group) {
  state.category = group;
  showView("run");
  const own = listed(group);
  if (own.length && !own.some((job) => job.id === state.chosen)) {
    state.chosen = own[0].id;
  }
  drawList();
  drawStage();
}

// The group the list shows now, for the strip to mark.
export function displayedCategory() {
  return state.category;
}

function drawList() {
  const list = document.getElementById("jobs");
  const focused = focusedKey(list);
  const query = document.getElementById("job-filter").value.trim().toLowerCase();
  const retained = state.jobs.filter((job) => !query || `${job.id} ${job.about}`.toLowerCase().includes(query));
  list.replaceChildren();
  for (const group of ORDER) {
    if (state.category && group !== state.category && !query) {
      continue;
    }
    const jobs = retained.filter((job) => listed(group).includes(job));
    if (!jobs.length) {
      continue;
    }
    const block = groupOf(group, group, jobs.length, 0, query, group !== "stage");
    if (group === "stage") {
      const subjects = [...new Set(jobs.map(subject))];
      for (const name of subjects) {
        const own = jobs.filter((job) => subject(job) === name);
        const inner = groupOf(`${group}/${name}`, name, own.length, 1, query, false);
        own.forEach((job) => inner.append(item(job)));
        block.append(inner);
      }
    } else {
      jobs.forEach((job) => block.append(item(job)));
    }
    list.append(block);
  }
  refocus(list, focused);
}

function item(job) {
  const button = element("button", { className: "item", type: "button", title: job.id });
  button.dataset.key = job.id;
  if (liveOf(job.id)) {
    button.append(element("span", { className: "live" }));
  }
  button.append(job.title);
  if (state.chosen === job.id) {
    button.setAttribute("aria-current", "true");
  }
  button.addEventListener("click", () => choose(job.id));
  return button;
}

export function drawStage() {
  const stage = document.getElementById("job-stage");
  const job = state.jobs.find((one) => one.id === state.chosen);
  if (!job) {
    // An empty stage drawn again stays as it is, its name not displayed a second time.
    if (stage.querySelector(":scope > .empty")) {
      return;
    }
    const body = element("div", { className: "empty-body" }, wordmark("h1"));
    stage.replaceChildren(element("div", { className: "empty" }, body));
    return;
  }
  const values = remembered(job.id);
  // A job named for its file has one line for both, and the name opens the file.
  const head = element("div", { className: "job-head" });
  const link = element("a", { textContent: job.file, tabIndex: 0, title: job.file });
  link.addEventListener("click", () => state.openFile(job.file));
  link.addEventListener("keydown", (event) => event.key === "Enter" && state.openFile(job.file));
  if (job.title === job.file) {
    link.className = "named";
    head.append(element("h2", {}, link));
  } else {
    head.append(element("h2", { textContent: job.title }), element("div", { className: "file" }, link));
  }
  if (job.about) {
    head.append(element("p", { textContent: job.about }));
  }
  const params = element("div", { className: "params" });
  job.params.forEach((param) => params.append(...field(job, param, values)));

  const start = element("button", { className: "primary", type: "button", textContent: "Start" });
  const stop = element("button", { className: "secondary", type: "button", textContent: "Stop" });
  const said = element("span", { className: "said" });
  const actions = element("div", { className: "actions" }, start, stop, said);
  start.addEventListener("click", async () => {
    said.textContent = "";
    const given = valuesFrom(job);
    remember(job.id, given);
    try {
      await startJob(job.id, given);
    } catch (error) {
      said.textContent = String(error);
    }
  });
  stop.addEventListener("click", () => {
    const run = state.displayed.get(job.id);
    if (run !== undefined) {
      invoke("job_stop", { run }).catch((error) => (said.textContent = String(error)));
    }
  });
  stop.disabled = !liveOf(job.id);

  stage.replaceChildren(...[head, job.params.length ? params : null, actions, console_(job)].filter(Boolean));
  stage.querySelectorAll("input, select").forEach((node) => node.addEventListener("input", () => remember(job.id, valuesFrom(job))));
}

// The groups whose jobs build.
const BUILDS = new Set(["build", "executable"]);

// Starts a job and retains its run, from the Start button or from anywhere else in the app. The run
// shows in the job's console and in the status block either way, and a build's in the build window:
// the run view, its job chosen, wherever it was started from.
export async function startJob(id, values) {
  const run = await invoke("job_start", { job: id, values });
  // the run's start was said before this answer came, and its record is; one is made here where it was not
  onStart({ payload: { run, job: id } });
  const retained = runsOf(id);
  for (const old of retained.filter((one) => one.done).slice(0, Math.max(0, retained.length - RETAINED_RUNS))) {
    state.runs.delete(old.run);
  }
  write("runs", [run, { job: id }]);
  state.displayed.set(id, run);
  if (BUILDS.has(state.jobs.find((job) => job.id === id)?.group)) {
    showView("run");
    if (state.chosen !== id) {
      showJob(id);
    }
  }
  drawList();
  if (state.chosen === id) {
    drawStage();
  }
  return run;
}

function onLine({ payload }) {
  const run = state.runs.get(payload.run);
  if (!run) {
    return;
  }
  run.lines.push(payload);
  planNoted(run, payload);
  telemetryLine(run.telemetry, payload.text);
  panelRedrawn(run);
  runRead(run);
  run.clock = Math.min(run.clock, performance.now() - payload.ms);
  if (payload.stream === "command") {
    run.started += 1;
    run.steps.push(payload.ms);
  }
  if (run.lines.length > RETAINED_LINES) {
    run.lines.splice(0, run.lines.length - RETAINED_LINES);
  }
  if (state.chosen === run.job && (state.displayed.get(run.job) ?? run.run) === run.run) {
    state.fuse.flare();
    state.ruler.wake();
    const lines = document.getElementById("lines");
    if (lines && run.view === CONSOLE) {
      const atEnd = lines.scrollHeight - lines.scrollTop - lines.clientHeight < 40;
      lines.append(lineNode(payload));
      if (lines.childNodes.length > RETAINED_LINES) {
        lines.firstChild.remove();
      }
      if (atEnd) {
        lines.scrollTop = lines.scrollHeight;
      }
    }
  }
}

function onEnd({ payload }) {
  const run = state.runs.get(payload.run);
  if (!run) {
    return;
  }
  runRead(run);
  Object.assign(run, { done: true, code: payload.code, stopped: payload.stopped, views: payload.views, made: payload.made ?? null, endMs: payload.ms });
  write("runs", [payload.run, null]);
  drawList();
  if (state.chosen === run.job) {
    drawStage();
  }
}

function choose(id) {
  state.chosen = id;
  drawList();
  drawStage();
}

// The jobs as the catalog lists them, for the menu bar.
export function listedJobs() {
  return state.jobs;
}

// Shows a job in the run view, its form ready, and the list scrolled to it.
export function showJob(id) {
  choose(id);
  document.querySelector(`#jobs .item[data-key="${CSS.escape(id)}"]`)?.scrollIntoView({ block: "nearest" });
}

export function chosenJob() {
  return state.jobs.find((job) => job.id === state.chosen) ?? null;
}

export function chosenLive() {
  return state.chosen !== null && liveOf(state.chosen);
}

// Starts the chosen job as its Start button does, with what its form holds.
export function startChosen() {
  document.querySelector("#job-stage .actions .primary")?.click();
}

// Every live run stopped, whichever job it is of, and how many were: what a test harness leaving the page asks, so
// that nothing it started runs on after it.
export async function stopLive() {
  const live = [...state.runs.values()].filter((run) => !run.done);
  await Promise.all(live.map((run) => invoke("job_stop", { run: run.run }).catch(() => {})));
  return live.length;
}

export function stopChosen() {
  runsOf(state.chosen)
    .filter((run) => !run.done)
    .forEach((run) => invoke("job_stop", { run: run.run }).catch(() => {}));
}

export async function startRun(openFile) {
  state.openFile = openFile;
  await listen("run-start", onStart);
  await listen("run-line", onLine);
  // the lines a run said between two of the server's readings come as one event, so that a run saying thousands a
  // second leaves the page's event queue open to every other event, the command line's among them
  await listen("run-lines", ({ payload }) => payload.forEach((said) => onLine({ payload: said })));
  await listen("run-end", onEnd);
  document.getElementById("job-filter").addEventListener("input", drawList);
  retainListKeys(document.getElementById("jobs"), document.getElementById("job-filter"));
  menuOn(document.getElementById("jobs"), jobItems);
  menuOn(document.getElementById("job-stage"), outputItems);
}

// A job's menu: start it as its Start button would, with what its form holds, stop its runs, open
// its file, or copy where the file is.
function jobItems(event) {
  const job = state.jobs.find((one) => one.id === event.target.closest(".item")?.dataset.key);
  if (!job) {
    return null;
  }
  const live = runsOf(job.id).filter((run) => !run.done);
  const act = (command, label, more = {}) => ({ label, command, args: [job.id], run: () => runCommands[command]([job.id]), ...more });
  return [act("job-start", "Start"), act("job-stop", "Stop", { disabled: !live.length }), "-", act("job-edit", "Edit"), act("job-copy-path", "Copy path")];
}

// The jobs' menu and their output's as commands, by the names commands.json gives them, each run
// from the menus or from the command line with a job's id.
const jobNamed = (id) => state.jobs.find((one) => one.id === id) ?? null;
const displayedLines = () => document.getElementById("lines");

export const runCommands = {
  "job-start": ([id]) => {
    if (jobNamed(id)) {
      showView("run");
      choose(id);
      document.querySelector("#job-stage .actions .primary")?.click();
    }
  },
  "job-stop": ([id]) => runsOf(id).filter((run) => !run.done).forEach((run) => invoke("job_stop", { run: run.run }).catch(() => {})),
  "job-edit": ([id]) => jobNamed(id) && state.openFile(jobNamed(id).file),
  "job-copy-path": ([id]) => jobNamed(id) && copyText(jobNamed(id).file),
  "output-copy": () => {
    const chosen = chosenIn(displayedLines());
    return chosen && copyText(chosen);
  },
  "output-copy-all": ([id]) => {
    if (id && jobNamed(id)) {
      showView("run");
      choose(id);
    }
    return displayedLines() && copyText(displayedLines().textContent);
  },
  "output-select-all": () => displayedLines() && window.getSelection().selectAllChildren(displayedLines()),
};

// Reads again, from its first line, each run retained on the tree's machine that this window has not
// seen end: a run going there when the window opened, and one whose lines stopped coming when the
// link to the machine dropped. A tree on this machine retains none.
export async function readRunsAgain() {
  const retained = await invoke("runs_detached").catch(() => []);
  for (const one of retained) {
    const known = state.runs.get(one.run);
    if (known?.done || (!known && one.ended)) {
      continue;
    }
    state.runs.set(one.run, runRecord(one.run, one.job, [], [], performance.now()));
    if (!state.displayed.has(one.job)) {
      state.displayed.set(one.job, one.run);
    }
    write("runs", [one.run, { job: one.job }]);
    await invoke("run_follow", { run: one.run }).catch(() => {});
  }
  if (retained.length) {
    drawList();
    drawStage();
  }
}

export async function loadRun() {
  state.jobs = await invoke("catalog_read");
  if (state.chosen && !state.jobs.some((job) => job.id === state.chosen)) {
    state.chosen = null;
  }
  drawList();
  window.dispatchEvent(new Event("jobs-changed"));
  drawStage();
}

// A job started from a command line's words, `tab protocol learn costs --leave all` as the window hears it: the words
// read into the job's values as its fields would hold them, the job displayed and started.
export async function startJobWords(id, words) {
  const job = state.jobs.find((one) => one.id === id);
  if (!job) {
    throw new Error(`the window has no job ${id}`);
  }
  const values = valuesOfWords(job.params, words);
  remember(id, values);
  showView("run");
  showJob(id);
  return startJob(id, values);
}
