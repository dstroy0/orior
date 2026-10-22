// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A plugin of the fifth form of the calls: the language server its plugin.json names started, a file
// it serves handed to it and its diagnostics among the file's problems, what the server asks heard by
// the plugin and answered, what is pressed in its panel heard, its command run from the command
// palette, and a secret kept in the keychain, read back and forgotten.

import { invoke } from "../../gui/src-js/bridge.js";
import { openFile } from "../../gui/src-js/edit.js";
import { loadPlugins, setPluginOn } from "../../gui/src-js/plugins.js";
import { allowPlugin, pluginCommandRows, running, stopPlugin } from "../../gui/src-js/scripts.js";
import { knownProblems } from "../../gui/src-js/servers.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

// A language server in Python that, for each file it is given, says one thing is wrong in it and asks
// the editor a question, and says back the answer it is given.
const STAND_IN = `import json, sys
def read():
    length = 0
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        name, _, value = line.decode().partition(":")
        if name.lower() == "content-length":
            length = int(value)
    return json.loads(sys.stdin.buffer.read(length))
def send(message):
    data = json.dumps(message).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\\r\\n\\r\\n" % len(data) + data)
    sys.stdout.buffer.flush()
while True:
    message = read()
    if message is None:
        break
    method = message.get("method")
    if method == "initialize":
        send({"jsonrpc": "2.0", "id": message["id"], "result": {"capabilities": {}}})
    elif method == "textDocument/didOpen":
        uri = message["params"]["textDocument"]["uri"]
        place = {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 3}}
        found = {"range": place, "severity": 2, "message": "the stand-in finds this"}
        send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics", "params": {"uri": uri, "diagnostics": [found]}})
        send({"jsonrpc": "2.0", "id": 9, "method": "orior/question", "params": {"uri": uri}})
    elif method is None and message.get("id") == 9:
        send({"jsonrpc": "2.0", "method": "orior/answered", "params": {"result": message.get("result")}})
`;

// The plugin: its server started as a file it serves opens, the server's question answered, and each
// thing it hears shown in its panel.
const SERVES = `const said = [];
const show = () => orior.panels.show("heard", { title: "Heard", html: "<p id='heard'>" + said.join(" | ") + "</p><form><input name='word' value='typed'><button data-command='press' data-value='seven'>Press</button></form>" });
orior.on("open", async ({ path }) => {
  if (path.endsWith("served.py")) {
    try {
      await orior.server.start(null);
      said.push("started");
    } catch (error) {
      said.push("failed to start: " + error.message);
    }
    await show();
  }
});
orior.server.on("orior/question", async () => {
  said.push("asked");
  await show();
  return "yes";
});
orior.server.on("orior/answered", async ({ result }) => {
  said.push("answered " + result);
  await show();
});
orior.on("panel", async ({ command, value, fields }) => {
  said.push("pressed " + command + " " + value + " " + fields.word);
  await show();
});
orior.commands.on("hello", async () => {
  try {
    await orior.secrets.write("token", "kept secret");
    said.push("secret " + (await orior.secrets.read("token")));
    await orior.secrets.write("token", null);
    said.push("then " + (await orior.secrets.read("token")));
  } catch (error) {
    said.push("failed " + error.message);
  }
  await show();
});
`;

const heard = () => [...document.querySelectorAll(".plugin-panel-frame")].map((frame) => frame.dataset.html ?? "").find((html) => html.includes("id='heard'")) ?? "";

// Waits until `ready` holds, or fails saying what the panel shows and which files have problems.
async function until(ready, ms, what) {
  try {
    await ui.waitFor(ready, ms, what);
  } catch (error) {
    throw new Error(`${error.message}; the panel shows ${heard() || "nothing"}; the files with problems are ${JSON.stringify([...knownProblems().keys()])}`);
  }
}

test("a plugin's language server, its panel, its command and its secrets", async () => {
  const home = await invoke("home_folder");
  const folder = `${home}/plugins/serves`;
  await invoke("file_write_any", { path: `${folder}/stand_in.py`, text: STAND_IN });
  const manifest = { id: "serves", name: "Serves", version: "1.0.0", kind: "script", api: 5, commands: [{ id: "hello", title: "Serves: Hello" }], server: { toolchain: "python", program: "python", args: [`${folder}/stand_in.py`], languages: ["python"] } };
  await invoke("file_write_any", { path: `${folder}/plugin.json`, text: JSON.stringify(manifest) });
  await invoke("file_write_any", { path: `${folder}/plugin.js`, text: SERVES });
  await loadPlugins();
  await allowPlugin("serves");
  await ui.waitFor(() => running().has("serves"), 5000, "the plugin to run");
  try {
    await invoke("file_write", { path: "served.py", text: "one = 1\n" });
    showView("edit");
    await openFile("served.py");
    await until(() => heard().includes("answered yes"), 20000, "the server's question answered");
    assert.ok(heard().includes("asked"), "the plugin hears what its server asks");
    const problems = [...knownProblems().entries()].find(([path]) => path.endsWith("served.py"))?.[1] ?? [];
    assert.ok(JSON.stringify(problems).includes("the stand-in finds this"), "the server's diagnostics are among the file's problems");
    const frame = [...document.querySelectorAll(".plugin-panel-frame")].find((one) => (one.dataset.html ?? "").includes("id='heard'"));
    await ui.waitFor(() => frame.contentDocument?.querySelector("[data-command='press']"), 5000, "the panel's button");
    frame.contentDocument.querySelector("[data-command='press']").click();
    await ui.waitFor(() => heard().includes("pressed press seven typed"), 5000, "the press heard");
    const row = pluginCommandRows().find((one) => one.label === "Serves: Hello");
    assert.ok(row, "the plugin's command is in the command palette");
    row.run();
    await ui.waitFor(() => heard().includes("then ") || heard().includes("failed "), 10000, "the secret kept and forgotten");
    assert.ok(!heard().includes("failed "), heard());
    assert.ok(heard().includes("secret kept secret"), "a secret is kept and read back");
    assert.ok(heard().includes("then null"), "and is forgotten");
  } finally {
    // the plugin turned off, so no test after this one runs it again as plugins are read
    setPluginOn("serves", false);
    stopPlugin("serves");
  }
});
