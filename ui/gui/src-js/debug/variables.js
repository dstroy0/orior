// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { showMenu } from "../menu.js";
import { say } from "../statusbar.js";
import { chosen, element, parts, pausedOf, state, WATCHES } from "../debug.js";
import { editBreakpoint, keepExceptions, setBreakpoint, setMessageBreakpoint, steppingOf, tellBreaks, toggleBreakpoint } from "./breakpoints.js";
import { attachNative, sayState, showHistory, write } from "./session.js";
import { compareValues, keepValue, readWhole, showBytecode, showInstructions, showMemory, watchData } from "./values.js";

// A stop.

export async function stopped(id, body) {
  const session = state.sessions.get(id);
  if (!session) {
    return;
  }
  if (body.reason === "entry" && state.mixing.has(id)) {
    state.mixing.delete(id);
    await attachNative(session, body.threadId);
    return;
  }
  if (state.once) {
    const path = state.once.path;
    state.once = null;
    tellBreaks(path);
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
  // A recorded run says which of its steps it stands at.
  const why = session.info.recorded && body.description ? `at ${body.description}` : { breakpoint: "at a breakpoint", step: "after a step", pause: "paused", exception: "on an exception", entry: "at the start", "data breakpoint": "where its data changed" }[session.paused.reason] ?? session.paused.reason;
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

export async function chooseFrame(at) {
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

export function drawFrames() {
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
export function frameMenu(frame, x, y) {
  const session = chosen();
  const python = session?.info.language === "python";
  const at = String(pausedOf()?.frames.indexOf(frame) ?? 0);
  showMenu(x, y, [
    { label: python ? "Show Bytecode" : "Show Instructions", command: "frame-code", args: [at], disabled: python ? !frame.path : !(session?.info.disassembly && frame.ip), run: () => (python ? showBytecode(frame) : showInstructions(frame)) },
  ]);
}

// Variables and watches.

// A row of the tree: a twisty where it holds more, its name, value and type. `open` says whether it
// is open, and `toggle` opens or closes it; `menu` gives the row's menu.
export function variableRow(variable, depth, open, toggle, menu = null) {
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
  const name = variable.name;
  showMenu(x, y, [
    { label: "Keep Value", command: "variable-keep", args: [name], run: () => keepValue(variable) },
    { label: "Compare with Kept", disabled: !state.kept.length, items: state.kept.map((kept) => ({ label: kept.label, command: "variable-compare", args: [name, kept.label], run: async () => compareValues(kept, await readWhole(variable)) })) },
    "-",
    { label: "Stop When It Changes", command: "variable-watch", args: [name], disabled: !session?.info.data, run: () => watchData(variable.name, container) },
    { label: "Show Memory", command: "variable-memory", args: [name], disabled: !(session?.info.memory && variable.memory), run: () => showMemory(variable.memory, variable.name) },
    ...(session?.info.recorded ? ["-", { label: "Values Through the Run", command: "variable-history", args: [name], run: () => showHistory(variable) }] : []),
  ]);
}

// The variable `name` of the frame stopped at, as the first of its scopes that holds it gives it,
// with that scope's reference, or null.
async function variableNamed(name) {
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (!session || !frame) {
    say("Nothing is stopped: a variable is read from the frame a program stops at.", { failed: true });
    return null;
  }
  for (const scope of await invoke("debug_scopes", { session: session.info.id, frame: frame.id }).catch(() => [])) {
    const found = (await invoke("debug_variables", { session: session.info.id, reference: scope.reference }).catch(() => [])).find((one) => one.name === name);
    if (found) {
      return { variable: found, container: scope.reference };
    }
  }
  say(`The frame stopped at holds no variable ${name}.`, { failed: true });
  return null;
}

// The debugger's menus as commands, by the names commands.json gives them, each run from the menus or
// from the command line: a breakpoint by its file and its line counted from 1, an exception by the
// debugger's name for it, a frame by its place in the stack from 0, a variable by its name.
const lineOf = (line) => Math.max(0, Number(line) - 1);

export const debugCommands = {
  "breakpoint-edit": ([path, line, condition, hits, log]) =>
    condition === undefined ? editBreakpoint(path, lineOf(line)) : setBreakpoint(path, lineOf(line), { condition, hits: hits ?? "", log: log ?? "" }),
  "breakpoint-message": ([path, line, ...message]) => (message.length ? setMessageBreakpoint(path, lineOf(line), message.join(" ")) : editBreakpoint(path, lineOf(line), { focus: "log" })),
  "breakpoint-remove": ([path, line]) => state.breaks.get(path)?.has(lineOf(line)) && toggleBreakpoint(path, lineOf(line)),
  "debug-exception": async ([name, on]) => {
    const session = chosen();
    const filter = session?.info.filters.find((one) => one.filter === name || one.label === name);
    if (!filter) {
      say(`The session's debugger stops on no exception named ${name}.`, { failed: true });
      return;
    }
    filter.on = on === "on" ? true : on === "off" ? false : !filter.on;
    const kept = session.info.filters.filter((one) => one.on).map((one) => one.filter);
    keepExceptions(session.info.language, kept);
    await invoke("debug_exceptions", { session: session.info.id, on: kept }).catch((error) => say(String(error), { failed: true }));
  },
  "frame-code": ([at]) => {
    const paused = pausedOf();
    const frame = paused?.frames[at === undefined ? paused.at : Number(at)];
    if (!frame) {
      say("Nothing is stopped: a frame's code is read where a program stops.", { failed: true });
      return;
    }
    return chosen()?.info.language === "python" ? showBytecode(frame) : showInstructions(frame);
  },
  "variable-keep": async ([name]) => {
    const found = await variableNamed(name);
    return found && keepValue(found.variable);
  },
  "variable-compare": async ([name, label]) => {
    const found = await variableNamed(name);
    const kept = state.kept.find((one) => one.label === label || one.label.startsWith(`${label ?? name}`)) ?? state.kept[0];
    return found && kept && compareValues(kept, await readWhole(found.variable));
  },
  "variable-watch": async ([name]) => {
    const found = await variableNamed(name);
    return found && watchData(name, found.container);
  },
  "variable-memory": async ([name]) => {
    const found = await variableNamed(name);
    return found && (found.variable.memory ? showMemory(found.variable.memory, name) : say(`The debugger gives ${name} no place in memory.`, { failed: true }));
  },
  "variable-history": async ([name]) => {
    const found = await variableNamed(name);
    return found && showHistory(found.variable);
  },
};


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

export async function drawVariables() {
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
export async function evaluate(text) {
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
