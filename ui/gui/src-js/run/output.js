// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A job's output: its runs' tabs and the console of the run displayed, what a build that made a program offers, and the
// output's menu.

import { invoke, viewOpen } from "../bridge.js";
import { copyText } from "../menu.js";
import { PROTOCOL, drawStage, element, runsOf, startJob, state } from "../run.js";
import { coloredHtml } from "../screen.js";
import { say } from "../statusbar.js";
import { runInTerminal } from "../terminal.js";
import { valuesFrom } from "./form.js";
import { tabStrip, viewBody } from "./panel.js";

// A job's output, which a job that has not run yet has none of.
export function console_(job) {
  const runs = runsOf(job.id);
  if (!runs.length) {
    state.fuse.follow(null);
    state.ruler.follow(null);
    return null;
  }
  const displayed = state.runs.get(state.displayed.get(job.id)) ?? runs[runs.length - 1];
  const head = element("div", { className: "console-head" });
  for (const run of runs) {
    const tab = element("button", { className: `run ${endClass(run)}`, type: "button", textContent: `#${run.run} ${status(run)}` });
    if (run === displayed) {
      tab.setAttribute("aria-current", "true");
    }
    tab.addEventListener("click", () => {
      state.displayed.set(job.id, run.run);
      drawStage();
    });
    head.append(tab);
  }
  if (displayed?.views.length) {
    const views = element("div", { className: "views" });
    for (const page of displayed.views) {
      const button = element("button", { type: "button", textContent: page.split("/").pop(), title: page });
      button.addEventListener("click", () => viewOpen(page));
      views.append(button);
    }
    head.append(views);
  }
  if (displayed?.made) {
    head.append(madeButtons(job, displayed.made));
  }
  const protocol = job.group === PROTOCOL && displayed;
  if (protocol) {
    const body = viewBody(displayed);
    requestAnimationFrame(() => {
      body.scrollTop = body.scrollHeight;
      state.fuse.follow(displayed);
      state.ruler.follow(displayed);
    });
    return element("div", { className: "console" }, head, tabStrip(displayed), body);
  }
  const lines = element("pre", { className: "lines", id: "lines" });
  displayed?.lines.forEach((line) => lines.append(lineNode(line)));
  if (displayed?.done) {
    lines.append(endNode(displayed));
  }
  requestAnimationFrame(() => {
    lines.scrollTop = lines.scrollHeight;
    state.fuse.follow(displayed ?? null);
    state.ruler.follow(displayed ?? null);
  });
  if (!state.ruler.element.contains(state.fuse.canvas)) {
    state.ruler.element.prepend(state.fuse.canvas);
  }
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
export function lineNode(line) {
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
// built and run again in the job, as tabvla runs it or as the system runs a program opened on its
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

export function endNode(run) {
  return element("span", { className: `end ${endClass(run)}`, textContent: status(run) });
}

// The text chosen in `lines`, as the document holds it: read from the range chosen, which gives it in a
// window without the keys, where WebKit's selection gives none.
export function chosenIn(lines) {
  const selection = window.getSelection();
  return lines && selection?.rangeCount && !selection.isCollapsed && lines.contains(selection.anchorNode) ? selection.getRangeAt(0).toString() : "";
}

// The output's menu: copy what is chosen in it, or all of it, or choose all of it.
export function outputItems(event) {
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
