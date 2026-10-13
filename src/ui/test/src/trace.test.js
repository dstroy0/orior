// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window's own trace: joined to a run started apart from the window, each traced call is a line of
// the trace's own buffer, indented by the calls it was made inside, and the lines reach the run's buffer
// as the blob window.trace through the backend's primary channel; joined to no run, nothing is gathered.

import { invoke } from "../../src/bridge.js";
import { enter, joinRun, leave, traced, traceJoined, traceLines, tracing, TRACE_BLOB } from "../../src/trace.js";

const { test, assert, ui } = window.__harness;

const LINE = /^( *)([\w.]+) (\d+\.\d{3}) (\d+\.\d{3})$/;

test("joined to a run, each traced call reaches the run as a line of window.trace, nested as it was called, and joined to none nothing is gathered", async () => {
  const writer = window.__harness.servers.ORIOR_TEST_QRY_WRITER;
  assert.ok(writer, "the harness built the run's writer");
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("folder_create", { path: "build/trace" }).catch(() => {});
  const relative = "build/trace/query1-20261011T130000.qry";
  const qry = `${root}/${relative}`;
  await ui.run(writer, ["start", qry], { wait: false });
  try {
    await ui.run(writer, ["ready", qry]);
    await joinRun(null);
    assert.equal(tracing(), false);
    assert.equal(traced("unseen", () => 7), 7, "a traced call gives its value");
    assert.equal(traceLines(), "", "joined to no run, nothing is gathered");

    assert.ok(await joinRun(relative), "joined");
    assert.equal(tracing(), true);
    const outer = enter("outer");
    traced("inner", () => {
      traced("innermost", () => {});
    });
    leave(outer);
    const taken = traceLines().split("\n").filter(Boolean);
    const at = taken.findIndex((line) => LINE.exec(line)?.[2] === "outer");
    assert.ok(at >= 0, `the outer call is a line: ${taken.join(" | ")}`);
    const read = taken.slice(at, at + 3).map((line) => LINE.exec(line));
    assert.deepEqual(read.map((one) => [one[1].length, one[2]]), [[0, "outer"], [2, "inner"], [4, "innermost"]], "each indented two spaces a call deeper");
    const [entered, spent] = [Number(read[0][3]), Number(read[0][4])];
    assert.ok(Number(read[1][3]) >= entered && Number(read[1][3]) + Number(read[1][4]) <= entered + spent + 0.001, "an inner call runs inside its outer one");

    traced("handed", () => {});
    await ui.waitFor(async () => (await invoke("qry_joined", { path: relative, name: TRACE_BLOB })).includes("handed "), 5000);
    const joined = await traceJoined();
    assert.ok(joined.path.replaceAll("\\", "/").endsWith(relative), `the window's run: ${joined.path}`);
    assert.ok(joined.queued.handed > 0, "the backend handed the trace");

    traced("last", () => {});
    await joinRun(null);
    assert.equal(tracing(), false, "joined to none, the trace stops");
    await ui.waitFor(async () => (await invoke("qry_joined", { path: relative, name: TRACE_BLOB })).includes("last "), 5000);
    await ui.run(writer, ["stop", qry]);
    const kept = await invoke("qry_joined", { path: relative, name: TRACE_BLOB });
    assert.ok(kept.includes("handed ") && kept.includes("last "), "the run's file keeps every line handed");
    assert.ok(kept.split("\n").filter(Boolean).every((line) => LINE.test(line)), "every line of the trace reads as a call");
  } finally {
    await joinRun(null);
    await ui.run(writer, ["stop", qry]).catch(() => {});
  }
});
