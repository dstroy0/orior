// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's kernel: a cell run on it, or debugged, what it says heard into the cell's outputs, and the kernel
// started, restarted, stopped, and the notebook run and checked from the top.

import { invoke } from "../bridge.js";
import { debugNext } from "../debug.js";
import { say } from "../statusbar.js";
import { runInTerminal } from "../terminal.js";
import { blurCell, cellFile, cellName, drawBar, focusCell, state } from "../notebook.js";
import { drawMarks, drawOutputs } from "./draw.js";

// Runs a code cell on the notebook's kernel, starting the kernel where none runs.
export async function runCell(book, cell) {
  if (cell.kind !== "code") {
    if (book.focused === cell) {
      blurCell(book);
    }
    return;
  }
  if (!(await ensureKernel(book))) {
    return;
  }
  cell.running = true;
  cell.outputs = [];
  cell.runs.push(++book.clock);
  drawOutputs(book, cell);
  drawMarks(book);
  try {
    const id = await invoke("kernel_execute", { key: book.path, code: cell.session.doc.text() });
    book.waiting.set(id, cell);
    const held = book.early.get(id) ?? [];
    book.early.delete(id);
    held.forEach((message) => heard(book, message));
  } catch (error) {
    cell.running = false;
    say(String(error), { failed: true });
    drawMarks(book);
  }
}

// Runs a Python code cell under the debugger, which stops at the cell's breakpoints, or at its first
// line where it has none, and lets go of the kernel once the cell has run.
export async function debugCell(book, cell) {
  const file = cellFile(book, cell);
  if (!file || !(await ensureKernel(book))) {
    return;
  }
  const code = cell.session.doc.text();
  try {
    const port = await invoke("kernel_debug", { key: book.path, file, code });
    const first = code.split("\n").findIndex((line) => line.trim() && !line.trim().startsWith("#"));
    cell.debugged = await debugNext("127.0.0.1", port, `${cellName(book, cell)} of ${book.path}`, first >= 0 ? { path: file, line: first } : null);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  if (!cell.debugged) {
    return;
  }
  await runCell(book, cell);
  if (!cell.running) {
    cell.debugged?.();
    cell.debugged = null;
  }
}

// What a kernel said, given to the cell whose run it answers.
export function heard(book, message) {
  const cell = book.waiting.get(message.parent);
  const content = message.content ?? {};
  if (message.msg_type === "status" && !message.parent.startsWith("check")) {
    book.kernel.state = content.execution_state ?? book.kernel.state;
    drawBar(book);
  }
  if (!cell) {
    if (message.parent && message.channel !== "log") {
      book.early.set(message.parent, [...(book.early.get(message.parent) ?? []), message]);
      if (book.early.size > 64) {
        book.early.delete(book.early.keys().next().value);
      }
    }
    return;
  }
  switch (message.msg_type) {
    case "execute_input":
      cell.count = content.execution_count ?? cell.count;
      break;
    case "stream": {
      const last = cell.outputs.at(-1);
      if (last?.output_type === "stream" && last.name === content.name) {
        last.text += content.text;
      } else {
        cell.outputs.push({ output_type: "stream", name: content.name, text: content.text });
      }
      break;
    }
    case "execute_result":
      cell.count = content.execution_count ?? cell.count;
      cell.outputs.push({ output_type: "execute_result", data: content.data, metadata: content.metadata ?? {}, execution_count: content.execution_count });
      break;
    case "display_data":
      cell.outputs.push({ output_type: "display_data", data: content.data, metadata: content.metadata ?? {} });
      break;
    case "error":
      cell.outputs.push({ output_type: "error", ename: content.ename, evalue: content.evalue, traceback: content.traceback ?? [] });
      break;
    case "clear_output":
      cell.outputs = [];
      break;
    case "execute_reply":
      cell.running = false;
      book.waiting.delete(message.parent);
      if (content.status === "aborted") {
        cell.runs.pop();
      }
      cell.debugged?.();
      cell.debugged = null;
      break;
    default:
      return;
  }
  book.changed = true;
  drawOutputs(book, cell);
  drawMarks(book);
  state.hooks.changed();
}

// The kernel the notebook runs on, started where none runs: the one its file names, else the first
// of its language. Says whether one runs.
async function ensureKernel(book) {
  if (book.kernel.state !== "none" && book.kernel.state !== "dead") {
    return true;
  }
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === book.kernelName) ?? specs.find((one) => one.language.toLowerCase() === book.languageName && !one.install) ?? specs.find((one) => one.language.toLowerCase() === book.languageName);
  if (!spec) {
    say(`The machine has no kernel for ${book.languageName}: Jupyter's kernels for it are found where Jupyter keeps them`, { failed: true });
    return false;
  }
  return startKernel(book, spec);
}

export async function kernelSpecs() {
  state.specs = state.specs ?? (await invoke("kernel_specs").catch(() => []));
  return state.specs;
}

export async function startKernel(book, spec) {
  if (spec.install) {
    say(`${spec.display_name} has no ipykernel; press here to install it`, { failed: true, act: () => runInTerminal(spec.install) });
    return false;
  }
  book.kernel = { state: "starting", display: spec.display_name, name: spec.name };
  drawBar(book);
  try {
    await invoke("kernel_start", { key: book.path, name: spec.name });
    book.kernel.state = "idle";
    if (book.kernelName !== spec.name) {
      book.kernelName = spec.name;
      book.raw.metadata = { ...(book.raw.metadata ?? {}), kernelspec: { name: spec.name, display_name: spec.display_name, language: spec.language } };
      book.changed = true;
      state.hooks.changed();
    }
    drawBar(book);
    return true;
  } catch (error) {
    book.kernel.state = "dead";
    drawBar(book);
    say(String(error), { failed: true });
    return false;
  }
}

export async function restartKernel(book) {
  for (const cell of book.cells) {
    cell.running = false;
  }
  book.waiting.clear();
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === book.kernel.name ?? book.kernelName);
  book.kernel.state = "none";
  for (const cell of book.cells) {
    cell.runs = [];
    cell.edited = 0;
  }
  book.gone = [];
  drawMarks(book);
  return spec ? startKernel(book, spec) : ensureKernel(book);
}

export async function stopKernel(book) {
  if (book.kernel.state === "none") {
    return;
  }
  const ended = await invoke("kernel_stop", { key: book.path }).catch((error) => say(String(error), { failed: true }));
  book.kernel.state = "none";
  book.cells.forEach((cell) => (cell.running = false));
  book.waiting.clear();
  drawBar(book);
  drawMarks(book);
  if (ended) {
    say("The kernel did not stop when asked, and its process was ended");
  }
}

export async function runAll(book) {
  for (const cell of book.cells) {
    await runCell(book, cell);
  }
}

// Runs every code cell from the first in a kernel of the notebook's own kind started for it alone,
// and says the first that fails, or that each ran; the notebook's own kernel and outputs stay as
// they are.
export async function checkFromTop(book) {
  const specs = await kernelSpecs();
  const spec = specs.find((one) => one.name === (book.kernel.name ?? book.kernelName)) ?? specs.find((one) => one.language.toLowerCase() === book.languageName && !one.install);
  if (!spec) {
    say(`The machine has no kernel for ${book.languageName} to check the notebook in`, { failed: true });
    return;
  }
  const key = `check:${book.path}`;
  book.check = { cells: new Map(), early: new Map(), failed: null, left: 0, done: null };
  drawBar(book);
  try {
    await invoke("kernel_start", { key, name: spec.name });
    const finished = new Promise((resolve) => (book.check.done = resolve));
    const code = book.cells.filter((cell) => cell.kind === "code" && cell.session.doc.text().trim());
    book.check.left = code.length;
    for (const cell of code) {
      const id = await invoke("kernel_execute", { key, code: cell.session.doc.text() });
      book.check.cells.set(id, cell);
      const held = book.check.early.get(id) ?? [];
      book.check.early.delete(id);
      held.forEach((message) => heardCheck(book, message));
    }
    if (code.length === 0) {
      book.check.done();
    }
    await finished;
    const failed = book.check.failed;
    say(failed ? `From the top, ${cellName(book, failed.cell)} fails: ${failed.ename}: ${failed.evalue}` : `From the top, each of the ${code.length} code cells runs`, { failed: Boolean(failed), act: failed ? () => focusCell(book, failed.cell) : null });
  } catch (error) {
    say(String(error), { failed: true });
  } finally {
    invoke("kernel_stop", { key }).catch(() => {});
    book.check = null;
    drawBar(book);
  }
}

export function heardCheck(book, message) {
  const check = book.check;
  const cell = check?.cells.get(message.parent);
  if (!cell) {
    if (check && message.parent) {
      check.early.set(message.parent, [...(check.early.get(message.parent) ?? []), message]);
    }
    return;
  }
  if (message.msg_type === "error" && !check.failed) {
    check.failed = { cell, ename: message.content.ename, evalue: message.content.evalue };
  }
  if (message.msg_type === "execute_reply") {
    check.left -= 1;
    if (check.left <= 0 || message.content.status !== "ok") {
      check.done?.();
    }
  }
}
