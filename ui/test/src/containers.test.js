// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Containers window: logs, a command, a shell, a folder, start, stop and restart.

import { containersShown } from "../../gui/src-js/containers.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { asFound, calls, made, press, putBackAsFound } from "../menus.js";

const { test, assert, ui } = window.__harness;

test("the Containers window's menu and controls, from the command line: logs, a command, a shell, a folder, start, stop and restart, Docker and Kubernetes", async () => {
  asFound();
  calls.answer("containers_list", () => ({ items: [{ name: "web", image: "nginx", status: "Up 2 minutes", state: "running" }] }));
  calls.answer("pods_list", () => ({ items: [{ namespace: "shop", name: "api", containers: ["app", "side"], ready: "2/2", phase: "Running" }] }));
  calls.answer("container_logs", ({ name }) => `the logs of ${name}`);
  calls.answer("pod_logs", ({ name, container }) => `the logs of ${name} ${container}`);
  calls.answer("container_run", ({ name, line }) => `${name} ran ${line}`);
  const acted = [];
  calls.answer("container_act", ({ name, act }) => acted.push(`${act} ${name}`));
  const opened = [];
  calls.answer("window_open", ({ remote }) => opened.push(remote));
  await press("containers-kind", "docker");
  await ui.waitFor(() => containersShown().rows.some(([key]) => key === "web"), 5000, "the container listed");
  const read = made("containers_list").length;
  await press("containers-read");
  assert.ok(made("containers_list").length > read, "read again");
  await press("container-logs", "web");
  await ui.waitFor(() => containersShown().out === "the logs of web", 5000, "its logs");
  await press("container-run", "web", "ls", "/");
  await ui.waitFor(() => containersShown().out === "web ran ls /", 5000, "the command's output");
  for (const how of ["start", "stop", "restart"]) {
    await press(`container-${how}`, "web");
  }
  assert.deepEqual(acted, ["start web", "stop web", "restart web"]);
  await press("container-open", "web", "/app");
  assert.deepEqual(opened, ["docker:web:/app"]);
  await press("container-terminal", "web");
  await ui.waitFor(() => /docker exec -it 'web' sh/.test(document.getElementById("term-views").textContent), 10000, "a shell asked for in the container");
  await press("containers-kind", "pods");
  await ui.waitFor(() => containersShown().rows.some(([key]) => key === "shop/api"), 5000, "the pod listed");
  await press("container-logs", "shop/api", "side");
  await ui.waitFor(() => containersShown().out === "the logs of api side", 5000, "a pod's container's logs");
  await press("container-terminal", "shop/api", "side");
  await ui.waitFor(() => /kubectl exec -it -n 'shop' 'api' -c 'side' -- sh/.test(document.getElementById("term-views").textContent), 10000, "a shell asked for in the pod");
  await press("containers-kind", "docker");
  await press("kill").catch(() => {});
  asFound();
  runCommand("containers-view");
  if (!document.getElementById("containers").hidden) {
    runCommand("containers-view");
  }
}, { timeout: 60000 });

test("the window is put back as the file found it", putBackAsFound);
