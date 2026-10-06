// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The run view: every job the catalog reads from the tree, the values each takes, and the output of
// each run as it arrives.

import { invoke, listen, pick } from "./bridge.js";

import { drawLattice } from "./lattice.js";
import { write } from "./status.js";

// The groups in the order the engine's own steps run, and then what reads its results.
const ORDER = ["build", "protocol", "ingest", "run", "render", "sim", "view", "pipeline", "stage", "test"];

// The lines a run keeps. Past this the oldest go. A run that prints without end cannot fill memory.
const KEPT_LINES = 20000;

const state = {
  jobs: [],
  chosen: null,
  runs: new Map(),
  shown: new Map(),
  openFile: () => {},
  // Lines and ends that arrive before job_start has returned the run they belong to.
  early: new Map(),
};

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

function subject(job) {
  return job.file.split("/")[1] || job.file;
}

function drawList() {
  const list = document.getElementById("jobs");
  const query = document.getElementById("job-filter").value.trim().toLowerCase();
  const kept = state.jobs.filter((job) => !query || `${job.id} ${job.about}`.toLowerCase().includes(query));
  list.replaceChildren();
  for (const group of ORDER) {
    const jobs = kept.filter((job) => job.group === group);
    if (!jobs.length) {
      continue;
    }
    const block = element("details", { className: "group", open: Boolean(query) || group !== "stage" });
    block.append(element("summary", {}, group, element("span", { className: "count", textContent: String(jobs.length) })));
    if (group === "stage") {
      const subjects = [...new Set(jobs.map(subject))];
      for (const name of subjects) {
        const inner = element("details", { className: "group", open: Boolean(query) });
        const own = jobs.filter((job) => subject(job) === name);
        inner.append(element("summary", {}, name, element("span", { className: "count", textContent: String(own.length) })));
        own.forEach((job) => inner.append(item(job)));
        block.append(inner);
      }
    } else {
      jobs.forEach((job) => block.append(item(job)));
    }
    list.append(block);
  }
}

function item(job) {
  const button = element("button", { className: "item", type: "button", title: job.id });
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
    const canvas = element("canvas", { className: "lattice" });
    const tree = document.getElementById("tree-path").textContent;
    const body = element("div", { className: "empty-body" }, element("h1", { textContent: "orior" }), element("p", { textContent: tree }));
    stage.replaceChildren(element("div", { className: "empty" }, canvas, body));
    requestAnimationFrame(() => drawLattice(canvas));
    return;
  }
  const values = remembered(job.id);
  const head = element("div", { className: "job-head" });
  head.append(element("h2", { textContent: job.title }));
  const link = element("a", { textContent: job.file, tabIndex: 0 });
  link.addEventListener("click", () => state.openFile(job.file));
  head.append(element("div", { className: "file" }, link));
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

  stage.replaceChildren(head, job.params.length ? params : null, actions, console_(job));
  stage.querySelectorAll("input, select").forEach((node) => node.addEventListener("input", () => remember(job.id, valuesFrom(job))));
}

// Starts a job and keeps its run, from the Start button or from anywhere else in the app. The run
// shows in the job's console and in the status block either way.
export async function startJob(id, values) {
  const run = await invoke("job_start", { job: id, values });
  const before = early(run);
  state.early.delete(run);
  state.runs.set(run, { run, job: id, lines: before.lines, done: false, code: null, views: [], stopped: false });
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

function console_(job) {
  const runs = runsOf(job.id);
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
  requestAnimationFrame(() => (lines.scrollTop = lines.scrollHeight));
  return element("div", { className: "console" }, runs.length ? head : null, lines);
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

function lineNode(line) {
  return element("span", { className: line.stream, textContent: `${line.text}\n` });
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
  if (run.lines.length > KEPT_LINES) {
    run.lines.splice(0, run.lines.length - KEPT_LINES);
  }
  if (state.chosen === run.job && (state.shown.get(run.job) ?? run.run) === run.run) {
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
  Object.assign(run, { done: true, code: payload.code, stopped: payload.stopped, views: payload.views });
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

export async function startRun(openFile) {
  state.openFile = openFile;
  await listen("run-line", onLine);
  await listen("run-end", onEnd);
  document.getElementById("job-filter").addEventListener("input", drawList);
}

export async function loadRun() {
  state.jobs = await invoke("catalog_read");
  if (state.chosen && !state.jobs.some((job) => job.id === state.chosen)) {
    state.chosen = null;
  }
  drawList();
  drawStage();
}
