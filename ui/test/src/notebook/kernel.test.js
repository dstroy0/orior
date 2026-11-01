// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A notebook's kernel: a cell run on the kernel the notebook names, started where none runs, what the kernel says
// heard into the cell's outputs, what it says before the run's id comes back kept for it, and the notebook checked
// from the top in a kernel of its own.

import { state } from "../../../gui/src-js/notebook.js";
import { checkFromTop, heard, heardCheck, runCell, startKernel, stopKernel } from "../../../gui/src-js/notebook/kernel.js";
import { code, markdown, openedNotebook } from "../../notebooks.js";
import { putBackAsFound, told } from "../../menus.js";

const { test, assert, ui, calls } = window.__harness;

const PYTHON = { name: "python3", display_name: "Python 3", language: "python" };

// Runs `work` with the machine's kernels read as `specs`, a kernel started, run and stopped answered by the test,
// and the kernels the machine has read again after it.
async function withKernels(specs, work) {
  const kept = state.specs;
  state.specs = specs;
  let run = 0;
  calls.answer("kernel_start", () => null);
  calls.answer("kernel_stop", () => null);
  calls.answer("kernel_execute", ({ key }) => `${key.startsWith("check:") ? "check" : "run"}-${++run}`);
  try {
    await work();
  } finally {
    state.specs = kept;
  }
}

const said = (parent, msg_type, content = {}) => ({ parent, msg_type, content, channel: "iopub" });

test("a cell runs on a kernel started for it, and what the kernel says fills its outputs and count", async () => {
  await withKernels([PYTHON], async () => {
    const book = await openedNotebook("run", [code("print('ab')"), markdown("x")]);
    const cell = book.cells[0];
    await runCell(book, cell);
    assert.equal(book.kernel.state, "idle");
    assert.ok(cell.running && book.waiting.get("run-1") === cell);
    heard(book, said("run-1", "status", { execution_state: "busy" }));
    assert.equal(book.kernel.state, "busy");
    heard(book, said("run-1", "execute_input", { execution_count: 7 }));
    heard(book, said("run-1", "stream", { name: "stdout", text: "a" }));
    heard(book, said("run-1", "stream", { name: "stdout", text: "b\n" }));
    heard(book, said("run-1", "execute_result", { execution_count: 7, data: { "text/plain": "3" } }));
    heard(book, said("run-1", "execute_reply", { status: "ok" }));
    assert.ok(!cell.running && !book.waiting.has("run-1"));
    assert.equal(cell.count, 7);
    assert.deepEqual(cell.outputs.map((one) => one.output_type), ["stream", "execute_result"]);
    assert.equal(cell.outputs[0].text, "ab\n", "a stream's text goes on from its last");
    assert.equal(cell.node.querySelector(".nb-count").textContent, "[7]");
    await runCell(book, book.cells[1]);
    assert.equal(book.cells[1].runs.length, 0, "a Markdown cell does not run");
    await stopKernel(book);
    assert.equal(book.kernel.state, "none");
  });
});

test("what the kernel says of a run before its id comes back is kept for it, and an aborted run is not counted", async () => {
  await withKernels([PYTHON], async () => {
    const book = await openedNotebook("early", [code("x")]);
    const cell = book.cells[0];
    heard(book, said("run-1", "stream", { name: "stdout", text: "early\n" }));
    assert.ok(book.early.has("run-1"));
    await runCell(book, cell);
    assert.equal(cell.outputs[0]?.text, "early\n");
    heard(book, said("run-1", "execute_reply", { status: "aborted" }));
    assert.deepEqual(cell.runs, [], "the run taken back");
    await stopKernel(book);
  });
});

test("with no kernel for the notebook's language it is said, a kernel with no ipykernel offers it, and the one started is named", async () => {
  await withKernels([], async () => {
    const book = await openedNotebook("none", [code("x")]);
    await runCell(book, book.cells[0]);
    assert.ok(told(/^The machine has no kernel for python/));
    assert.ok(!(await startKernel(book, { ...PYTHON, install: "pip install ipykernel" })));
    assert.ok(told(/^Python 3 has no ipykernel; press here to install it/));
    assert.ok(await startKernel(book, { name: "py-other", display_name: "Other", language: "python" }));
    assert.equal(book.raw.metadata.kernelspec.name, "py-other", "the kernel started is the one the file names");
    assert.ok(book.changed);
    await stopKernel(book);
  });
});

test("Check from the Top runs each code cell in a kernel of its own and says the first that fails", async () => {
  await withKernels([PYTHON], async () => {
    const book = await openedNotebook("check", [code("a = 1"), markdown("x"), code("b = c")]);
    const checking = checkFromTop(book);
    await ui.waitFor(() => book.check?.cells.size === 2, 3000, "each code cell asked of the check's kernel");
    const [first, second] = [...book.check.cells.keys()];
    heardCheck(book, said(first, "execute_reply", { status: "ok" }));
    heardCheck(book, said(second, "error", { ename: "NameError", evalue: "name 'c' is not defined" }));
    heardCheck(book, said(second, "execute_reply", { status: "error" }));
    await checking;
    assert.ok(told(/^From the top, cell 3 fails: NameError: name 'c' is not defined/));
    assert.equal(book.check, null);
    assert.equal(book.kernel.state, "none", "the notebook's own kernel as it was");
  });
});

test("the window is put back as the file found it", putBackAsFound);
