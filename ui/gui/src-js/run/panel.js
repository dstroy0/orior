// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A protocol run's panel: its line of nodes with the fuse burning along it, the one line said under it, its tabs
// over the console, the reading of its database, and the plan and the course its console says.

import { invoke } from "../bridge.js";
import { courseLine, courseRead, courseStart, panelRead, panelReplanned, panelStart, panelStep, planOf, spanSaid, telemetryRead, telemetryStart, telemetryStep } from "../protocol.js";
import { PROTOCOL, drawStage, element, shownRun, state } from "../run.js";
import { endNode, lineNode } from "./output.js";

// How far the fuse has burnt for the run shown: a protocol run's place along its line, the ruler's head
// for any other run.
export function fusePlace() {
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
  const errors = [["blocked", read.blocked], ["refused", read.refused], ["faulted", read.faulted], ["panics", read.panics]].filter(([, count]) => count);
  if (errors.length) {
    box.append(element("span", { className: "warn", textContent: errors.map(([name, count]) => `${count} ${name}`).join(" \u00b7 ") }));
  }
  // each cellblock's two peaks against its pool, a warning where they stand at nine tenths of it or more together
  for (const cellblock of read.cells) {
    const mib = (bytes) => (bytes / 1048576).toFixed(1);
    const close = (cellblock.persistent + cellblock.temporary) * 10 >= cellblock.size * 9;
    const said = `${cellblock.name} ${mib(cellblock.persistent)} + ${mib(cellblock.temporary)} of ${mib(cellblock.size)} MiB`;
    box.append(element("span", { className: close ? "warn" : "", textContent: said }));
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
export const CONSOLE = "console";
export function tabStrip(run) {
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
export function viewBody(run) {
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

// A run record as it starts: its console lines and, for a protocol run, the reading of its database, the
// .qry its writer names and where the next read of it goes on from.
export function runRecord(run, job, lines, steps, clock) {
  return { run, job, lines, started: steps.length, steps, clock, endMs: null, done: false, code: null, views: [], made: null, stopped: false, reading: panelStart(), plan: null, pass: "", tools: false, qry: null, next: 0, readUnderWay: false, readAgain: false, view: OVERVIEW, telemetry: telemetryStart(), reachedAt: {}, course: courseStart() };
}

// The run read where its writer wrote it: its buffer through qry_read, from where the last read stopped, each blob
// taken into the panel's reading in the order the writer placed it. A read comes as a console line of the run comes
// and once more at its end; one asked for while another is under way is made as that one ends.
export async function runRead(run) {
  if (!run.qry) {
    return;
  }
  if (run.readUnderWay) {
    run.readAgain = true;
    return;
  }
  run.readUnderWay = true;
  do {
    run.readAgain = false;
    const read = await invoke("qry_read", { path: run.qry, from: run.next }).catch(() => null);
    if (read) {
      run.next = read.next;
      for (const blob of read.blobs) {
        panelStep(run.reading, blob, { plan: run.plan });
        telemetryStep(run.telemetry, blob, performance.now() - run.clock);
      }
      panelRedrawn(run);
    }
  } while (run.readAgain);
  run.readUnderWay = false;
}

// The panel drawn again in place for the run shown, as a blob or the plan comes: once a frame however
// many came, and only where what it shows has changed, so that it stands still between changes.
let panelFrame = 0;
export function panelRedrawn(run) {
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
export function planNoted(run, line) {
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
    // the pass's name off the command line, `orior_gui protocol learn <pass> ...`, for the line before the plan comes
    const words = line.text.split(/\s+/);
    const at = words.indexOf("learn");
    run.pass = at >= 0 && words[at + 1] ? words[at + 1] : words[words.indexOf("protocol") + 1] ?? "";
    panelRedrawn(run);
  }
  // the writer naming the run's database: the tools are built, and the writer's blobs come from here on
  const made = /^\s*orior_qry_writer: the run of (.+) is made\s*$/.exec(line.text);
  if (made) {
    run.tools = true;
    run.qry = made[1];
    panelRedrawn(run);
  }
}
