// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The debugger, as debug.rs in the command line's crate runs a debug adapter: Debug File builds the
// file in the editor where its language is built and runs it under its language's debugger, and the
// Debug window under the views shows what it is doing. Its bar holds the steps, Continue, Pause, Step
// Over, Step Into, Step Out, Restart and Stop, and what the program is doing. Under it Frames lists
// the stopped thread's calls, Variables the chosen frame's scopes with the watches over them, and
// Console the program's output, with a field that evaluates in the chosen frame.
//
// A breakpoint is set or cleared by a press in the gutter's strip left of a line's number, or F9, and
// is drawn there as a dot, hollow where the debugger could not bind it to code. Breakpoints and
// watches are kept between visits, the breakpoints by tree. Where the program stops, its file opens
// at the line, which is marked until it runs on.

import { invoke, listen } from "./bridge.js";
import { say } from "./statusbar.js";

const BREAKS = "orior.breakpoints";
const WATCHES = "orior.watches";
const HEIGHT = "orior.debug.height";

// The most lines the console keeps.
const CONSOLE_LINES = 5000;

const state = {
  hooks: null,
  // The file being debugged, its language and the toolchain debugging it, while a session runs.
  session: null,
  starting: false,
  // The stopped thread, its frames and the frame chosen, while the program is stopped.
  paused: null,
  // Breakpoints by tree path: each a Map of file line to whether the debugger bound it.
  breaks: new Map(),
  watches: [],
  // The variables opened in the tree, by their path of names, and the scopes closed, which are
  // open until closed.
  opened: new Set(),
  closedScopes: new Set(),
  asked: 0,
};
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const treeKey = () => document.getElementById("tree-path")?.textContent ?? "";

// Breakpoints.

function keepBreaks() {
  let all = {};
  try {
    all = JSON.parse(localStorage.getItem(BREAKS) ?? "{}") ?? {};
  } catch {
    all = {};
  }
  const mine = {};
  for (const [path, lines] of state.breaks) {
    if (lines.size) {
      mine[path] = [...lines.keys()].sort((a, b) => a - b);
    }
  }
  all[treeKey()] = mine;
  localStorage.setItem(BREAKS, JSON.stringify(all));
}

// Reads the breakpoints kept for the tree that is open.
export function loadBreakpoints() {
  state.breaks = new Map();
  try {
    const mine = JSON.parse(localStorage.getItem(BREAKS) ?? "{}")?.[treeKey()] ?? {};
    for (const [path, lines] of Object.entries(mine)) {
      state.breaks.set(path, new Map(lines.map((line) => [line, true])));
    }
  } catch {
    state.breaks = new Map();
  }
  state.hooks?.repaint();
}

// The breakpoints of a file, each line mapped to whether it is bound, or null where it has none.
export function breakpointsOf(path) {
  const lines = state.breaks.get(path);
  return lines?.size ? lines : null;
}

// The line of a file the program is stopped on, or null.
export function pausedLineOf(path) {
  const frame = state.paused?.frames[state.paused.at];
  return frame && frame.path === path ? frame.line : null;
}

async function tellBreaks(path) {
  if (!state.session) {
    return;
  }
  const lines = [...(state.breaks.get(path)?.keys() ?? [])];
  const placed = await invoke("debug_breakpoints", { path, lines }).catch(() => null);
  if (placed) {
    state.breaks.set(path, new Map(placed.map(([line, bound]) => [line, bound])));
    state.hooks.repaint();
  }
}

export function toggleBreakpoint(path, line) {
  if (!state.breaks.has(path)) {
    state.breaks.set(path, new Map());
  }
  const lines = state.breaks.get(path);
  if (lines.has(line)) {
    lines.delete(line);
  } else {
    lines.set(line, true);
  }
  keepBreaks();
  state.hooks.repaint();
  tellBreaks(path);
}

export function toggleBreakpointHere() {
  const tab = state.hooks.tab();
  const editor = state.hooks.editor();
  if (tab?.session && editor?.s === tab.session) {
    toggleBreakpoint(tab.file, tab.session.base + editor.head().line);
  }
}

// The session.

export const debugging = () => Boolean(state.session);
export const isPaused = () => Boolean(state.paused);

function sayState(text, failed = false) {
  parts.said.textContent = text;
  parts.said.classList.toggle("failed", failed);
  for (const button of parts.bar.querySelectorAll("button[data-needs]")) {
    const need = button.dataset.needs;
    button.disabled = need === "paused" ? !state.paused : need === "running" ? !state.session || Boolean(state.paused) : need === "session" ? !state.session : !state.session && !state.last;
  }
}

function write(text, kind = "stdout") {
  const last = parts.out.lastElementChild;
  // A piece of output with no line end runs on into the next.
  if (last && last.dataset.kind === kind && !last.dataset.ended) {
    last.textContent += text;
  } else {
    parts.out.append(element("div", { className: "debug-line", textContent: text }));
  }
  const line = parts.out.lastElementChild;
  line.dataset.kind = kind;
  if (text.endsWith("\n")) {
    line.dataset.ended = "true";
    line.textContent = line.textContent.replace(/\n$/, "");
  }
  while (parts.out.childElementCount > CONSOLE_LINES) {
    parts.out.firstElementChild.remove();
  }
  parts.out.scrollTop = parts.out.scrollHeight;
}

export async function debugFile() {
  const tab = state.hooks.tab();
  const s = tab?.session;
  if (!s || tab.commit) {
    say("Open a file to debug it.");
    return;
  }
  if (state.starting) {
    return;
  }
  await state.hooks.save(tab);
  if (state.session) {
    await stopDebug();
  }
  const language = s.language?.id ?? "plaintext";
  state.last = { path: tab.file, language };
  toggleDebugPanel(true);
  parts.out.replaceChildren();
  state.paused = null;
  state.starting = true;
  sayState(`Starting ${tab.file}…`);
  const breakpoints = {};
  for (const [path, lines] of state.breaks) {
    if (lines.size) {
      breakpoints[path] = [...lines.keys()];
    }
  }
  try {
    const tool = await invoke("debug_start", { path: tab.file, language, breakpoints });
    state.session = { path: tab.file, language, tool };
    if (!state.paused) {
      sayState(`Running ${tab.file} under ${tool}`);
    }
  } catch (error) {
    sayState("Did not start", true);
    write(`${String(error)}\n`, "stderr");
  } finally {
    state.starting = false;
  }
  for (const path of state.breaks.keys()) {
    tellBreaks(path);
  }
}

export async function restartDebug() {
  if (!state.last) {
    return;
  }
  await state.hooks.open(state.last.path);
  debugFile();
}

export async function stopDebug() {
  if (!state.session) {
    return;
  }
  await invoke("debug_stop").catch(() => {});
  ended("Stopped");
}

function ended(text) {
  state.session = null;
  state.paused = null;
  drawFrames();
  drawVariables();
  sayState(text);
  state.hooks.repaint();
}

export async function step(how) {
  const thread = state.paused?.thread ?? state.thread;
  if (!state.session || thread === undefined) {
    return;
  }
  if (how !== "pause" && !state.paused) {
    return;
  }
  try {
    await invoke("debug_step", { how, thread });
  } catch (error) {
    write(`${String(error)}\n`, "stderr");
  }
}

// A stop.

async function stopped(body) {
  const asked = ++state.asked;
  let thread = body.threadId;
  if (thread === undefined) {
    const threads = await invoke("debug_threads").catch(() => []);
    thread = threads[0]?.id;
  }
  state.thread = thread;
  const frames = thread === undefined ? [] : await invoke("debug_stack", { thread }).catch(() => []);
  if (asked !== state.asked) {
    return;
  }
  state.paused = { thread, frames, at: 0, reason: body.reason ?? "pause" };
  const top = frames[0];
  const where = top ? `${top.path ? `${top.path.split(/[\\/]/).pop()}:${top.line + 1}` : top.name}` : "";
  const why = { breakpoint: "at a breakpoint", step: "after a step", pause: "paused", exception: "on an exception", entry: "at the start" }[state.paused.reason] ?? state.paused.reason;
  sayState(`Stopped ${why}${where ? `, ${where}` : ""}`);
  if (body.reason === "exception" && body.text) {
    write(`${body.text}\n`, "stderr");
  }
  await chooseFrame(0);
}

async function chooseFrame(at) {
  if (!state.paused) {
    return;
  }
  state.paused.at = at;
  const frame = state.paused.frames[at];
  drawFrames();
  if (frame?.path && !/^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(frame.path)) {
    await state.hooks.openAt(frame.path, frame.line, frame.col);
  }
  state.hooks.repaint();
  await drawVariables();
}

function drawFrames() {
  const frames = state.paused?.frames ?? [];
  parts.frames.replaceChildren(
    ...frames.map((frame, at) => {
      const row = element(
        "button",
        { className: "debug-frame", type: "button", title: frame.path ? `${frame.path}:${frame.line + 1}` : frame.name },
        element("span", { className: "debug-frame-name", textContent: frame.name }),
        element("span", { className: "debug-frame-where", textContent: frame.path ? `${frame.path.split(/[\\/]/).pop()}:${frame.line + 1}` : "" }),
      );
      row.setAttribute("aria-current", String(at === state.paused.at));
      row.addEventListener("click", () => chooseFrame(at));
      return row;
    }),
  );
  if (!frames.length) {
    parts.frames.append(element("p", { className: "debug-empty", textContent: state.session ? "Running." : "Frames show here when the program stops." }));
  }
}

// Variables and watches.

// A row of the tree: a twisty where it holds more, its name, value and type. `open` says whether it
// is open, and `toggle` opens or closes it.
function variableRow(variable, depth, open, toggle) {
  const row = element("div", { className: "debug-var" });
  row.style.paddingLeft = `${0.5 + depth * 0.9}rem`;
  const twisty = element("button", { className: "debug-twisty", type: "button", textContent: variable.reference ? (open ? "▾" : "▸") : "" });
  twisty.disabled = !variable.reference;
  twisty.setAttribute("aria-label", open ? `Close ${variable.name}` : `Open ${variable.name}`);
  row.append(twisty, element("span", { className: "debug-var-name", textContent: variable.name }), element("span", { className: "debug-var-value", textContent: variable.value, title: variable.kind ? `${variable.kind}: ${variable.value}` : variable.value }));
  if (variable.kind) {
    row.append(element("span", { className: "debug-var-kind", textContent: variable.kind }));
  }
  const holder = element("div", {}, row);
  if (variable.reference) {
    twisty.addEventListener("click", () => {
      toggle();
      drawVariables();
    });
  }
  return holder;
}

function flip(set, key) {
  if (set.has(key)) {
    set.delete(key);
  } else {
    set.add(key);
  }
}

async function variablesUnder(reference, depth, prefix, into, asked) {
  const found = await invoke("debug_variables", { reference }).catch(() => []);
  for (const variable of found) {
    if (asked !== state.asked) {
      return;
    }
    const key = `${prefix}/${variable.name}`;
    const open = state.opened.has(key);
    const holder = variableRow(variable, depth, open, () => flip(state.opened, key));
    into.append(holder);
    if (open && variable.reference && depth < 12) {
      await variablesUnder(variable.reference, depth + 1, key, holder, asked);
    }
  }
}

async function drawVariables() {
  const asked = state.asked;
  const frame = state.paused?.frames[state.paused.at];
  const box = element("div");
  for (const [at, expression] of state.watches.entries()) {
    const row = element("div", { className: "debug-var debug-watch" });
    const remove = element("button", { className: "debug-twisty", type: "button", textContent: "×", title: "Remove the watch" });
    remove.addEventListener("click", () => {
      state.watches.splice(at, 1);
      localStorage.setItem(WATCHES, JSON.stringify(state.watches));
      drawVariables();
    });
    const value = element("span", { className: "debug-var-value" });
    row.append(remove, element("span", { className: "debug-var-name", textContent: expression }), value);
    box.append(row);
    if (frame) {
      invoke("debug_evaluate", { expression, frame: frame.id, context: "watch" })
        .then((found) => {
          value.textContent = found.value;
          value.title = found.kind ? `${found.kind}: ${found.value}` : found.value;
        })
        .catch((error) => {
          value.textContent = String(error);
          value.classList.add("failed");
        });
    } else {
      value.textContent = "";
    }
  }
  if (frame) {
    const scopes = await invoke("debug_scopes", { frame: frame.id }).catch(() => []);
    for (const [at, scope] of scopes.entries()) {
      if (asked !== state.asked) {
        return;
      }
      // The first scope, a frame's own variables, is open until closed; the rest, such as globals
      // and registers, and any the debugger says is slow to read, are closed until opened.
      const key = `scope:${scope.name}`;
      const first = at === 0 && !scope.expensive;
      const open = first ? !state.closedScopes.has(scope.name) : state.opened.has(key);
      const toggle = () => (first ? flip(state.closedScopes, scope.name) : flip(state.opened, key));
      const holder = variableRow({ name: scope.name, value: "", kind: "", reference: scope.reference }, 0, open, toggle);
      holder.firstElementChild.classList.add("debug-scope");
      box.append(holder);
      if (open) {
        await variablesUnder(scope.reference, 1, key, holder, asked);
      }
    }
  } else if (!state.watches.length) {
    box.append(element("p", { className: "debug-empty", textContent: "Variables show here when the program stops." }));
  }
  if (asked === state.asked) {
    parts.vars.replaceChildren(...box.children);
  }
}

// The console's field: evaluates in the chosen frame.
async function evaluate(text) {
  const frame = state.paused?.frames[state.paused.at];
  write(`> ${text}\n`, "input");
  try {
    const found = await invoke("debug_evaluate", { expression: text, frame: frame?.id ?? null, context: "repl" });
    write(`${found.value}\n`, "result");
  } catch (error) {
    write(`${String(error)}\n`, "stderr");
  }
  if (state.paused) {
    drawVariables();
  }
}

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
  ["restart-debug", "Restart", "Ctrl+Shift+F5", "last", "M12.5 8A4.5 4.5 0 1 1 11 4.6M11.5 2.2v2.9H8.6"],
  ["stop-debug", "Stop", "Shift+F5, Ctrl+F2", "session", "M4.5 4.5h7v7h-7z"],
];

// `hooks` gives the editor and the tab it shows, saves a tab, opens a file or opens it at a place,
// draws the editor again, and runs a command of the menus.
export async function startDebug(hooks) {
  state.hooks = hooks;
  // A session left by a page shown before this one has no window to answer to, and is ended.
  invoke("debug_stop").catch(() => {});
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
    const button = element("button", { className: `debug-step ${command}`, type: "button", title: `${label} (${keys})` });
    button.dataset.needs = needs;
    button.setAttribute("aria-label", label);
    button.innerHTML = `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="${path}"/></svg>`;
    button.addEventListener("click", () => hooks.run(command));
    parts.bar.insertBefore(button, parts.said);
  }
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
  await listen("debug-event", ({ payload }) => {
    const { event, body } = payload;
    if (event === "output") {
      if (body?.category !== "telemetry" && body?.output) {
        write(body.output, body.category === "stderr" ? "stderr" : body.category === "console" || body.category === "important" ? "console" : "stdout");
      }
    } else if (event === "stopped") {
      stopped(body ?? {});
    } else if (event === "continued") {
      state.asked += 1;
      state.paused = null;
      drawFrames();
      sayState(state.session ? `Running ${state.session.path} under ${state.session.tool}` : "Running");
      hooks.repaint();
    } else if (event === "exited") {
      write(`The program ended with exit code ${body?.exitCode ?? "unknown"}.\n`, "console");
    } else if (event === "terminated" || event === "adapterStopped") {
      if (state.session || state.starting) {
        ended("Ended");
      }
    }
  });
  drawFrames();
  drawVariables();
  sayState("Debug File (Shift+F9) runs the file in the editor under its debugger.");
  loadBreakpoints();
}
