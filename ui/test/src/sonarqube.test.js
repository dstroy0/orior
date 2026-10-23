// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The SonarQube plugin: a Python file analysed alone, its issue among the file's problems with the
// rule's description and its actions; then a connection made to the SonarQube server of the Docker
// image at 127.0.0.1:9000 through the plugin's own panel, checked, and the tree bound to its project.
// The server's token is read from build/harness/sonarqube_token.txt, which `harness.py sonarqube project`
// writes; without it, or without the server, the connected part says so and passes.

import { invoke } from "../../gui/src-js/bridge.js";
import { openFile } from "../../gui/src-js/edit.js";
import { loadPlugins, setPluginOn } from "../../gui/src-js/plugins.js";
import { allowPlugin, pluginCommandRows, running, stopPlugin } from "../../gui/src-js/scripts.js";
import { knownProblems } from "../../gui/src-js/servers.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

// A file SonarQube finds issues in: a local variable assigned and never read, and a comparison of a
// value with itself, whose issue has a second location.
const UNUSED = "def compute(value):\n    if value == value:\n        return 1\n    x = 5\n    return value\n";

// The HTML of the plugin's panel titled `title`, "" where none is shown.
const panel = (title) => [...document.querySelectorAll(".plugin-panel")].find((one) => one.getAttribute("aria-label") === title)?.querySelector("iframe")?.dataset.html ?? "";

// The frame of the plugin's panel titled `title`.
const frameOf = (title) => [...document.querySelectorAll(".plugin-panel")].find((one) => one.getAttribute("aria-label") === title)?.querySelector("iframe");

// The problems of the file at `path`, as the window holds them.
const problemsOf = (path) => JSON.stringify([...knownProblems().entries()].find(([one]) => one.endsWith(path))?.[1] ?? []);

// Waits until `ready` holds, or fails with what the plugin's output and its last question say.
async function until(ready, ms, what) {
  try {
    await ui.waitFor(ready, ms, what);
  } catch (error) {
    pluginCommandRows().find((one) => one.label === "SonarQube: Show Output")?.run();
    await ui.rest(2000);
    const home = window.__harness.servers.ORIOR_TEST_HOME.replace(/\\/g, "/");
    await invoke("file_write_any", { path: `${home}/../sonarqube_output.html`, text: panel("SonarQube Output") }).catch(() => {});
    const said = panel("SonarQube Output").replace(/<style>[\s\S]*?<\/style>/, "").replace(/\s+/g, " ").slice(0, 3000);
    const panels = [...document.querySelectorAll(".plugin-panel")].map((one) => `${one.getAttribute("aria-label")}: ${(one.querySelector("iframe")?.contentDocument?.body?.innerText ?? "").replace(/\s+/g, " ").slice(0, 300)}`).join(" || ");
    throw new Error(`${error.message}; the panels: ${panels}; the plugin asks ${panel("SonarQube").replace(/<style>[\s\S]*?<\/style>/, "")}; its output: ${said}`);
  }
}

// The command of the plugin's titled `title` run from the command palette.
function commanded(title) {
  const row = pluginCommandRows().find((one) => one.label === title);
  assert.ok(row, `${title} is in the command palette`);
  row.run();
}

// The button of `command` pressed in the panel titled `title`, its form's fields set first.
async function pressed(title, command, fields = {}) {
  const page = () => frameOf(title)?.contentDocument;
  await until(() => page()?.documentElement?.dataset.heard !== undefined && page().querySelector(`[data-command='${command}']`), 60000, `${command} in ${title}`);
  for (const [name, value] of Object.entries(fields)) {
    page().querySelector(`[name='${name}']`).value = value;
  }
  page().querySelector(`[data-command='${command}']`).click();
}

// `promise`, a refusal of it named by the step `what`, since a call refused answers a bare reason.
const step = (what, promise) =>
  promise.catch((error) => {
    throw new Error(`${what}: ${error?.message ?? error}`);
  });

test("SonarQube analyses a file alone, and bound to a project of its server", async () => {
  await step("loading the plugins", loadPlugins());
  await step("allowing the plugin", allowPlugin("sonarqube"));
  await ui.waitFor(() => running().has("sonarqube"), 5000, "the plugin to run");
  try {
    await invoke("file_write", { path: "unused.py", text: UNUSED });
    showView("edit");
    await step("opening the file", openFile("unused.py"));
    await until(() => problemsOf("unused.py").includes("unused local variable"), 120000, "SonarQube's issue among the file's problems");
    assert.ok(problemsOf("unused.py").includes("python:S1481"), `the issue names its rule: ${problemsOf("unused.py")}`);
    const actions = await step("asking for the file's actions", invoke("lsp_actions", { path: "unused.py", from: { line: 3, col: 4 }, to: { line: 3, col: 5 } }));
    assert.ok(actions.some((one) => one.raw?.orior_plugin === "sonarqube"), `the issue's actions are among the file's: ${JSON.stringify(actions.map((one) => one.title))}`);
    const describe = actions.find((one) => one.raw?.orior_plugin === "sonarqube" && /description|details/i.test(one.title));
    assert.ok(describe, "one opens the rule's description");
    await step("acting on the description", invoke("lsp_act", { path: "unused.py", raw: describe.raw }));
    await until(() => panel("SonarQube Rule").includes("S1481"), 20000, "the rule's description");

    commanded("SonarQube: Show Findings");
    await until(() => panel("SonarQube Findings").includes("unused local variable"), 10000, "the issue among the findings");

    const home = window.__harness.servers.ORIOR_TEST_HOME.replace(/\\/g, "/");
    const token = String(await invoke("file_read_any", { path: `${home}/../../../../harness/sonarqube_token.txt` }).catch(() => "")).trim();
    if (!token) {
      assert.ok(true, "no token of the Docker server: the connected part is not run");
      return;
    }
    commanded("SonarQube: Connect to SonarQube Server");
    await pressed("SonarQube Connection", "connection-save", { serverUrl: "http://127.0.0.1:9000", token, connectionId: "docker" });
    await until(() => panel("SonarQube Connections").includes("docker"), 60000, "the connection made and checked");
    assert.ok(!panel("SonarQube Connection").includes('class="fail"') || !frameOf("SonarQube Connection"), panel("SonarQube Connection"));
    await pressed("SonarQube Connections", "connection-check", {});
    await until(() => panel("SonarQube Connections").includes("Connected"), 30000, "the connection checked");
    await pressed("SonarQube Connections", "binding-edit", {});
    await pressed("SonarQube Binding", "binding-key", { key: "orior-sonarqube-test" });
    await until(() => panel("SonarQube Connections").includes("orior-sonarqube-test"), 60000, "the tree bound to the project");
    await invoke("file_write", { path: "unused.py", text: `${UNUSED}\n` });
    await until(() => problemsOf("unused.py").includes("unused local variable"), 120000, "the issue found bound to the project");
    // The duplications the server holds for the project, read through the plugin's own request to it.
    commanded("SonarQube: Show Duplications");
    await until(() => /duplicated lines/.test(panel("SonarQube Duplications")), 60000, "the duplications read from the server");
    assert.ok(!panel("SonarQube Duplications").includes('class="fail"'), panel("SonarQube Duplications").replace(/<style>[\s\S]*?<\/style>/, ""));
  } finally {
    setPluginOn("sonarqube", false);
    stopPlugin("sonarqube");
  }
}, { timeout: 600000 });
