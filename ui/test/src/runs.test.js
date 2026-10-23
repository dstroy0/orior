// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Runs: a query run started apart from the window, by the run's own writer of src/cu, shows as it runs,
// its blobs as they are handed, a panic the moment it lands, a conference's accounts as they are given
// with the overseer's ruling, its folder's lessons beside it, and its table once it ends.

import { invoke } from "../../gui/src-js/bridge.js";
import { showGroup } from "../../gui/src-js/explorer.js";
import { conferenceLines } from "../../gui/src-js/runs.js";

const { test, assert, ui } = window.__harness;

const rowOf = (name) => [...document.querySelectorAll("#runs .runs-run")].find((row) => row.textContent.includes(name));
const shownText = () => document.getElementById("runs").textContent;

test("a conference's lines are each expert's account and the overseer's ruling", () => {
  assert.deepEqual(conferenceLines("carrier: it hung\n\noverseer: a stall\nno one said this"), [
    { who: "carrier", said: "it hung", ruling: false },
    { who: "overseer", said: "a stall", ruling: true },
    { who: "", said: "no one said this", ruling: false },
  ]);
});

test("a run started apart from the window shows as it runs: its blobs as handed, a panic as it lands, a conference with its ruling, its lessons, and its table once it ends", async () => {
  const writer = window.__harness.servers.ORIOR_TEST_QRY_WRITER;
  assert.ok(writer, "the harness built the run's writer");
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("folder_create", { path: "build/qry" }).catch(() => {});
  await invoke("file_write", { path: "build/qry/overseer.txt", text: "lesson hang carrier_a stall the launch\nlesson fault * retry once\n" });
  const qry = `${root}/build/qry/query1-20261011T120000.qry`;
  await ui.run(writer, [qry, root], { wait: false });
  try {
    assert.ok(await invoke("qry_ready", { path: qry }), "the run's writer came ready");
    await ui.click(document.querySelector(".strip-button.strip-runs"));
    await ui.waitFor(() => rowOf("query1-20261011T120000")?.classList.contains("live"), 15000);
    assert.equal(document.querySelector("#explorer .explorer-head h2").textContent, "Runs", "the explorer says it shows the runs");
    await ui.waitFor(() => document.querySelector(".strip-button.strip-runs").classList.contains("live"), 3000);
    assert.match(shownText(), /hang\s*carrier_a\s*stall the launch/, "the folder's lessons beside its runs");
    await invoke("qry_append", { path: qry, name: "questions.txt", text: "one\ntwo\n" });
    await ui.click(rowOf("query1-20261011T120000"));
    await ui.waitFor(() => /questions\.txt/.test(shownText()), 5000);
    await invoke("qry_append", { path: qry, name: "conference_why_it_hung", text: "carrier: it hung past its bound\n" });
    await ui.waitFor(() => /why it hung/.test(shownText()) && /it hung past its bound/.test(shownText()), 5000);
    assert.ok(!document.querySelector("#runs .runs-ruling"), "no ruling before the overseer gives one");
    await invoke("qry_append", { path: qry, name: "conference_why_it_hung", text: "gate: the launch held\noverseer: a stall, the launch held\n" });
    await ui.waitFor(() => document.querySelector("#runs .runs-ruling")?.textContent.includes("a stall, the launch held"), 5000);
    assert.equal(document.querySelectorAll("#runs .runs-conference .runs-account").length, 3, "each account as it was given, then the ruling");
    const handed = performance.now();
    await invoke("qry_append", { path: qry, name: "panic", text: "the part faulted\n", priority: 0 });
    await ui.waitFor(() => document.querySelector("#runs .runs-panic")?.textContent.includes("the part faulted"), 3000);
    assert.ok(performance.now() - handed < 3000, "the panic shows as it lands");
    await ui.waitFor(() => document.querySelector(".strip-button.strip-runs").classList.contains("marked"), 3000);
    await ui.waitFor(() => /query1-20261011T120000 panicked at blob \d+/.test(document.getElementById("status-said")?.textContent ?? ""), 3000);
    await invoke("qry_stop", { path: qry });
    await ui.waitFor(() => rowOf("query1-20261011T120000") && !rowOf("query1-20261011T120000").classList.contains("live"), 10000);
    await ui.waitFor(() => /read by its table/.test(shownText()), 5000);
    assert.match(shownText(), /questions\.txt/, "its blobs, read again from its table");
    assert.ok(document.querySelector("#runs .runs-panic"), "and its panic");
  } finally {
    await invoke("qry_stop", { path: qry }).catch(() => {});
    // The side shows the tree's files again for the tests after this one.
    showGroup("explorer");
  }
});
