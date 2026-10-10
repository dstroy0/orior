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

import { invoke, listen, pick } from "./bridge.js";
import { showMenu } from "./menu.js";
import { askFor, sheet } from "./menubar.js";
import { say } from "./statusbar.js";

const BREAKS = "orior.breakpoints";
const WATCHES = "orior.watches";
const HEIGHT = "orior.debug.height";
const EXCEPTIONS = "orior.debug.exceptions";
const STEPPING = "orior.debug.stepping";

// The most lines the console keeps, the most values kept to compare, and how deep and how wide a
// value kept is read.
const CONSOLE_LINES = 5000;
const KEPT_MOST = 20;
const KEPT_DEPTH = 3;
const KEPT_WIDTH = 60;

const state = {
  hooks: null,
  // The sessions running, by number: each its info, its stop, its thread.
  sessions: new Map(),
  chosen: null,
  starting: false,
  // What was last started, for Restart.
  last: null,
  // Breakpoints by tree path: each a Map of file line to { bound, condition, hits, log }.
  breaks: new Map(),
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
};
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const treeKey = () => document.getElementById("tree-path")?.textContent ?? "";
const chosen = () => (state.chosen === null ? null : state.sessions.get(state.chosen) ?? null);
const pausedOf = () => chosen()?.paused ?? null;

// Breakpoints.

function readKept(key) {
  try {
    return JSON.parse(localStorage.getItem(key) ?? "{}") ?? {};
  } catch {
    return {};
  }
}

// A breakpoint as the adapter takes it: its line, its condition, its count of hits, its message.
const specOf = (line, one) => ({ line, condition: one.condition ?? "", hits: one.hits ?? "", log: one.log ?? "" });

function keepBreaks() {
  const all = readKept(BREAKS);
  const mine = {};
  for (const [path, lines] of state.breaks) {
    if (lines.size) {
      mine[path] = [...lines].sort(([a], [b]) => a - b).map(([line, one]) => (one.condition || one.hits || one.log ? specOf(line, one) : line));
    }
  }
  all[treeKey()] = mine;
  localStorage.setItem(BREAKS, JSON.stringify(all));
}

// Each breakpoint kept for a file, a line alone or a breakpoint whole, as a Map of its line to it.
function breaksOf(kept) {
  return new Map(kept.map((one) => (typeof one === "number" ? [one, { bound: true }] : [one.line, { bound: true, condition: one.condition ?? "", hits: one.hits ?? "", log: one.log ?? "" }])));
}

// Reads the breakpoints kept for the tree that is open.
export function loadBreakpoints() {
  state.breaks = new Map();
  for (const [path, kept] of Object.entries(readKept(BREAKS)[treeKey()] ?? {})) {
    state.breaks.set(path, breaksOf(kept));
  }
  state.hooks?.repaint();
}

// The breakpoints of a file, each line mapped to { bound, condition, hits, log }, or null where it
// has none.
export function breakpointsOf(path) {
  const lines = state.breaks.get(path);
  return lines?.size ? lines : null;
}

// The line of a file the chosen session is stopped on, or null.
export function pausedLineOf(path) {
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  return frame && frame.path === path ? frame.line : null;
}

// The breakpoints of every file, as the adapters take them.
function allBreakpoints() {
  const out = {};
  for (const [path, lines] of state.breaks) {
    if (lines.size) {
      out[path] = [...lines].map(([line, one]) => specOf(line, one));
    }
  }
  return out;
}

async function tellBreaks(path) {
  if (!state.sessions.size) {
    return;
  }
  const lines = state.breaks.get(path) ?? new Map();
  const placed = await invoke("debug_breakpoints", { path, breakpoints: [...lines].map(([line, one]) => specOf(line, one)) }).catch(() => null);
  if (placed) {
    const before = [...lines.values()];
    state.breaks.set(path, new Map(placed.map(([line, bound], at) => [line, { ...before[at], bound }])));
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
    lines.set(line, { bound: true, condition: "", hits: "", log: "" });
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

// Sets what the breakpoint on `line` of `path` asks: its condition, its count of hits, its message.
function setBreakpoint(path, line, change) {
  if (!state.breaks.has(path)) {
    state.breaks.set(path, new Map());
  }
  const lines = state.breaks.get(path);
  lines.set(line, { bound: true, condition: "", hits: "", log: "", ...lines.get(line), ...change });
  keepBreaks();
  state.hooks.repaint();
  tellBreaks(path);
}

// The menu of the breakpoint strip on `line` of `path`: edit the breakpoint there, set one that
// writes a message, or take it away.
export function breakpointMenu(path, line, x, y) {
  const here = state.breaks.get(path)?.get(line);
  showMenu(x, y, [
    { label: here ? "Edit Breakpoint…" : "Add Breakpoint…", run: () => editBreakpoint(path, line) },
    { label: "Add Message Breakpoint…", disabled: Boolean(here?.log), run: () => editBreakpoint(path, line, { focus: "log" }) },
    "-",
    { label: "Remove Breakpoint", disabled: !here, run: () => toggleBreakpoint(path, line) },
  ]);
}

// The sheet that edits a breakpoint: the expression it stops only where it holds, the count of hits
// it stops at, and the message it writes in place of stopping.
function editBreakpoint(path, line, { focus = "condition" } = {}) {
  const here = state.breaks.get(path)?.get(line) ?? {};
  const form = element("form", { className: "sheet-report" });
  const field = (label, value, placeholder) => {
    const input = element("input", { className: "report-field", value: value ?? "", placeholder, spellcheck: false });
    input.setAttribute("aria-label", label);
    const row = element("label", { className: "report-row" }, element("span", { textContent: label }), input);
    return [row, input];
  };
  const [conditionRow, condition] = field("Stop only where", here.condition, "An expression, such as x > 2");
  const [hitsRow, hits] = field("Stop at the hit", here.hits, "A count, such as 3 or >= 3");
  const [logRow, log] = field("Write in place of stopping", here.log, "A message, {name} the value of name");
  const go = element("button", { className: "primary", type: "submit", textContent: "Set" });
  form.append(element("h2", { textContent: `Breakpoint at ${path.split("/").pop()}:${line + 1}` }), conditionRow, hitsRow, logRow, element("div", { className: "report-foot" }, go));
  const dialog = sheet(form);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    setBreakpoint(path, line, { condition: condition.value.trim(), hits: hits.value.trim(), log: log.value.trim() });
    dialog.close();
  });
  ({ condition, hits, log })[focus].focus();
}

// Run, Export Breakpoints: the tree's breakpoints and the watches, written to a file chosen.
export async function exportBreakpoints() {
  const file = await pick("save").catch(() => null);
  if (!file) {
    return;
  }
  const kept = { breakpoints: Object.fromEntries([...state.breaks].filter(([, lines]) => lines.size).map(([path, lines]) => [path, [...lines].map(([line, one]) => specOf(line, one))])), watches: state.watches };
  try {
    await invoke("file_write_any", { path: file, text: `${JSON.stringify(kept, null, 2)}\n` });
    say(`The breakpoints and watches are written to ${file}.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Import Breakpoints: the breakpoints and watches of a file chosen, taken in beside the tree's.
export async function importBreakpoints() {
  const file = await pick("file").catch(() => null);
  if (!file) {
    return;
  }
  try {
    const kept = JSON.parse(await invoke("file_read_any", { path: file }));
    for (const [path, list] of Object.entries(kept.breakpoints ?? {})) {
      state.breaks.set(path, new Map([...(state.breaks.get(path) ?? new Map()), ...breaksOf(list)]));
      tellBreaks(path);
    }
    for (const watch of kept.watches ?? []) {
      if (!state.watches.includes(watch)) {
        state.watches.push(watch);
      }
    }
    localStorage.setItem(WATCHES, JSON.stringify(state.watches));
    keepBreaks();
    state.hooks.repaint();
    drawVariables();
    say(`The breakpoints and watches of ${file} are taken in.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Exceptions and stepping.

// The exceptions a session in `language` stops on, by the adapter's names for them, as last chosen.
function exceptionsOf(language) {
  return readKept(EXCEPTIONS)[language] ?? null;
}

function keepExceptions(language, on) {
  const all = readKept(EXCEPTIONS);
  all[language] = on;
  localStorage.setItem(EXCEPTIONS, JSON.stringify(all));
}

// The stepping the tree asks for: whether a step keeps to its own code, the patterns a step passes
// over, and the patterns of the frames a stop hides.
export function steppingOf() {
  return { mine_only: true, skip: [], hide: [], ...(readKept(STEPPING)[treeKey()] ?? {}) };
}

// Run, Stepping: the sheet that sets the tree's stepping.
export function askStepping() {
  const now = steppingOf();
  const form = element("form", { className: "sheet-report" });
  const mine = element("input", { type: "checkbox", checked: now.mine_only });
  const skip = element("textarea", { className: "report-field", rows: 4, value: now.skip.join("\n"), placeholder: "A module, a path's glob or a function's pattern a line", spellcheck: false });
  const hide = element("textarea", { className: "report-field", rows: 3, value: now.hide.join("\n"), placeholder: "A pattern a line: a frame whose name or file holds it is hidden", spellcheck: false });
  form.append(
    element("h2", { textContent: "Stepping" }),
    element("label", { className: "report-check" }, mine, "Step into the tree's own code only"),
    element("label", { className: "report-row" }, element("span", { textContent: "Pass over" }), skip),
    element("label", { className: "report-row" }, element("span", { textContent: "Hide frames" }), hide),
    element("div", { className: "report-foot" }, element("button", { className: "primary", type: "submit", textContent: "Set" })),
  );
  const dialog = sheet(form);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const lines = (text) => text.split("\n").map((one) => one.trim()).filter(Boolean);
    const all = readKept(STEPPING);
    all[treeKey()] = { mine_only: mine.checked, skip: lines(skip.value), hide: lines(hide.value) };
    localStorage.setItem(STEPPING, JSON.stringify(all));
    say("The tree's stepping is set; a session started from now takes it.");
    drawFrames();
    dialog.close();
  });
}

// The menu of the exceptions the chosen session can stop on.
function exceptionsMenu(button) {
  const session = chosen();
  if (!session?.info.filters.length) {
    say("The session's debugger stops on no exceptions of its own.");
    return;
  }
  const box = button.getBoundingClientRect();
  showMenu(
    box.left,
    box.bottom + 2,
    session.info.filters.map((filter) => ({
      label: filter.label,
      checked: filter.on,
      run: async () => {
        filter.on = !filter.on;
        const on = session.info.filters.filter((one) => one.on).map((one) => one.filter);
        keepExceptions(session.info.language, on);
        await invoke("debug_exceptions", { session: session.info.id, on }).catch((error) => say(String(error), { failed: true }));
      },
    })),
    { anchor: button },
  );
}

// The session.

export const debugging = () => state.sessions.size > 0;
export const isPaused = () => Boolean(pausedOf());

function sayState(text, failed = false) {
  parts.said.textContent = text;
  parts.said.classList.toggle("failed", failed);
  const session = chosen();
  for (const button of parts.bar.querySelectorAll("button[data-needs]")) {
    const need = button.dataset.needs;
    button.disabled = need === "paused" ? !session?.paused : need === "running" ? !session || Boolean(session.paused) : need === "session" ? !session : !session && !state.last;
  }
  drawSessions();
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

// Starts a session as `start` says, `name` saying what runs.
async function begin(start, name, { fresh = true } = {}) {
  if (state.starting) {
    return null;
  }
  toggleDebugPanel(true);
  if (fresh) {
    parts.out.replaceChildren();
  }
  state.starting = true;
  sayState(`Starting ${name}…`);
  const stepping = steppingOf();
  const language = start.language ?? "python";
  const options = { breakpoints: allBreakpoints(), exceptions: exceptionsOf(language) ?? defaultExceptions(language), stepping: { mine_only: stepping.mine_only, skip: stepping.skip } };
  try {
    const info = await invoke("debug_start", { start, options });
    known(info);
    state.chosen = info.id;
    if (!chosen()?.paused) {
      sayState(`Running ${info.name} under ${info.tool}`);
    }
    for (const path of state.breaks.keys()) {
      tellBreaks(path);
    }
    return info;
  } catch (error) {
    sayState("Did not start", true);
    write(`${String(error)}\n`, "stderr");
    return null;
  } finally {
    state.starting = false;
  }
}

// The exceptions a session in `language` stops on before any are chosen: those nothing catches.
const defaultExceptions = (language) => (language === "python" ? ["uncaught"] : []);

export async function debugFile() {
  const tab = state.hooks.tab();
  const s = tab?.session;
  if (!s || tab.commit) {
    say("Open a file to debug it.");
    return;
  }
  await state.hooks.save(tab);
  const language = s.language?.id ?? "plaintext";
  state.last = { start: { kind: "file", path: tab.file, language }, name: tab.file };
  await begin(state.last.start, tab.file);
}

export async function restartDebug() {
  if (!state.last) {
    return;
  }
  const session = chosen();
  if (session) {
    await invoke("debug_stop", { session: session.info.id }).catch(() => {});
    ended(session.info.id, "Stopped");
  }
  if (state.last.start.kind === "file") {
    await state.hooks.open(state.last.start.path);
  }
  await begin(state.last.start, state.last.name);
}

// Debugs the Python test `id`, named as pytest names it, run by pytest under the debugger with the
// breakpoints set.
export async function debugTest(id) {
  state.last = { start: { kind: "test", id, language: "python" }, name: id };
  await begin(state.last.start, id);
}

// Run, Attach to Process: the machine's processes in the quick open; the one chosen is debugged, by
// debugpy where its program is Python and by the native debugger where not.
export async function attachProcess() {
  const found = await invoke("debug_processes").catch(() => []);
  const { openPalette } = await import("./palette.js");
  const { fuzzy } = await import("./fuzzy.js");
  openPalette("", {
    custom: {
      placeholder: "Attach to a process: type to filter, Enter to debug it",
      empty: "No process is found.",
      rows: (query) =>
        found
          .map((one) => {
            const label = `${one.name} ${one.pid}`;
            const hit = fuzzy(query, `${label} ${one.command}`);
            return hit ? { label, hits: hit.hits.filter((at) => at < label.length), detail: one.command.slice(0, 160), score: query ? hit.score : 0, run: () => attachTo(one) } : null;
          })
          .filter(Boolean)
          .sort((a, b) => b.score - a.score)
          .slice(0, 300),
    },
  });
}

async function attachTo(process) {
  const language = /python/i.test(process.name) ? "python" : "c";
  const start = { kind: "process", pid: process.pid, language };
  state.last = { start, name: `${process.name} ${process.pid}` };
  await begin(start, state.last.name);
}

// Run, Attach to Address: a program whose debugpy listens at host:port, and after it the folder the
// tree's files are at where the program runs, where that is not the tree, as /app in a container.
export async function attachAddress(given) {
  const answer = (given ?? (await askFor("The address debugpy listens at, host:port, and the folder the program runs in where it is not this tree, such as /app in a container", "127.0.0.1:5678")))?.trim();
  if (!answer) {
    return;
  }
  const [address, ...folder] = answer.split(/\s+/);
  const [host, port] = address.includes(":") ? [address.slice(0, address.lastIndexOf(":")), address.slice(address.lastIndexOf(":") + 1)] : ["127.0.0.1", address];
  if (!/^\d+$/.test(port)) {
    say(`${address} is no address: host:port, such as 127.0.0.1:5678.`, { failed: true });
    return;
  }
  const start = { kind: "address", host: host || "127.0.0.1", port: Number(port), remote: folder.join(" ") || null, language: "python" };
  state.last = { start, name: `${start.host}:${start.port}` };
  await begin(start, state.last.name);
}

// Stops the chosen session, and its children with it.
export async function stopDebug() {
  const session = chosen();
  if (!session) {
    return;
  }
  await invoke("debug_stop", { session: session.info.id }).catch(() => {});
  ended(session.info.id, "Stopped");
}

function ended(id, text) {
  const gone = [id, ...[...state.sessions.values()].filter((one) => one.info.parent === id).map((one) => one.info.id)];
  gone.forEach((one) => state.sessions.delete(one));
  if (gone.includes(state.chosen)) {
    state.chosen = state.sessions.size ? [...state.sessions.keys()].at(-1) : null;
  }
  state.asked += 1;
  drawFrames();
  drawVariables();
  sayState(state.sessions.size ? summaryOf(chosen()) : text);
  state.hooks.repaint();
}

function summaryOf(session) {
  if (!session) {
    return "";
  }
  return session.paused ? session.said ?? "Stopped" : `Running ${session.info.name} under ${session.info.tool}`;
}

export async function step(how) {
  const session = chosen();
  const thread = session?.paused?.thread ?? session?.thread;
  if (!session || thread === undefined) {
    return;
  }
  if (how !== "pause" && !session.paused) {
    return;
  }
  try {
    await invoke("debug_step", { session: session.info.id, how, thread });
  } catch (error) {
    write(`${String(error)}\n`, "stderr");
  }
}

// Chooses the session the bar and the columns show.
function choose(id) {
  if (!state.sessions.has(id)) {
    return;
  }
  state.chosen = id;
  state.asked += 1;
  sayState(summaryOf(chosen()));
  drawFrames();
  drawVariables();
  const paused = pausedOf();
  if (paused) {
    chooseFrame(paused.at);
  }
  state.hooks.repaint();
}

function drawSessions() {
  if (!parts.sessions) {
    return;
  }
  const sessions = [...state.sessions.values()];
  parts.sessions.hidden = sessions.length < 2;
  parts.sessions.replaceChildren(
    ...sessions.map((one) => {
      const option = element("option", { value: String(one.info.id), textContent: `${one.info.parent ? "↳ " : ""}${one.info.name}${one.paused ? " (stopped)" : ""}` });
      option.selected = one.info.id === state.chosen;
      return option;
    }),
  );
}

// A stop.

async function stopped(id, body) {
  const session = state.sessions.get(id);
  if (!session) {
    return;
  }
  const asked = ++state.asked;
  let thread = body.threadId;
  if (thread === undefined) {
    const threads = await invoke("debug_threads", { session: id }).catch(() => []);
    thread = threads[0]?.id;
  }
  session.thread = thread;
  const frames = thread === undefined ? [] : await invoke("debug_stack", { session: id, thread }).catch(() => []);
  if (asked !== state.asked) {
    return;
  }
  session.paused = { thread, frames, at: 0, reason: body.reason ?? "pause" };
  // A stop in any session chooses it.
  state.chosen = id;
  const top = frames[0];
  const where = top ? `${top.path ? `${top.path.split(/[\\/]/).pop()}:${top.line + 1}` : top.name}` : "";
  const why = { breakpoint: "at a breakpoint", step: "after a step", pause: "paused", exception: "on an exception", entry: "at the start", "data breakpoint": "where its data changed" }[session.paused.reason] ?? session.paused.reason;
  session.said = `Stopped ${why}${where ? `, ${where}` : ""}`;
  sayState(session.said);
  if (body.reason === "exception") {
    if (session.info.exception_info && thread !== undefined) {
      const raised = await invoke("debug_raised", { session: id, thread }).catch(() => null);
      if (raised) {
        session.said = `Stopped on ${raised.id}${raised.description ? `: ${raised.description}` : ""}${where ? `, ${where}` : ""}`;
        sayState(session.said);
        write(`${raised.id}: ${raised.description}\n${raised.stack ? `${raised.stack}\n` : ""}`, "stderr");
      }
    } else if (body.text) {
      write(`${body.text}\n`, "stderr");
    }
  }
  await chooseFrame(firstShown(frames));
}

// The first frame a stop shows: the first the tree's patterns do not hide.
function firstShown(frames) {
  const at = frames.findIndex((frame) => !hidden(frame));
  return at < 0 ? 0 : at;
}

function hidden(frame) {
  const patterns = steppingOf().hide;
  return patterns.some((pattern) => {
    try {
      return new RegExp(pattern).test(`${frame.name} ${frame.path ?? ""}`);
    } catch {
      return `${frame.name} ${frame.path ?? ""}`.includes(pattern);
    }
  });
}

async function chooseFrame(at) {
  const paused = pausedOf();
  if (!paused) {
    return;
  }
  paused.at = at;
  const frame = paused.frames[at];
  drawFrames();
  if (frame?.path && !/^(?:[A-Za-z]:[\\/]|\/|\\\\)/.test(frame.path)) {
    await state.hooks.openAt(frame.path, frame.line, frame.col);
  }
  state.hooks.repaint();
  await drawVariables();
}

function drawFrames() {
  const paused = pausedOf();
  const frames = paused?.frames ?? [];
  const shown = frames.map((frame, at) => [frame, at]).filter(([frame]) => !hidden(frame));
  parts.frames.replaceChildren(
    ...shown.map(([frame, at]) => {
      const row = element(
        "button",
        { className: "debug-frame", type: "button", title: frame.path ? `${frame.path}:${frame.line + 1}` : frame.name },
        element("span", { className: "debug-frame-name", textContent: frame.name }),
        element("span", { className: "debug-frame-where", textContent: frame.path ? `${frame.path.split(/[\\/]/).pop()}:${frame.line + 1}` : "" }),
      );
      row.setAttribute("aria-current", String(at === paused.at));
      row.addEventListener("click", () => chooseFrame(at));
      row.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        event.stopPropagation();
        frameMenu(frame, event.clientX, event.clientY);
      });
      return row;
    }),
  );
  if (frames.length > shown.length) {
    parts.frames.append(element("p", { className: "debug-empty", textContent: `${frames.length - shown.length} frame${frames.length - shown.length === 1 ? "" : "s"} hidden by the tree's patterns: Run, Stepping sets them.` }));
  }
  if (!frames.length) {
    parts.frames.append(element("p", { className: "debug-empty", textContent: chosen() ? "Running." : "Frames show here when the program stops." }));
  }
}

// A frame's menu: its instructions about the line, or its bytecode for Python.
function frameMenu(frame, x, y) {
  const session = chosen();
  const python = session?.info.language === "python";
  showMenu(x, y, [
    { label: python ? "Show Bytecode" : "Show Instructions", disabled: python ? !frame.path : !(session?.info.disassembly && frame.ip), run: () => (python ? showBytecode(frame) : showInstructions(frame)) },
  ]);
}

// Variables and watches.

// A row of the tree: a twisty where it holds more, its name, value and type. `open` says whether it
// is open, and `toggle` opens or closes it; `menu` gives the row's menu.
function variableRow(variable, depth, open, toggle, menu = null) {
  const row = element("div", { className: "debug-var" });
  row.style.paddingLeft = `${0.5 + depth * 0.9}rem`;
  const twisty = element("button", { className: "debug-twisty", type: "button", textContent: variable.reference ? (open ? "▾" : "▸") : "" });
  twisty.disabled = !variable.reference;
  twisty.setAttribute("aria-label", open ? `Close ${variable.name}` : `Open ${variable.name}`);
  row.append(twisty, element("span", { className: "debug-var-name", textContent: variable.name }), element("span", { className: "debug-var-value", textContent: variable.value, title: variable.kind ? `${variable.kind}: ${variable.value}` : variable.value }));
  if (variable.kind) {
    row.append(element("span", { className: "debug-var-kind", textContent: variable.kind }));
  }
  if (menu) {
    row.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      event.stopPropagation();
      menu(event.clientX, event.clientY);
    });
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

// The menu of a variable: keep its value, compare it with one kept, stop where it changes, or read
// the memory it stands in.
function variableMenu(variable, container, x, y) {
  const session = chosen();
  showMenu(x, y, [
    { label: "Keep Value", run: () => keepValue(variable) },
    { label: "Compare with Kept", disabled: !state.kept.length, items: state.kept.map((kept) => ({ label: kept.label, run: async () => compareValues(kept, await readWhole(variable)) })) },
    "-",
    { label: "Stop When It Changes", disabled: !session?.info.data, run: () => watchData(variable.name, container) },
    { label: "Show Memory", disabled: !(session?.info.memory && variable.memory), run: () => showMemory(variable.memory, variable.name) },
  ]);
}

async function variablesUnder(reference, depth, prefix, into, asked) {
  const session = chosen();
  const found = await invoke("debug_variables", { session: session.info.id, reference }).catch(() => []);
  for (const variable of found) {
    if (asked !== state.asked) {
      return;
    }
    const key = `${prefix}/${variable.name}`;
    const open = state.opened.has(key);
    const holder = variableRow(variable, depth, open, () => flip(state.opened, key), (x, y) => variableMenu(variable, reference, x, y));
    into.append(holder);
    if (open && variable.reference && depth < 12) {
      await variablesUnder(variable.reference, depth + 1, key, holder, asked);
    }
  }
}

async function drawVariables() {
  const asked = state.asked;
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
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
      invoke("debug_evaluate", { session: session.info.id, expression, frame: frame.id, context: "watch" })
        .then((found) => {
          value.textContent = found.value;
          value.title = found.kind ? `${found.kind}: ${found.value}` : found.value;
        })
        .catch((error) => {
          value.textContent = String(error);
          value.classList.add("failed");
        });
    }
  }
  if (frame) {
    const scopes = await invoke("debug_scopes", { session: session.info.id, frame: frame.id }).catch(() => []);
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
  } else if (!state.watches.length && !state.kept.length) {
    box.append(element("p", { className: "debug-empty", textContent: "Variables show here when the program stops." }));
  }
  if (state.kept.length) {
    box.append(element("div", { className: "debug-var debug-scope debug-kept-head", textContent: "Kept" }));
    for (const [at, kept] of state.kept.entries()) {
      const row = element("div", { className: "debug-var debug-kept" });
      const remove = element("button", { className: "debug-twisty", type: "button", textContent: "×", title: "Let the value go" });
      remove.addEventListener("click", () => {
        state.kept.splice(at, 1);
        drawVariables();
      });
      row.append(remove, element("span", { className: "debug-var-name", textContent: kept.label }), element("span", { className: "debug-var-value", textContent: kept.tree.value }));
      box.append(row);
    }
  }
  if (asked === state.asked) {
    parts.vars.replaceChildren(...box.children);
  }
}

// The console's field: evaluates in the chosen frame.
async function evaluate(text) {
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  write(`> ${text}\n`, "input");
  if (!session) {
    write("Nothing is being debugged.\n", "stderr");
    return;
  }
  try {
    const found = await invoke("debug_evaluate", { session: session.info.id, expression: text, frame: frame?.id ?? null, context: "repl" });
    write(`${found.value}\n`, "result");
  } catch (error) {
    write(`${String(error)}\n`, "stderr");
  }
  if (paused) {
    drawVariables();
  }
}

// Values kept to compare.

// A value read whole, as deep as KEPT_DEPTH and as wide as KEPT_WIDTH at each level.
async function readWhole(variable, depth = 0) {
  const tree = { name: variable.name, value: variable.value, kind: variable.kind, children: [] };
  if (variable.reference && depth < KEPT_DEPTH) {
    const found = await invoke("debug_variables", { session: chosen().info.id, reference: variable.reference }).catch(() => []);
    for (const child of found.slice(0, KEPT_WIDTH)) {
      tree.children.push(await readWhole(child, depth + 1));
    }
  }
  return tree;
}

async function keepValue(variable) {
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  const tree = await readWhole(variable);
  const where = frame?.path ? ` at ${frame.path.split("/").pop()}:${frame.line + 1}` : "";
  state.kept.unshift({ label: `${variable.name}${where}`, tree });
  state.kept.length = Math.min(state.kept.length, KEPT_MOST);
  say(`${variable.name} is kept as it stands${where}.`);
  drawVariables();
}

// Each field of a value, by its path of names, with its value.
function fieldsOf(tree, prefix = "", into = new Map()) {
  into.set(prefix || tree.name, tree.value);
  for (const child of tree.children) {
    fieldsOf(child, `${prefix || tree.name}.${child.name}`, into);
  }
  return into;
}

// The sheet that sets a kept value beside one read now, field by field, a field that differs marked.
function compareValues(kept, now) {
  const before = fieldsOf(kept.tree, kept.tree.name);
  const after = fieldsOf(now, kept.tree.name);
  const paths = [...new Set([...before.keys(), ...after.keys()])];
  const table = element("table", { className: "debug-compare" }, element("thead", {}, element("tr", {}, element("th", { textContent: "Field" }), element("th", { textContent: kept.label }), element("th", { textContent: "Now" }))));
  const body = element("tbody");
  let differ = 0;
  for (const path of paths) {
    const a = before.get(path);
    const b = after.get(path);
    const row = element("tr", {}, element("td", { textContent: path }), element("td", { textContent: a ?? "—" }), element("td", { textContent: b ?? "—" }));
    if (a !== b) {
      row.classList.add("differs");
      differ += 1;
    }
    body.append(row);
  }
  table.append(body);
  const form = element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `${now.name}: ${differ} of ${paths.length} field${paths.length === 1 ? "" : "s"} differ${differ === 1 ? "s" : ""}` }), table);
  sheet(form);
}

// Data, memory and instructions.

async function watchData(name, container) {
  const session = chosen();
  try {
    const watched = await invoke("debug_watch_data", { session: session.info.id, name, reference: container ?? null, bytes: null });
    say(`The program stops where ${watched} changes.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Stop When Memory Changes: a data breakpoint on the bytes at an address given.
export async function watchAddress(given) {
  const session = chosen();
  if (!session?.info.data) {
    say("The chosen session's debugger cannot stop where memory changes.", { failed: true });
    return;
  }
  const answer = given ?? (await askFor("The address to watch, and how many bytes, such as 0x7ffe1000 8", ""));
  const [address, bytes] = (answer ?? "").trim().split(/\s+/);
  if (!address) {
    return;
  }
  try {
    const watched = await invoke("debug_watch_data", { session: session.info.id, name: address, reference: null, bytes: Number(bytes) || 4 });
    say(`The program stops where ${watched} changes.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// The sheet of the bytes at `reference`: a row of sixteen a line, as hex and as text, read on as it
// is paged.
export async function showMemory(reference, label = reference) {
  const session = chosen();
  if (!session?.info.memory) {
    say("The chosen session's debugger cannot read memory.", { failed: true });
    return;
  }
  const given = reference ?? (await askFor("The address to read, such as 0x7ffe1000", ""));
  if (!given) {
    return;
  }
  let offset = 0;
  const pre = element("pre", { className: "debug-memory" });
  const head = element("h2", { textContent: `Memory at ${label ?? given}` });
  const read = async () => {
    try {
      const found = await invoke("debug_memory", { session: session.info.id, reference: given, offset, count: 256 });
      const base = BigInt(found.address);
      const lines = [];
      for (let at = 0; at < found.bytes.length; at += 16) {
        const row = found.bytes.slice(at, at + 16);
        const hex = row.map((byte) => byte.toString(16).padStart(2, "0")).join(" ").padEnd(47, " ");
        const text = row.map((byte) => (byte >= 32 && byte < 127 ? String.fromCharCode(byte) : "·")).join("");
        lines.push(`${(base + BigInt(at)).toString(16).padStart(16, "0")}  ${hex}  ${text}`);
      }
      pre.textContent = lines.join("\n") + (found.unreadable ? `\n${found.unreadable} bytes past these could not be read.` : "");
    } catch (error) {
      pre.textContent = String(error);
    }
  };
  const nav = element("div", { className: "report-foot" });
  for (const [label2, by] of [["Before", -256], ["After", 256]]) {
    const button = element("button", { type: "button", textContent: label2 });
    button.addEventListener("click", () => {
      offset += by;
      read();
    });
    nav.append(button);
  }
  sheet(element("div", { className: "sheet-report sheet-wide" }, head, pre, nav));
  await read();
}

// The sheet of the instructions about a native frame's line, the frame's own marked.
async function showInstructions(frame) {
  const session = chosen();
  try {
    const found = await invoke("debug_instructions", { session: session.info.id, reference: frame.ip, offset: -12, count: 40 });
    const pre = element("pre", { className: "debug-memory" });
    for (const one of found) {
      const line = element("div", { textContent: `${one.address}  ${one.text}${one.symbol ? `   <${one.symbol}>` : ""}${one.line !== null && one.line !== undefined ? `   line ${one.line + 1}` : ""}` });
      if (one.address === frame.ip || BigInt(one.address || 0) === BigInt(frame.ip || 0)) {
        line.className = "here";
      }
      pre.append(line);
    }
    sheet(element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `Instructions about ${frame.name}` }), pre));
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// The sheet of the bytecode of a Python frame's function, the instructions of the frame's line marked.
async function showBytecode(frame) {
  try {
    const [name, found] = await invoke("debug_bytecode", { path: frame.path, line: frame.line });
    const pre = element("pre", { className: "debug-memory" });
    for (const one of found) {
      const line = element("div", { textContent: `${one.address.padStart(5, " ")}  ${one.text}${one.line !== null && one.line !== undefined ? `   line ${one.line + 1}` : ""}` });
      if (one.line === frame.line) {
        line.className = "here";
      }
      pre.append(line);
    }
    sheet(element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `Bytecode of ${name}` }), pre));
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Show Bytecode or Instructions: those of the chosen frame, or of the cursor's line of a Python
// file where nothing is stopped.
export async function showMachineCode() {
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (frame) {
    frameMenu(frame, window.innerWidth / 3, window.innerHeight / 3);
    return;
  }
  const tab = state.hooks.tab();
  const editor = state.hooks.editor();
  if (tab?.file?.endsWith(".py") && editor?.s === tab.session) {
    await showBytecode({ path: tab.file, line: tab.session.base + editor.head().line });
  } else {
    say("Stop a program, or open a Python file, to read its machine code or its bytecode.");
  }
}

// The value under the pointer.

// The expression a place of a line stands in: the name there and the names and dots before it.
function expressionAt(text, col) {
  let from = col;
  let to = col;
  while (to < text.length && /[\w$]/.test(text[to])) {
    to += 1;
  }
  while (from > 0 && /[\w$.]/.test(text[from - 1])) {
    from -= 1;
  }
  const expression = text.slice(from, to).replace(/^\.+/, "");
  return /^[A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*$/.test(expression) ? { expression, from: from + (text.slice(from, to).length - expression.length), to } : null;
}

// The value of the name the pointer rests on in `file`, while the chosen session is stopped: a row
// for it, opened a level at a time where it holds more, or null.
export async function valueAt(file, doc, p) {
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (!session || !frame || !file) {
    return null;
  }
  const found = expressionAt(doc.line(p.line), p.col);
  if (!found) {
    return null;
  }
  let variable;
  try {
    variable = await invoke("debug_evaluate", { session: session.info.id, expression: found.expression, frame: frame.id, context: "hover" });
  } catch {
    return null;
  }
  const node = element("div", { className: "debug-hover" });
  const add = (one, depth, into) => {
    let open = false;
    let shown = null;
    const holder = variableRow(one, depth, false, () => {}, null);
    const twisty = holder.querySelector(".debug-twisty");
    const fresh = twisty.cloneNode(true);
    twisty.replaceWith(fresh);
    fresh.addEventListener("click", async () => {
      open = !open;
      fresh.textContent = open ? "▾" : "▸";
      if (open) {
        shown = element("div");
        const children = await invoke("debug_variables", { session: session.info.id, reference: one.reference }).catch(() => []);
        children.slice(0, 200).forEach((child) => add(child, depth + 1, shown));
        holder.append(shown);
      } else {
        shown?.remove();
      }
    });
    into.append(holder);
  };
  add({ ...variable, name: found.expression }, 0, node);
  return { from: { line: p.line, col: found.from }, to: { line: p.line, col: found.to }, node };
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

// Takes a session the app started as running, and hears what it told while it started.
function known(info) {
  state.sessions.set(info.id, { info, paused: null, thread: undefined });
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
    const button = element("button", { className: `debug-step ${command}`, type: "button", title: `${label} (${keys})` });
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
