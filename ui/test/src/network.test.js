// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The network: Off the Network refusing each reach, saying what it was for; Network listing each
// address reached with when and what for; and this machine's own addresses neither held nor listed.

import { invoke } from "../../gui/src-js/bridge.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { offNetwork, setOffNetwork } from "../../gui/src-js/network.js";

const { test, assert, ui } = window.__harness;

// An address that reaches no machine: TEST-NET-1, set aside for examples.
const AWAY = "http://192.0.2.1:9/";

const send = (url) => invoke("requests_send", { path: "network.http", text: `GET ${url}\n`, line: 0, env: null });

test("Off the Network refuses each reach, saying what it was for, and lets this machine's own through", async () => {
  await invoke("file_write", { path: "network.http", text: `GET ${AWAY}\n` });
  await setOffNetwork(true);
  const refused = await send(AWAY).then((answer) => answer.failed ?? "", (error) => String(error));
  assert.match(refused, /orior is held off the network, and a GET request did not reach http:\/\/192\.0\.2\.1:9\//);
  const here = window.__harness.servers?.ORIOR_TEST_HTTP;
  if (here) {
    const answer = await send(`${here}/echo`);
    assert.ok(!answer.failed, "this machine's own address is no part of the network");
  }
  assert.ok(!(await invoke("network_reached")).some((one) => one.address === AWAY), "a reach refused is not one reached");
  await setOffNetwork(false);
});

test("Network lists each address reached, with when and what for, and turns Off the Network on and off", async () => {
  const before = Date.now();
  // The address answers no one; its reach is kept as it starts, before the wait for an answer.
  send(AWAY).catch(() => {});
  await ui.waitFor(async () => (await invoke("network_reached")).some((one) => one.address === AWAY && one.at >= before - 1000), 5000);
  const reached = (await invoke("network_reached")).find((one) => one.address === AWAY);
  assert.equal(reached.for, "a GET request");
  assert.ok(!(await invoke("network_reached")).some((one) => /127\.0\.0\.1|localhost/.test(one.address)), "none of this machine's own");
  runCommand("network");
  const row = () => document.querySelector(`.network-row[data-address="${AWAY}"]`);
  await ui.waitFor(() => row(), 5000);
  assert.equal(row().querySelector(".network-for").textContent, "a GET request");
  const box = document.querySelector(".sheet-network input[type=checkbox]");
  assert.equal(box.checked, false);
  await ui.click(box);
  await ui.waitFor(() => offNetwork() && /held off the network/.test(document.querySelector(".network-said").textContent), 2000);
  document.querySelector("dialog.sheet")?.close();
  runCommand("network-off", ["off"]);
  await ui.waitFor(() => !offNetwork(), 2000);
});
