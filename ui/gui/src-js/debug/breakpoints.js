// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, pick } from "../bridge.js";
import { showMenu } from "../menu.js";
import { sheet } from "../menubar.js";
import { say } from "../statusbar.js";
import { BREAKS, chosen, element, EXCEPTIONS, pausedOf, state, STEPPING, treeKey, WATCHES } from "../debug.js";
import { debugCommands, drawFrames, drawVariables } from "./variables.js";

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

function retainBreaks() {
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

// Each breakpoint retained for a file, a line alone or a breakpoint whole, as a Map of its line to it.
function breaksOf(retained) {
  return new Map(retained.map((one) => (typeof one === "number" ? [one, { bound: true }] : [one.line, { bound: true, condition: one.condition ?? "", hits: one.hits ?? "", log: one.log ?? "" }])));
}

// Reads the breakpoints retained for the tree that is open.
export function loadBreakpoints() {
  state.breaks = new Map();
  for (const [path, retained] of Object.entries(readKept(BREAKS)[treeKey()] ?? {})) {
    state.breaks.set(path, breaksOf(retained));
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
export function allBreakpoints() {
  const out = {};
  for (const [path, lines] of state.breaks) {
    if (lines.size) {
      out[path] = [...lines].map(([line, one]) => specOf(line, one));
    }
  }
  if (state.once && !out[state.once.path]) {
    out[state.once.path] = [specOf(state.once.line, {})];
  }
  return out;
}

export async function tellBreaks(path) {
  if (!state.sessions.size) {
    return;
  }
  const lines = state.breaks.get(path) ?? new Map();
  const once = state.once?.path === path && !lines.size ? [specOf(state.once.line, {})] : [];
  const placed = await invoke("debug_breakpoints", { path, breakpoints: [...lines].map(([line, one]) => specOf(line, one)).concat(once) }).catch(() => null);
  if (placed) {
    const before = [...lines.values()];
    state.breaks.set(path, new Map(placed.slice(0, before.length).map(([line, bound], at) => [line, { ...before[at], bound }])));
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
  retainBreaks();
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
export function setBreakpoint(path, line, change) {
  if (!state.breaks.has(path)) {
    state.breaks.set(path, new Map());
  }
  const lines = state.breaks.get(path);
  lines.set(line, { bound: true, condition: "", hits: "", log: "", ...lines.get(line), ...change });
  retainBreaks();
  state.hooks.repaint();
  tellBreaks(path);
}

// Puts on `line` of `path` a breakpoint that writes `log` in place of stopping.
export function setMessageBreakpoint(path, line, log) {
  setBreakpoint(path, line, { log });
}

// The menu of the breakpoint strip on `line` of `path`: edit the breakpoint there, set one that
// writes a message, or take it away.
export function breakpointMenu(path, line, x, y) {
  const here = state.breaks.get(path)?.get(line);
  const at = String(line + 1);
  showMenu(x, y, [
    { label: here ? "Edit Breakpoint…" : "Add Breakpoint…", command: "breakpoint-edit", args: [path, at], run: () => editBreakpoint(path, line) },
    { label: "Add Message Breakpoint…", command: "breakpoint-message", args: [path, at], disabled: Boolean(here?.log), run: () => editBreakpoint(path, line, { focus: "log" }) },
    "-",
    { label: "Remove Breakpoint", command: "breakpoint-remove", args: [path, at], disabled: !here, run: () => toggleBreakpoint(path, line) },
  ]);
}

// The sheet that edits a breakpoint: the expression it stops only where it holds, the count of hits
// it stops at, and the message it writes in place of stopping.
export function editBreakpoint(path, line, { focus = "condition" } = {}) {
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
  const retained = { breakpoints: Object.fromEntries([...state.breaks].filter(([, lines]) => lines.size).map(([path, lines]) => [path, [...lines].map(([line, one]) => specOf(line, one))])), watches: state.watches };
  try {
    await invoke("file_write_any", { path: file, text: `${JSON.stringify(retained, null, 2)}\n` });
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
    const retained = JSON.parse(await invoke("file_read_any", { path: file }));
    for (const [path, list] of Object.entries(retained.breakpoints ?? {})) {
      state.breaks.set(path, new Map([...(state.breaks.get(path) ?? new Map()), ...breaksOf(list)]));
      tellBreaks(path);
    }
    for (const watch of retained.watches ?? []) {
      if (!state.watches.includes(watch)) {
        state.watches.push(watch);
      }
    }
    localStorage.setItem(WATCHES, JSON.stringify(state.watches));
    retainBreaks();
    state.hooks.repaint();
    drawVariables();
    say(`The breakpoints and watches of ${file} are taken in.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Exceptions and stepping.

// The exceptions a session in `language` stops on, by the adapter's names for them, as last chosen.
export function exceptionsOf(language) {
  return readKept(EXCEPTIONS)[language] ?? null;
}

export function retainExceptions(language, on) {
  const all = readKept(EXCEPTIONS);
  all[language] = on;
  localStorage.setItem(EXCEPTIONS, JSON.stringify(all));
}

// The stepping the tree asks for: whether a step retains to its own code, the patterns a step passes
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
export function exceptionsMenu(button) {
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
      command: "debug-exception",
      args: [filter.filter, filter.on ? "off" : "on"],
      run: () => debugCommands["debug-exception"]([filter.filter, filter.on ? "off" : "on"]),
    })),
    { anchor: button },
  );
}
