// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The run view: every job the catalog reads from the tree, the values each takes, and the output of
// each run as it arrives.

import { invoke, listen, pick } from "./bridge.js";

import { makeFuse } from "./fuse.js";
import { courseLine, courseRead, courseStart, panelCounted, panelRead, panelReplanned, panelStart, panelStep, planOf, spanSaid, telemetryLine, telemetryRead, telemetryStart, telemetryStep, valuesOfWords } from "./protocol.js";
import { makeRuler } from "./ruler.js";
import { keepLattice } from "./lattice.js";
import { focusedKey, keepListKeys, refocus } from "./lists.js";
import { copyText, menuOn } from "./menu.js";
import { coloredHtml } from "./screen.js";
import { write } from "./status.js";
import { say } from "./statusbar.js";
import { runInTerminal } from "./terminal.js";
import { wordmark } from "./wordmark.js";
import { showView } from "./views.js";

// The groups in the order the engine's own steps run, and then what reads its results.
export const ORDER = ["build", "executable", "deploy", "protocol", "ingest", "run", "render", "sim", "view", "pipeline", "stage", "test"];

// The lines a run keeps. Past this the oldest go. A run that prints without end cannot fill memory.
const KEPT_LINES = 20000;

// The runs a job keeps, the newest. Past this the oldest that have ended go, lines and all. A job
// run again and again cannot fill memory either.
const KEPT_RUNS = 10;

const state = {
  jobs: [],
  chosen: null,
  // The one group of jobs the list shows, chosen from the strip; null before any is.
  category: null,
  runs: new Map(),
  shown: new Map(),
  openFile: () => {},
  // The fuse at the foot of the output, kept from one drawing of the stage to the next: a protocol
  // run burns from objective to objective, every other run by the ruler.
  fuse: makeFuse(() => fusePlace()),
  // Which groups of the list the reader opened or closed, by key.
  opened: new Map(),
};

// The time ruler under the fuse, kept as the fuse is.
state.ruler = makeRuler(state.fuse.canvas);

// How far the fuse has burnt for the run shown: a protocol run's place along its line, the ruler's head
// for any other run.
function fusePlace() {
  const run = shownRun();
  const job = run && state.jobs.find((one) => one.id === run.job);
  return job?.group === PROTOCOL ? lineRead(run).place : state.ruler.headAt();
}

// The line of a protocol run as the panel reads it: a course's steps where the run is the course, each a
// node, the fire at the step under way part way by its pass; else the pass's own nodes. Each node has its
// label and whether it is reached, current or failed.
function lineRead(run) {
  const pass = panelRead(run.reading, shownOf(run));
  if (run.course.steps.length) {
    return courseRead(run.course, pass.place, run.done);
  }
  const nodes = pass.nodes.map((node, index) => ({ key: node.key, label: node.label, reached: node.reached, current: !run.done && index === pass.at + 1, failed: false }));
  return { nodes, place: pass.place };
}

// The line of a protocol run: its nodes, each a circle with its label, the fuse between them, which burns
// from node to node as the run goes. The fuse's canvas is the one fuse of the page, taken from the ruler's
// box while a protocol run is shown and given back after.
let lineBox = null;
function lineOf(run) {
  if (!lineBox) {
    lineBox = element("div", { className: "protocol-line" });
  }
  lineBox.replaceChildren(state.fuse.canvas, nodesOf(run));
  return lineBox;
}

function nodesOf(run) {
  const { nodes } = lineRead(run);
  const box = element("div", { className: "protocol-nodes", id: "protocol-nodes" });
  const last = nodes.length - 1;
  nodes.forEach((node, index) => {
    const state_ = node.failed ? " failed" : node.current ? " current" : node.reached ? " reached" : "";
    const one = element("div", { className: `protocol-node${state_}${index === 0 ? " first" : ""}${index === last ? " last" : ""}`, title: node.label });
    one.style.left = last > 0 ? `${(index / last) * 100}%` : "0";
    one.append(element("span", { className: "protocol-dot", textContent: node.label }));
    box.append(one);
  });
  return box;
}

// The one line said under the line: the step under way and its round, the time gone, and the errors
// where there are any.
function statusOf(run) {
  const nowMs = nowOf(run);
  const read = telemetryRead(run.telemetry, { nowMs, plan: run.plan, reading: run.reading, ended: run.done });
  const pass = panelRead(run.reading, shownOf(run));
  const parts = [];
  if (run.course.steps.length) {
    const step = run.course.at;
    parts.push(step ? `step ${step} of ${run.course.steps.length}, ${run.course.steps[step - 1]}` : `${run.course.steps.length} steps`);
  } else if (run.pass) {
    parts.push(run.pass);
  }
  if (pass.round.rounds) {
    parts.push(`round ${pass.round.rounds}, ${pass.round.answered} of ${pass.round.asked} answered`);
  }
  parts.push(spanSaid(nowMs));
  const box = element("div", { className: "protocol-status", id: "protocol-status" }, element("span", { textContent: parts.join(" \u00b7 ") }));
  const errors = [["held", read.held], ["refused", read.refused], ["faulted", read.faulted], ["panics", read.panics]].filter(([, count]) => count);
  if (errors.length) {
    box.append(element("span", { className: "warn", textContent: errors.map(([name, count]) => `${count} ${name}`).join(" \u00b7 ") }));
  }
  return box;
}

// The run's clock: milliseconds since the job started, standing still at the end once the run has ended.
function nowOf(run) {
  return run.done && run.endMs !== null ? run.endMs : performance.now() - run.clock;
}

// The views of a protocol run, as tabs over the console box: the overview, the run's line with the fire on
// it and one line said under it; and the console.
const OVERVIEW = "overview";
const CONSOLE = "console";
function tabStrip(run) {
  const strip = element("div", { className: "tabs console-tabs", id: "protocol-tabs", role: "tablist" });
  const views = [
    { key: OVERVIEW, label: "Overview", count: 0 },
    { key: CONSOLE, label: "Console", count: run.lines.length },
  ];
  for (const view of views) {
    const tab = element("button", { className: "tab", type: "button", role: "tab" });
    tab.setAttribute("aria-selected", run.view === view.key ? "true" : "false");
    tab.append(element("span", { textContent: view.label }), element("span", { className: "count", textContent: view.count ? String(view.count) : "" }));
    tab.addEventListener("click", () => {
      run.view = view.key;
      drawStage();
    });
    strip.append(tab);
  }
  return strip;
}

// The body of a protocol run's view: the line for the overview, the console's lines, or a node's blobs.
function viewBody(run) {
  if (run.view === OVERVIEW) {
    const overview = element("div", { className: "protocol-overview" }, lineOf(run), statusOf(run));
    // the clock moves, so the line said is drawn again each second while the overview stands; a run that
    // has ended stands still, drawn once more at its end and then left
    const ticker = setInterval(() => {
      const shown = document.getElementById("protocol-status");
      if (!shown || !overview.isConnected) {
        clearInterval(ticker);
        return;
      }
      shown.replaceWith(statusOf(run));
      if (run.done) {
        clearInterval(ticker);
      }
    }, 1000);
    return overview;
  }
  const lines = element("pre", { className: "lines", id: "lines" });
  run.lines.forEach((line) => lines.append(lineNode(line)));
  if (run.done) {
    lines.append(endNode(run));
  }
  return lines;
}

function shownOf(run) {
  // the tools are built by the time the writer names the run's database, and the run has started once its first blob
  // comes
  return { pass: run.pass, plan: run.plan, tools: run.tools, started: run.telemetry.firstAt !== null, ended: run.done };
}

// The protocol jobs, whose runs show in the panel.
const PROTOCOL = "protocol";

// A run record as it starts: its console lines and, for a protocol run, the reading of its database, the
// .qry its writer names and where the next read of it goes on from.
function runRecord(run, job, lines, steps, clock) {
  return { run, job, lines, started: steps.length, steps, clock, endMs: null, done: false, code: null, views: [], made: null, stopped: false, reading: panelStart(), plan: null, pass: "", tools: false, view: OVERVIEW, telemetry: telemetryStart(), reachedAt: {}, course: courseStart() };
}

// A run's start, said before its first line: its record made here, so that every line finds it.
function onStart({ payload }) {
  if (!state.runs.has(payload.run)) {
    state.runs.set(payload.run, runRecord(payload.run, payload.job, [], [], performance.now()));
    drawList();
  }
}

// A blob the run's writer placed, as the writer said it: taken into the panel's reading in the writer's order, the
// one order of the run. Nothing is read back off the run; the writer says each blob as it places it.
function onBlob(run, blob) {
  panelStep(run.reading, blob, { plan: run.plan });
  telemetryStep(run.telemetry, blob, performance.now() - run.clock);
  panelRedrawn(run);
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

// The run on the stage: the one the reader chose of the chosen job's runs, else its newest, or null
// where no job is chosen or it has not run.
function shownRun() {
  if (state.chosen === null) {
    return null;
  }
  const chosen = state.shown.get(state.chosen);
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
export function shownCategory() {
  return state.category;
}

function drawList() {
  const list = document.getElementById("jobs");
  const focused = focusedKey(list);
  const query = document.getElementById("job-filter").value.trim().toLowerCase();
  const kept = state.jobs.filter((job) => !query || `${job.id} ${job.about}`.toLowerCase().includes(query));
  list.replaceChildren();
  for (const group of ORDER) {
    if (state.category && group !== state.category && !query) {
      continue;
    }
    const jobs = kept.filter((job) => listed(group).includes(job));
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

// The groups whose jobs build.
const BUILDS = new Set(["build", "executable"]);

// Starts a job and keeps its run, from the Start button or from anywhere else in the app. The run
// shows in the job's console and in the status block either way, and a build's in the build window:
// the run view, its job chosen, wherever it was started from.
export async function startJob(id, values) {
  const run = await invoke("job_start", { job: id, values });
  // the run's start was said before this answer came, and its record stands; one is made here where it was not
  onStart({ payload: { run, job: id } });
  const kept = runsOf(id);
  for (const old of kept.filter((one) => one.done).slice(0, Math.max(0, kept.length - KEPT_RUNS))) {
    state.runs.delete(old.run);
  }
  write("runs", [run, { job: id }]);
  state.shown.set(id, run);
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
  if (shown?.made) {
    head.append(madeButtons(job, shown.made));
  }
  const protocol = job.group === PROTOCOL && shown;
  if (protocol) {
    const body = viewBody(shown);
    requestAnimationFrame(() => {
      body.scrollTop = body.scrollHeight;
      state.fuse.follow(shown);
      state.ruler.follow(shown);
    });
    return element("div", { className: "console" }, head, tabStrip(shown), body);
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
  if (!state.ruler.element.contains(state.fuse.canvas)) {
    state.ruler.element.prepend(state.fuse.canvas);
  }
  return element("div", { className: "console" }, head, lines, state.ruler.element);
}

// The panel drawn again in place for the run shown, as a blob or the plan comes: once a frame however
// many came, and only where what it shows has changed, so that it stands still between changes.
let panelFrame = 0;
function panelRedrawn(run) {
  if (panelFrame) {
    return;
  }
  panelFrame = requestAnimationFrame(() => {
    panelFrame = 0;
    const nodes = document.getElementById("protocol-nodes");
    const status_ = document.getElementById("protocol-status");
    const tabs = document.getElementById("protocol-tabs");
    if ((nodes || tabs) && state.chosen === run.job && (state.shown.get(run.job) ?? run.run) === run.run) {
      const drawn = nodesOf(run);
      if (nodes && drawn.innerHTML !== nodes.innerHTML) {
        nodes.replaceWith(drawn);
      }
      if (status_) {
        status_.replaceWith(statusOf(run));
      }
      const strip = tabStrip(run);
      if (tabs && strip.innerHTML !== tabs.innerHTML) {
        tabs.replaceWith(strip);
      }
      state.fuse.flare();
    }
  });
}

// The plan the teacher says on its console, kept on the run; the course's lines, each step of which is a
// pass of its own whose reading starts over; and the writer naming its tap, which says the run started.
function planNoted(run, line) {
  if (/^\s*course: /.test(line.text)) {
    const before = run.course.at;
    courseLine(run.course, line.text);
    if (run.course.at !== before) {
      // the step to come is a member of the course's one run, and the writer's blobs go on into a reading of its own
      run.reading = panelStart();
      run.plan = null;
      run.pass = run.course.steps[run.course.at - 1] ?? "";
    }
    panelRedrawn(run);
    return;
  }
  if (/^\s*plan: /.test(line.text)) {
    const planned = planOf(line.text);
    if (planned && !run.plan) {
      run.plan = planned;
      run.reading = panelReplanned(run.reading, planned);
    }
    run.pass = run.plan?.pass ?? run.pass;
    panelRedrawn(run);
  }
  if (line.stream === "command" && !run.pass) {
    // the pass's name off the command line, `orior protocol learn <pass> ...`, for the line before the plan comes
    const words = line.text.split(/\s+/);
    const at = words.indexOf("learn");
    run.pass = at >= 0 && words[at + 1] ? words[at + 1] : words[words.indexOf("protocol") + 1] ?? "";
    panelRedrawn(run);
  }
  // the writer naming the run's database: the tools are built, and the writer's blobs come from here on
  if (/^\s*qry_writer: the buffer of .+ is made\s*$/.test(line.text)) {
    run.tools = true;
    panelRedrawn(run);
  }
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

// A path as a POSIX shell takes it, in single quotes.
function quoted(path) {
  return `'${path.replace(/'/g, "'\\''")}'`;
}

// The program a build made, by its full path: one in the tree is named from the tree's top folder.
function madePath(made) {
  if (/^([A-Za-z]:)?[\\/]/.test(made)) {
    return made;
  }
  const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
  return `${top}/${made}`;
}

// What a build that made a program offers: the program run in the terminal at the tree's top folder;
// built and run again in the job, as orior runs it or as the system runs a program opened on its
// own; the libraries it loads through a PATH copied beside it; and its path copied.
function madeButtons(job, made) {
  const buttons = element("div", { className: "views" });
  const button = (label, act, title = made) => {
    const one = element("button", { type: "button", textContent: label, title });
    one.addEventListener("click", act);
    buttons.append(one);
  };
  button(`Run ${made.split("/").pop()}`, () => {
    const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
    runInTerminal(`( cd -- ${quoted(top)} && ${quoted(madePath(made))} )`);
  });
  const again = (then) => startJob(job.id, { ...valuesFrom(job), then: [then] }).catch((error) => say(String(error), { failed: true }));
  button("Run in the Job", () => again("run"));
  button("Run as the System Runs It", () => again("run as the system runs it"));
  button("Copy Libraries Beside It", async () => {
    try {
      const copied = await invoke("made_libraries_copy", { path: made });
      say(copied.length ? `Copied ${copied.join(", ")} beside ${made.split("/").pop()}` : `${made.split("/").pop()} loads no library through a PATH`);
    } catch (error) {
      say(String(error), { failed: true });
    }
  });
  button("Copy Path", () => copyText(madePath(made)), madePath(made));
  return buttons;
}

function endNode(run) {
  return element("span", { className: `end ${endClass(run)}`, textContent: status(run) });
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
    if (lines && run.view === CONSOLE) {
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
    return;
  }
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
  // second leaves the page's event queue open to every other event, the command line's among them; a blob the run's
  // writer placed comes among them in the order said, known by its sequence
  await listen("run-lines", ({ payload }) =>
    payload.forEach((said) => {
      if (said.sequence === undefined && said.others === undefined) {
        onLine({ payload: said });
        return;
      }
      const run = state.runs.get(said.run);
      if (!run) {
        return;
      }
      if (said.others !== undefined) {
        // the blobs the panel only counts, counted by the server in place of an event each
        panelCounted(run.reading, said.others, { plan: run.plan });
        panelRedrawn(run);
        return;
      }
      onBlob(run, said);
    }),
  );
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
  const act = (command, label, more = {}) => ({ label, command, args: [job.id], run: () => runCommands[command]([job.id]), ...more });
  return [act("job-start", "Start"), act("job-stop", "Stop", { disabled: !live.length }), "-", act("job-edit", "Edit"), act("job-copy-path", "Copy path")];
}

// The jobs' menu and their output's as commands, by the names commands.json gives them, each run
// from the menus or from the command line with a job's id.
const jobNamed = (id) => state.jobs.find((one) => one.id === id) ?? null;
const shownLines = () => document.getElementById("lines");

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
    const chosen = chosenIn(shownLines());
    return chosen && copyText(chosen);
  },
  "output-copy-all": ([id]) => {
    if (id && jobNamed(id)) {
      showView("run");
      choose(id);
    }
    return shownLines() && copyText(shownLines().textContent);
  },
  "output-select-all": () => shownLines() && window.getSelection().selectAllChildren(shownLines()),
};

// The text chosen in `lines`, as the document holds it: read from the range chosen, which gives it in a
// window without the keys, where WebKit's selection gives none.
function chosenIn(lines) {
  const selection = window.getSelection();
  return lines && selection?.rangeCount && !selection.isCollapsed && lines.contains(selection.anchorNode) ? selection.getRangeAt(0).toString() : "";
}

// The output's menu: copy what is chosen in it, or all of it, or choose all of it.
function outputItems(event) {
  const lines = event.target.closest(".lines");
  if (!lines) {
    return null;
  }
  const chosen = chosenIn(lines);
  return [
    { label: "Copy", keys: "Ctrl+C", command: "output-copy", args: [], disabled: !chosen, run: () => copyText(chosen) },
    { label: "Copy all", command: "output-copy-all", args: [], run: () => copyText(lines.textContent) },
    "-",
    { label: "Select all", command: "output-select-all", args: [], run: () => window.getSelection().selectAllChildren(lines) },
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
    state.runs.set(one.run, runRecord(one.run, one.job, [], [], performance.now()));
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
  window.dispatchEvent(new Event("jobs-changed"));
  drawStage();
}

// A job started from a command line's words, `orior protocol learn costs --leave all` as the window hears it: the words
// read into the job's values as its fields would hold them, the job shown and started.
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
