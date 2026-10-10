// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The run view: every job the catalog reads from the tree, the values each takes, and the output of
// each run as it arrives.

import { invoke, listen, pick } from "./bridge.js";

import { makeFuse } from "./fuse.js";
import { makeRuler } from "./ruler.js";
import { keepLattice } from "./lattice.js";
import { focusedKey, keepListKeys, refocus } from "./lists.js";
import { copyText, menuOn } from "./menu.js";
import { coloredHtml } from "./screen.js";
import { write } from "./status.js";
import { wordmark } from "./wordmark.js";

// The groups in the order the engine's own steps run, and then what reads its results.
const ORDER = ["build", "protocol", "ingest", "run", "render", "sim", "view", "pipeline", "stage", "test"];

// The lines a run keeps. Past this the oldest go. A run that prints without end cannot fill memory.
const KEPT_LINES = 20000;

// The runs a job keeps, the newest. Past this the oldest that have ended go, lines and all. A job
// run again and again cannot fill memory either.
const KEPT_RUNS = 10;

const state = {
  jobs: [],
  chosen: null,
  runs: new Map(),
  shown: new Map(),
  openFile: () => {},
  // Lines and ends that arrive before job_start has returned the run they belong to.
  early: new Map(),
  // The fuse at the foot of the output, kept from one drawing of the stage to the next.
  fuse: makeFuse(() => state.ruler.headAt()),
  // Which groups of the list the reader opened or closed, by key.
  opened: new Map(),
};

// The time ruler under the fuse, kept as the fuse is.
state.ruler = makeRuler(state.fuse.canvas);

function early(run) {
  if (!state.early.has(run)) {
    state.early.set(run, { lines: [], end: null });
  }
  return state.early.get(run);
}

const remembered = (id) => JSON.parse(localStorage.getItem(`orior.values.${id}`) || "{}");
const remember = (id, values) => localStorage.setItem(`orior.values.${id}`, JSON.stringify(values));

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function runsOf(id) {
  return [...state.runs.values()].filter((run) => run.job === id);
}

function liveOf(id) {
  return runsOf(id).some((run) => !run.done);
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

function drawList() {
  const list = document.getElementById("jobs");
  const focused = focusedKey(list);
  const query = document.getElementById("job-filter").value.trim().toLowerCase();
  const kept = state.jobs.filter((job) => !query || `${job.id} ${job.about}`.toLowerCase().includes(query));
  list.replaceChildren();
  for (const group of ORDER) {
    const jobs = kept.filter((job) => job.group === group);
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

async function pickInto(param, input) {
  const given = await pick(param.kind);
  if (typeof given === "string") {
    input.value = given;
    input.dispatchEvent(new Event("input"));
  }
}

function field(job, param, values) {
  const id = `param-${param.key}`;
  const label = element("label", { htmlFor: id, textContent: param.key, className: param.required ? "needed" : "" });
  const given = values[param.key] ?? (param.default ? [param.default] : []);
  let control;
  if (param.kind === "choice") {
    control = element("select", { id });
    if (!param.required) {
      control.append(element("option", { value: "", textContent: "" }));
    }
    param.choices.forEach((value) => control.append(element("option", { value, textContent: value })));
    control.value = given[0] ?? "";
  } else if (param.kind === "many") {
    control = element("div", { className: "choices", id });
    for (const value of param.choices) {
      const box = element("input", { type: "checkbox", value, checked: given.includes(value) });
      control.append(element("label", {}, box, value));
    }
  } else {
    const input = element("input", { id, type: "text", value: given[0] ?? "", spellcheck: false });
    if (param.kind === "choice-or-file") {
      const list = element("datalist", { id: `${id}-list` });
      param.choices.forEach((value) => list.append(element("option", { value })));
      input.setAttribute("list", list.id);
      control = element("div", { className: "pick" }, input, list, pickButton(param, input));
    } else if (["file", "dir", "save", "save-dir"].includes(param.kind)) {
      control = element("div", { className: "pick" }, input, pickButton(param, input));
    } else {
      control = input;
    }
  }
  return [label, control];
}

function pickButton(param, input) {
  const button = element("button", { className: "secondary", type: "button", textContent: "Open" });
  button.addEventListener("click", () => pickInto(param, input));
  return button;
}

function valuesFrom(job) {
  const values = {};
  for (const param of job.params) {
    const node = document.getElementById(`param-${param.key}`);
    if (!node) {
      continue;
    }
    if (param.kind === "many") {
      values[param.key] = [...node.querySelectorAll("input:checked")].map((box) => box.value);
    } else {
      const input = node.matches("input, select") ? node : node.querySelector("input");
      values[param.key] = input.value.trim() ? [input.value.trim()] : [];
    }
  }
  return values;
}

function drawStage() {
  const stage = document.getElementById("job-stage");
  const job = state.jobs.find((one) => one.id === state.chosen);
  if (!job) {
    // An empty stage drawn again stays as it is, its name not shown a second time.
    if (stage.querySelector(":scope > .empty")) {
      return;
    }
    const canvas = element("canvas", { className: "lattice" });
    const body = element("div", { className: "empty-body" }, wordmark("h1"));
    stage.replaceChildren(element("div", { className: "empty" }, canvas, body));
    keepLattice(canvas);
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
    const run = state.shown.get(job.id);
    if (run !== undefined) {
      invoke("job_stop", { run }).catch((error) => (said.textContent = String(error)));
    }
  });
  stop.disabled = !liveOf(job.id);

  stage.replaceChildren(...[head, job.params.length ? params : null, actions, console_(job)].filter(Boolean));
  stage.querySelectorAll("input, select").forEach((node) => node.addEventListener("input", () => remember(job.id, valuesFrom(job))));
}

// Starts a job and keeps its run, from the Start button or from anywhere else in the app. The run
// shows in the job's console and in the status block either way.
export async function startJob(id, values) {
  const run = await invoke("job_start", { job: id, values });
  const before = early(run);
  state.early.delete(run);
  // `started` counts the steps begun, each of which writes its command line first, and `steps` holds
  // when each began. `clock` is the page's time when the run started, as near as its lines place it.
  const steps = before.lines.filter((line) => line.stream === "command").map((line) => line.ms);
  const clock = performance.now() - (before.lines[before.lines.length - 1]?.ms ?? 0);
  state.runs.set(run, { run, job: id, lines: before.lines, started: steps.length, steps, clock, endMs: null, done: false, code: null, views: [], stopped: false });
  const kept = runsOf(id);
  for (const old of kept.filter((one) => one.done).slice(0, Math.max(0, kept.length - KEPT_RUNS))) {
    state.runs.delete(old.run);
  }
  write("runs", [run, { job: id }]);
  state.shown.set(id, run);
  if (before.end) {
    onEnd({ payload: before.end });
    return run;
  }
  drawList();
  if (state.chosen === id) {
    drawStage();
  }
  return run;
}

// A job's output, which a job that has not run yet has none of.
function console_(job) {
  const runs = runsOf(job.id);
  if (!runs.length) {
    state.fuse.follow(null);
    state.ruler.follow(null);
    return null;
  }
  const shown = state.runs.get(state.shown.get(job.id)) ?? runs[runs.length - 1];
  const head = element("div", { className: "console-head" });
  for (const run of runs) {
    const tab = element("button", { className: `run ${endClass(run)}`, type: "button", textContent: `#${run.run} ${status(run)}` });
    if (run === shown) {
      tab.setAttribute("aria-current", "true");
    }
    tab.addEventListener("click", () => {
      state.shown.set(job.id, run.run);
      drawStage();
    });
    head.append(tab);
  }
  if (shown?.views.length) {
    const views = element("div", { className: "views" });
    for (const page of shown.views) {
      const button = element("button", { type: "button", textContent: page.split("/").pop(), title: page });
      button.addEventListener("click", () => invoke("view_open", { path: page }));
      views.append(button);
    }
    head.append(views);
  }
  const lines = element("pre", { className: "lines", id: "lines" });
  shown?.lines.forEach((line) => lines.append(lineNode(line)));
  if (shown?.done) {
    lines.append(endNode(shown));
  }
  requestAnimationFrame(() => {
    lines.scrollTop = lines.scrollHeight;
    state.fuse.follow(shown ?? null);
    state.ruler.follow(shown ?? null);
  });
  return element("div", { className: "console" }, head, lines, state.ruler.element);
}

function status(run) {
  if (!run.done) {
    return "running";
  }
  if (run.stopped) {
    return "stopped";
  }
  return run.code === null ? "ended" : `exit ${run.code}`;
}

function endClass(run) {
  if (!run.done) {
    return "";
  }
  return run.code === 0 && !run.stopped ? "ok" : "failed";
}

// A line of output, in the colors its escape sequences ask for.
function lineNode(line) {
  if (!line.text.includes("\x1b")) {
    return element("span", { className: line.stream, textContent: `${line.text}\n` });
  }
  return element("span", { className: line.stream, innerHTML: `${coloredHtml(line.text)}\n` });
}

function endNode(run) {
  return element("span", { className: `end ${endClass(run)}`, textContent: status(run) });
}

function onLine({ payload }) {
  const run = state.runs.get(payload.run);
  if (!run) {
    early(payload.run).lines.push(payload);
    return;
  }
  run.lines.push(payload);
  run.clock = Math.min(run.clock, performance.now() - payload.ms);
  if (payload.stream === "command") {
    run.started += 1;
    run.steps.push(payload.ms);
  }
  if (run.lines.length > KEPT_LINES) {
    run.lines.splice(0, run.lines.length - KEPT_LINES);
  }
  if (state.chosen === run.job && (state.shown.get(run.job) ?? run.run) === run.run) {
    state.fuse.flare();
    state.ruler.wake();
    const lines = document.getElementById("lines");
    if (lines) {
      const atEnd = lines.scrollHeight - lines.scrollTop - lines.clientHeight < 40;
      lines.append(lineNode(payload));
      if (lines.childNodes.length > KEPT_LINES) {
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
    early(payload.run).end = payload;
    return;
  }
  Object.assign(run, { done: true, code: payload.code, stopped: payload.stopped, views: payload.views, endMs: payload.ms });
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

export function stopChosen() {
  runsOf(state.chosen)
    .filter((run) => !run.done)
    .forEach((run) => invoke("job_stop", { run: run.run }).catch(() => {}));
}

export async function startRun(openFile) {
  state.openFile = openFile;
  await listen("run-line", onLine);
  await listen("run-end", onEnd);
  document.getElementById("job-filter").addEventListener("input", drawList);
  keepListKeys(document.getElementById("jobs"), document.getElementById("job-filter"));
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
  return [
    {
      label: "Start",
      run: () => {
        choose(job.id);
        document.querySelector("#job-stage .actions .primary")?.click();
      },
    },
    { label: "Stop", disabled: !live.length, run: () => live.forEach((run) => invoke("job_stop", { run: run.run }).catch(() => {})) },
    "-",
    { label: "Edit", run: () => state.openFile(job.file) },
    { label: "Copy path", run: () => copyText(job.file) },
  ];
}

// The output's menu: copy what is chosen in it, or all of it, or choose all of it.
function outputItems(event) {
  const lines = event.target.closest(".lines");
  if (!lines) {
    return null;
  }
  const selection = window.getSelection();
  const chosen = selection && !selection.isCollapsed && lines.contains(selection.anchorNode) ? selection.toString() : "";
  return [
    { label: "Copy", keys: "Ctrl+C", disabled: !chosen, run: () => copyText(chosen) },
    { label: "Copy all", run: () => copyText(lines.textContent) },
    "-",
    { label: "Select all", run: () => window.getSelection().selectAllChildren(lines) },
  ];
}

// Reads again, from its first line, each run kept on the tree's machine that this window has not
// seen end: a run going there when the window opened, and one whose lines stopped coming when the
// link to the machine dropped. A tree on this machine keeps none.
export async function readRunsAgain() {
  const kept = await invoke("runs_kept").catch(() => []);
  for (const one of kept) {
    const known = state.runs.get(one.run);
    if (known?.done || (!known && one.ended)) {
      continue;
    }
    state.runs.set(one.run, { run: one.run, job: one.job, lines: [], started: 0, steps: [], clock: performance.now(), endMs: null, done: false, code: null, views: [], stopped: false });
    if (!state.shown.has(one.job)) {
      state.shown.set(one.job, one.run);
    }
    write("runs", [one.run, { job: one.job }]);
    await invoke("run_follow", { run: one.run }).catch(() => {});
  }
  if (kept.length) {
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
  drawStage();
}
