// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Comms window, driven as a person drives it: the parts of orior's container and the channels
// they speak on, a terminal joining as a part while its shell runs, and a run that orior starts in
// that terminal told to the window on a channel of its own.

import { invoke } from "../../gui/src-js/bridge.js";
import { commsShown } from "../../gui/src-js/comms.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { killTerminal, newTerminal, runInTerminal, toggleTerminal } from "../../gui/src-js/terminal.js";

const { test, assert, ui } = window.__harness;

const panel = () => document.getElementById("comms");
const named = (name) => commsShown().parts.filter((one) => one.name === name);

// Waits for `what` as ui.waitFor does, and says `awaited` where it never comes.
const waitTo = (awaited, what, ms) => ui.waitFor(what, ms).catch((error) => {
  throw new Error(`${error.message}: ${awaited}; the parts: ${JSON.stringify(commsShown().parts.map((one) => [one.name, one.detail]))}; calls: ${JSON.stringify(window.__harness.calls.made.filter((one) => /term_(open|close)/.test(one.name)).map((one) => [one.name, one.args]))}`);
});

async function openWindow() {
  if (panel().hidden) {
    runCommand("comms-view");
    await ui.waitFor(() => !panel().hidden);
  }
}

test("the Comms window opens and closes from its strip icon, and from View, Comms", async () => {
  assert.equal(panel().hidden, true, "it starts closed");
  await ui.click(".strip-button.strip-comms");
  await ui.waitFor(() => !panel().hidden);
  await ui.click(".strip-button.strip-comms");
  await ui.waitFor(() => panel().hidden);
  runCommand("comms-view");
  assert.equal(panel().hidden, false);
});

test("orior's server and the window are parts of orior's own, each with the channels its key grants", async () => {
  await openWindow();
  await ui.waitFor(() => named("window").length > 0);
  const [server] = named("orior");
  const [shown] = named("window");
  assert.ok(server && /in orior/.test(server.detail) && /speaks on \*/.test(server.detail), JSON.stringify(commsShown().parts));
  assert.ok(/listens on \*/.test(shown.detail) && /speaks on nothing/.test(shown.detail), shown.detail);
});

test("what the server tells the window goes over channels the window counts, each one's last messages shown when pressed", async () => {
  await openWindow();
  // A file written outside the editor: the tree's watch tells the window of it.
  await invoke("file_write", { path: "comms_told.txt", text: String(Date.now()) });
  await ui.waitFor(() => commsShown().channels.some((one) => /window/.test(one.detail)), 10000, "a channel the window hears").catch((error) => {
    throw new Error(error.message + "; the channels: " + JSON.stringify(commsShown().channels));
  });
  const channel = commsShown().channels.find((one) => /window/.test(one.detail));
  assert.ok(/\d+ messages?/.test(channel.status), channel.status);
  await ui.click(`.comms-channel[data-key="${channel.name}"]`);
  await ui.waitFor(() => commsShown().title === `Last messages on ${channel.name}`);
  assert.ok(commsShown().out.startsWith("orior: "), commsShown().out);
});

test("a terminal is a part while its shell runs, and a run orior starts in it is told on a channel of its own", async () => {
  await openWindow();
  const before = named("terminal").length;
  const channelsBefore = new Set(commsShown().channels.map((one) => one.name));
  newTerminal();
  await openWindow();
  await waitTo("the terminal's part", () => named("terminal").length === before + 1, 15000);
  const terminal = named("terminal").at(-1);
  const shell = terminal.detail.match(/process (\d+)/)?.[1];
  assert.ok(/process \d+/.test(terminal.detail) && /listens on runs\/\*; speaks on runs\/\*/.test(terminal.detail), terminal.detail);
  runInTerminal("orior run run-file hello.py");
  await openWindow();
  const ran = () => commsShown().channels.find((one) => one.name.startsWith("runs/") && !channelsBefore.has(one.name));
  // orior's first run in a home asks once whether to file the errors it meets; the test says no.
  const screen = () => document.getElementById("term-views").textContent;
  await ui.waitFor(() => /File them\? \[Y\/n\]/.test(screen()) || ran(), 30000);
  if (!ran()) {
    runInTerminal("n");
  }
  await ui.waitFor(() => /^2 messages/.test(ran()?.status ?? ""), 60000).catch((error) => {
    throw new Error(`${error.message}; the terminal shows: ${screen().trim().slice(-600)}; the channels: ${JSON.stringify(commsShown().channels.map((one) => [one.name, one.status]))}`);
  });
  await ui.click(`.comms-channel[data-key="${ran().name}"]`);
  await ui.waitFor(() => commsShown().title === `Last messages on ${ran().name}`);
  const [started, ended] = commsShown().out.split("\n").map((line) => JSON.parse(line.replace(/^terminal: /, "")));
  assert.equal(started.state, "started");
  assert.equal(started.file, "hello.py");
  assert.equal(ended.state, "ended");
  assert.equal(ended.code, 0, "the run ended well");
  // The panel keeps a shell ready once its last one is closed: the closed shell's part leaves, and
  // the ready shell's joins.
  killTerminal();
  await openWindow();
  await waitTo("the closed shell's part to leave", () => !named("terminal").some((one) => one.detail.startsWith(`process ${shell};`)), 15000);
  toggleTerminal(false);
  runCommand("comms-view");
  await ui.waitFor(() => panel().hidden);
}, { timeout: 120000 });
