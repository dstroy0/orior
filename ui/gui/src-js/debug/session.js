// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { askFor, sheet } from "../menubar.js";
import { say } from "../statusbar.js";
import { chosen, CONSOLE_LINES, element, known, parts, pausedOf, state, toggleDebugPanel } from "../debug.js";
import { allBreakpoints, breakpointsOf, exceptionsOf, steppingOf, tellBreaks } from "./breakpoints.js";
import { chooseFrame, drawFrames, drawVariables } from "./variables.js";

// The session.

export const debugging = () => state.sessions.size > 0;

// The names of the sessions still starting.
export const debugStarting = () => [...state.starting];
export const isPaused = () => Boolean(pausedOf());
export const canStepBack = () => Boolean(pausedOf() && chosen()?.info.step_back);

export function sayState(text, failed = false) {
  parts.said.textContent = text;
  parts.said.classList.toggle("failed", failed);
  const session = chosen();
  for (const button of parts.bar.querySelectorAll("button[data-needs]")) {
    const need = button.dataset.needs;
    button.disabled = need === "paused" ? !session?.paused : need === "back" ? !(session?.paused && session.info.step_back) : need === "running" ? !session || Boolean(session.paused) : need === "session" ? !session : !session && !state.last;
    // The steps backward are displayed only for a session that takes them.
    button.hidden = need === "back" && !session?.info.step_back;
  }
  drawSessions();
}

export function write(text, type = "stdout") {
  const last = parts.out.lastElementChild;
  // A piece of output with no line end runs on into the next.
  if (last && last.dataset.type === type && !last.dataset.ended) {
    last.textContent += text;
  } else {
    parts.out.append(element("div", { className: "debug-line", textContent: text }));
  }
  const line = parts.out.lastElementChild;
  line.dataset.type = type;
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
async function begin(start, name, { fresh = true, entry = false } = {}) {
  if (state.starting.has(name)) {
    say(`${name} is still starting.`, { failed: true });
    return null;
  }
  state.starting.add(name);
  try {
    toggleDebugPanel(true);
    if (fresh) {
      parts.out.replaceChildren();
    }
    sayState(`Starting ${name}…`);
    const stepping = steppingOf();
    const language = start.language ?? "python";
    const options = { breakpoints: allBreakpoints(), exceptions: exceptionsOf(language) ?? defaultExceptions(language), stepping: { mine_only: stepping.mine_only, skip: stepping.skip }, entry };
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
    state.starting.delete(name);
  }
}

// The exceptions a session in `language` stops on before any are chosen: those nothing catches.
export const defaultExceptions = (language) => (language === "python" ? ["uncaught"] : []);

export async function debugFile() {
  const tab = state.hooks.tab();
  const s = tab?.session;
  if (!s || tab.commit) {
    say("Open a file to debug it.");
    return;
  }
  await state.hooks.save(tab);
  const language = s.language?.id ?? "plaintext";
  state.last = { start: { type: "file", path: tab.file, language }, name: tab.file };
  await begin(state.last.start, tab.file);
}

// Run, Record and Debug File: the Python file in the editor run under orior's recorder, every line
// of the tree's own code it runs retained with its values, and stepped through backward as well as
// forward once the run ends.
export async function recordFile() {
  const tab = state.hooks.tab();
  const s = tab?.session;
  if (!s || tab.commit) {
    say("Open a Python file to record its run.");
    return;
  }
  if (s.language?.id !== "python") {
    say("A run is recorded for Python files.", { failed: true });
    return;
  }
  await state.hooks.save(tab);
  state.last = { start: { type: "record", path: tab.file, language: "python" }, name: tab.file };
  await begin(state.last.start, tab.file);
}

// The values a name took through a recorded run, each with the line that gave it; a press on one
// goes to that line as it ran.
export async function showHistory(variable) {
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (!session || !frame) {
    return;
  }
  let changes;
  try {
    changes = await invoke("debug_history", { session: session.info.id, frame: frame.id, name: variable.name });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const body = element("tbody");
  const form = element("div", { className: "sheet-report sheet-wide" });
  let dialog = null;
  for (const change of changes) {
    const row = element("tr", { className: change.before ? "" : "later", tabIndex: 0 }, element("td", { textContent: `${change.path.split("/").pop()}:${change.line + 1}` }), element("td", { textContent: change.value ?? "gone" }));
    const go = () => {
      invoke("debug_goto", { session: session.info.id, step: change.step }).catch((error) => say(String(error), { failed: true }));
      dialog?.close();
    };
    row.addEventListener("click", go);
    row.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        go();
      }
    });
    body.append(row);
  }
  const table = element("table", { className: "debug-compare debug-history" }, element("thead", {}, element("tr", {}, element("th", { textContent: "Given at" }), element("th", { textContent: "Value" }))), body);
  form.append(element("h2", { textContent: `${variable.name}: ${changes.length} value${changes.length === 1 ? "" : "s"} through the run` }), changes.length ? table : element("p", { className: "debug-empty", textContent: "The run gave it no value in this frame." }));
  dialog = sheet(form);
}

// Run, Debug File with Its C: the Python file in the editor debugged with the native code of its
// process, the tree's C, C++ and Cython, by a session of its own attached to the same process; a step
// into a call of that code from Python goes into it, and a step out of it comes back to the Python.
export async function debugMixed() {
  const tab = state.hooks.tab();
  const s = tab?.session;
  if (!s || tab.commit || s.language?.id !== "python") {
    say("Open the Python file that calls the C to debug them together.");
    return;
  }
  await state.hooks.save(tab);
  state.last = { start: { type: "file", path: tab.file, language: "python" }, name: tab.file, mixed: true };
  await beginMixed();
}

async function beginMixed() {
  state.mixNext = true;
  await begin(state.last.start, state.last.name, { entry: true });
  state.mixNext = false;
}

// Attaches the native debugger to the process of Python session `session`, as its child, and lets
// the Python run from where it stopped before its first line.
export async function attachNative(session, thread) {
  const id = session.info.id;
  const listed = await invoke("debug_sessions").catch(() => []);
  const pid = listed.find((one) => one.id === id)?.process ?? session.info.process;
  if (pid) {
    sayState(`Attaching to the native code of ${session.info.name}…`);
    try {
      const native = await invoke("debug_start", { start: { type: "process", pid, language: "c", parent: id }, options: { breakpoints: allBreakpoints(), exceptions: [], stepping: { mine_only: true, skip: [] }, entry: false } });
      known(native);
      drawSessions();
      write(`${native.name} is debugged with ${session.info.name}.\n`, "console");
    } catch (error) {
      write(`${String(error)}\n`, "stderr");
    }
  } else {
    write(`${session.info.name} named no process for the native debugger to attach to.\n`, "stderr");
  }
  session.paused = null;
  sayState(`Running ${session.info.name} under ${session.info.tool}`);
  await invoke("debug_step", { session: id, how: "continue", thread }).catch((error) => write(`${String(error)}\n`, "stderr"));
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
  if (state.last.start.type === "file" || state.last.start.type === "record") {
    await state.hooks.open(state.last.start.path);
  }
  if (state.last.mixed) {
    await beginMixed();
    return;
  }
  await begin(state.last.start, state.last.name);
}

// Debugs the Python test `id`, named as pytest names it, run by pytest under the debugger with the
// breakpoints set.
export async function debugTest(id) {
  const language = /\.rs(::|$)/.test(id) ? "rust" : "python";
  state.last = { start: { type: "test", id, language }, name: id };
  await begin(state.last.start, id);
}

// Run, Attach to Process: the machine's processes in the quick open; the one chosen is debugged, by
// debugpy where its program is Python and by the native debugger where not.
export async function attachProcess() {
  const found = await invoke("debug_processes").catch(() => []);
  const { openPalette } = await import("../palette.js");
  openPalette("", {
    custom: {
      placeholder: "Attach to a process: type to filter, Enter to debug it",
      empty: "No process is found.",
      items: found.map((one) => {
        const label = `${one.name} ${one.pid}`;
        return { text: `${label} ${one.command}`, label, detail: one.command.slice(0, 160), run: () => attachTo(one) };
      }),
    },
  });
}

async function attachTo(process) {
  const language = /python/i.test(process.name) ? "python" : "c";
  const start = { type: "process", pid: process.pid, language };
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
  const start = { type: "address", host: host || "127.0.0.1", port: Number(port), remote: folder.join(" ") || null, language: "python" };
  state.last = { start, name: `${start.host}:${start.port}` };
  await begin(start, state.last.name);
}

// Debugs the Python a program listening at host:port runs next, the session named `name`, stopping
// first at `first`, a line as { path, line }, where that file has no breakpoint of its own. Gives
// what ends the session once that code has run, or null where it did not start.
export async function debugNext(host, port, name, first = null) {
  state.once = first && !breakpointsOf(first.path) ? first : null;
  const info = await begin({ type: "address", host, port, remote: null, language: "python" }, name);
  if (!info) {
    state.once = null;
    return null;
  }
  return async () => {
    if (state.sessions.has(info.id)) {
      await invoke("debug_stop", { session: info.id }).catch(() => {});
      ended(info.id, "Ended");
    }
  };
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

export function ended(id, text) {
  const gone = [id, ...[...state.sessions.values()].filter((one) => one.info.parent === id).map((one) => one.info.id)];
  gone.forEach((one) => state.sessions.delete(one));
  if (!state.sessions.size) {
    state.once = null;
  }
  if (gone.includes(state.chosen)) {
    state.chosen = state.sessions.size ? [...state.sessions.keys()].at(-1) : null;
  }
  state.asked += 1;
  drawFrames();
  drawVariables();
  sayState(state.sessions.size ? summaryOf(chosen()) : text);
  state.hooks.repaint();
}

export function summaryOf(session) {
  if (!session) {
    return "";
  }
  return session.paused ? session.said ?? "Stopped" : `Running ${session.info.name} under ${session.info.tool}`;
}

export async function step(how) {
  const session = chosen();
  let thread = session?.paused?.thread ?? session?.thread;
  // A program that has not stopped yet is paused on its first thread, as the debugger lists them.
  if (session && thread === undefined && how === "pause") {
    thread = (await invoke("debug_threads", { session: session.info.id }).catch(() => []))[0]?.id;
  }
  if (!session || thread === undefined) {
    return;
  }
  if (how !== "pause" && !session.paused) {
    return;
  }
  try {
    // A step into from Python goes on into the tree's C where a native session debugs the same
    // process.
    if (how === "stepIn" && session.info.language === "python") {
      await invoke("debug_step_across", { session: session.info.id, thread });
    } else {
      await invoke("debug_step", { session: session.info.id, how, thread });
    }
  } catch (error) {
    write(`${String(error)}\n`, "stderr");
  }
}

// Chooses the session the bar and the columns show.
export function choose(id) {
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

export function drawSessions() {
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
