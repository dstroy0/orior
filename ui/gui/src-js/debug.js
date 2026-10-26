// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger, as debug.rs in the command line's crate runs debug adapters: Debug File builds the
// file in the editor where its language is built and runs it under its language's debugger, and the
// Debug window under the views shows what it is doing. Its bar holds the steps, Continue, Pause, Step
// Over, Step Into, Step Out, Restart and Stop, the session they act on where more than one runs, the
// exceptions it stops on, and what the program is doing. Under it Frames lists the stopped thread's
// calls, less those the tree's patterns hide, Variables the chosen frame's scopes with the watches
// over them and the values kept to compare, and Console the program's output, with a field that
// evaluates in the chosen frame.
//
// More than one session runs at once: a file debugged, a test, a process attached to, a program
// listening at an address, and each process a Python program starts, which debugpy asks to be
// debugged as a child of its session. A stop in any session chooses it.
//
// A breakpoint is set or cleared by a press in the gutter's strip left of a line's number, or F9, and
// is drawn there as a dot, hollow where the debugger could not bind it to code, marked where it
// holds a condition or a count of hits, and a diamond where it writes a message in place of stopping;
// its menu, on a press with the other button, edits these. Breakpoints and watches are kept between
// visits, the breakpoints by tree, and are written out to a file and read back in. Where the program
// stops, its file opens at the line, which is marked until it runs on; a name the pointer rests on
// shows its value, opened a level at a time.

import { invoke, listen } from "./bridge.js";
import { allBreakpoints, exceptionsMenu, exceptionsOf, loadBreakpoints, steppingOf } from "./debug/breakpoints.js";
import { choose, defaultExceptions, drawSessions, ended, sayState, summaryOf, write } from "./debug/session.js";
import { drawFrames, drawVariables, evaluate, stopped } from "./debug/variables.js";
export { askStepping, breakpointMenu, breakpointsOf, exportBreakpoints, importBreakpoints, loadBreakpoints, pausedLineOf, setMessageBreakpoint, toggleBreakpoint, toggleBreakpointHere } from "./debug/breakpoints.js";
export { attachAddress, attachProcess, canStepBack, debugFile, debugging, debugMixed, debugNext, debugStarting, debugTest, isPaused, recordFile, restartDebug, step, stopDebug } from "./debug/session.js";
export { showMachineCode, showMemory, valueAt, watchAddress } from "./debug/values.js";
export { debugCommands } from "./debug/variables.js";

export const BREAKS = "orior.breakpoints";
export const WATCHES = "orior.watches";
const HEIGHT = "orior.debug.height";
export const EXCEPTIONS = "orior.debug.exceptions";
export const STEPPING = "orior.debug.stepping";

// The most lines the console keeps, the most values kept to compare, and how deep and how wide a
// value kept is read.
export const CONSOLE_LINES = 5000;
export const KEPT_MOST = 20;
export const KEPT_DEPTH = 3;
export const KEPT_WIDTH = 60;

export const state = {
  hooks: null,
  // The sessions running, by number: each its info, its stop, its thread.
  sessions: new Map(),
  chosen: null,
  // The names of the sessions starting: a second start of one waits for the first.
  starting: new Set(),
  // What was last started, for Restart.
  last: null,
  // Breakpoints by tree path: each a Map of file line to { bound, condition, hits, log }.
  breaks: new Map(),
  // A line the next stop is at, as { path, line }, in a file with no breakpoint of its own, kept
  // until a session stops and marked nowhere.
  once: null,
  watches: [],
  // The variables opened in the tree, by their path of names, and the scopes closed, which are
  // open until closed.
  opened: new Set(),
  closedScopes: new Set(),
  asked: 0,
  // Values kept to compare with a later stop.
  kept: [],
  // What a session told before its start returned, kept to be heard once it is known, and the
  // number of the last session known.
  early: new Map(),
  newest: 0,
  // The Python sessions that stop before their first line for the native debugger to attach to
  // their process, and whether the session starting is one.
  mixing: new Set(),
  mixNext: false,
};
export const parts = {};

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export const treeKey = () => document.getElementById("tree-path")?.textContent ?? "";
export const chosen = () => (state.chosen === null ? null : state.sessions.get(state.chosen) ?? null);
export const pausedOf = () => chosen()?.paused ?? null;

// The panel.

export function toggleDebugPanel(open = parts.panel.hidden) {
  parts.panel.hidden = !open;
}

function grip(event) {
  event.preventDefault();
  parts.grip.setPointerCapture(event.pointerId);
  const foot = parts.panel.getBoundingClientRect().bottom;
  const shape = getComputedStyle(parts.panel);
  const move = (moved) => {
    parts.panel.style.height = `${Math.max(parseFloat(shape.minHeight), Math.min(parseFloat(shape.maxHeight), foot - moved.clientY))}px`;
  };
  parts.grip.addEventListener("pointermove", move);
  parts.grip.addEventListener("pointerup", () => {
    parts.grip.removeEventListener("pointermove", move);
    localStorage.setItem(HEIGHT, parts.panel.style.height);
  }, { once: true });
}

// The bar's steps: each its command, its glyph, and when it can act.
const STEPS = [
  ["continue", "Continue", "F5", "paused", "M5 3.5v9l7.5-4.5z"],
  ["pause", "Pause", "F6", "running", "M5 3.5h2v9H5zM9 3.5h2v9H9z"],
  ["step-over", "Step Over", "F10", "paused", "M3 9a5 5 0 0 1 9.4-2.4M12.8 3.6v3.2H9.6M8 12.5h.01"],
  ["step-into", "Step Into", "F11, F7", "paused", "M8 2.5v7M5 6.8 8 9.8l3-3M8 13h.01"],
  ["step-out", "Step Out", "Shift+F11", "paused", "M8 10.5v-7M5 6.2 8 3.2l3 3M8 13h.01"],
  ["step-back", "Step Back", "", "back", "M13 9a5 5 0 0 0-9.4-2.4M3.2 3.6v3.2h3.2M8 12.5h.01"],
  ["reverse-continue", "Run Backward", "", "back", "M11 3.5v9L3.5 8z"],
  ["restart-debug", "Restart", "Ctrl+Shift+F5", "last", "M12.5 8A4.5 4.5 0 1 1 11 4.6M11.5 2.2v2.9H8.6"],
  ["stop-debug", "Stop", "Shift+F5, Ctrl+F2", "session", "M4.5 4.5h7v7h-7z"],
];

// Takes a session the app started as running, and hears what it told while it started.
export function known(info) {
  state.sessions.set(info.id, { info, paused: null, thread: undefined });
  if (state.mixNext && info.parent === null) {
    state.mixNext = false;
    state.mixing.add(info.id);
  }
  state.newest = Math.max(state.newest, info.id);
  const told = state.early.get(info.id) ?? [];
  state.early.delete(info.id);
  for (const message of told) {
    heard(message);
  }
}

// What a session's adapter tells as it goes.
function heard(message) {
  const { session: id, event, body } = message;
  // A session starting stops at a breakpoint, or ends, before its start returns.
  if (id > state.newest && ["stopped", "continued", "exited", "terminated", "adapterStopped"].includes(event)) {
    state.early.set(id, [...(state.early.get(id) ?? []), message]);
    return;
  }
  if (event === "output") {
    if (body?.category !== "telemetry" && body?.output) {
      write(body.output, body.category === "stderr" ? "stderr" : body.category === "console" || body.category === "important" ? "console" : "stdout");
    }
  } else if (event === "stopped") {
    stopped(id, body ?? {});
  } else if (event === "continued") {
    const session = state.sessions.get(id);
    if (session) {
      session.paused = null;
    }
    if (id === state.chosen) {
      state.asked += 1;
      drawFrames();
      sayState(summaryOf(chosen()));
    } else {
      drawSessions();
    }
    state.hooks.repaint();
  } else if (event === "exited") {
    write(`${state.sessions.get(id)?.info.name ?? "The program"} ended with exit code ${body?.exitCode ?? "unknown"}.\n`, "console");
  } else if (event === "terminated" || event === "adapterStopped") {
    if (state.sessions.has(id)) {
      ended(id, "Ended");
    }
  } else if (event === "debugpyAttach") {
    // A process the program started is debugged as a child of its session.
    const parent = state.sessions.get(id);
    if (parent) {
      invoke("debug_start", { start: { kind: "child", parent: id, config: body }, options: { breakpoints: allBreakpoints(), exceptions: exceptionsOf("python") ?? defaultExceptions("python"), stepping: { mine_only: steppingOf().mine_only, skip: steppingOf().skip } } })
        .then((info) => {
          known(info);
          drawSessions();
          write(`${info.name} is debugged with ${parent.info.name}.\n`, "console");
        })
        .catch((error) => write(`${String(error)}\n`, "stderr"));
    }
  }
}

// `hooks` gives the editor and the tab it shows, saves a tab, opens a file or opens it at a place,
// draws the editor again, and runs a command of the menus.
export async function startDebug(hooks) {
  state.hooks = hooks;
  // A session left by a page shown before this one has no window to answer to, and is ended.
  invoke("debug_stop", { session: null }).catch(() => {});
  for (const [name, id] of [
    ["panel", "debug"],
    ["grip", "debug-grip"],
    ["bar", "debug-bar"],
    ["said", "debug-said"],
    ["frames", "debug-frames"],
    ["vars", "debug-vars"],
    ["watch", "debug-watch"],
    ["out", "debug-out"],
    ["repl", "debug-repl"],
  ]) {
    parts[name] = document.getElementById(id);
  }
  try {
    state.watches = JSON.parse(localStorage.getItem(WATCHES) ?? "[]") ?? [];
  } catch {
    state.watches = [];
  }
  const height = localStorage.getItem(HEIGHT);
  if (height) {
    parts.panel.style.height = height;
  }
  for (const [command, label, keys, needs, path] of STEPS) {
    const button = element("button", { className: `debug-step ${command}`, type: "button", title: keys ? `${label} (${keys})` : label });
    button.dataset.needs = needs;
    button.setAttribute("aria-label", label);
    button.innerHTML = `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="${path}"/></svg>`;
    button.addEventListener("click", () => hooks.run(command));
    parts.bar.insertBefore(button, parts.said);
  }
  parts.sessions = element("select", { className: "debug-sessions", hidden: true, title: "The session the steps act on" });
  parts.sessions.setAttribute("aria-label", "Session");
  parts.sessions.addEventListener("change", () => choose(Number(parts.sessions.value)));
  const exceptions = element("button", { className: "debug-exceptions", type: "button", textContent: "Exceptions", title: "The exceptions the session stops on" });
  exceptions.dataset.needs = "session";
  exceptions.addEventListener("click", () => exceptionsMenu(exceptions));
  parts.bar.insertBefore(parts.sessions, parts.said);
  parts.bar.insertBefore(exceptions, parts.said);
  parts.grip.addEventListener("pointerdown", grip);
  parts.watch.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && parts.watch.value.trim()) {
      state.watches.push(parts.watch.value.trim());
      localStorage.setItem(WATCHES, JSON.stringify(state.watches));
      parts.watch.value = "";
      drawVariables();
    }
  });
  parts.repl.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && parts.repl.value.trim()) {
      const text = parts.repl.value.trim();
      parts.repl.value = "";
      evaluate(text);
    }
  });
  await listen("debug-event", ({ payload }) => heard(payload));
  drawFrames();
  drawVariables();
  sayState("Debug File (Shift+F9) runs the file in the editor under its debugger.");
  loadBreakpoints();
}
