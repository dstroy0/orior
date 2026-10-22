// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The fifth form of the calls script plugins draw through: the fourth's, and the language server a
// plugin's plugin.json names in `server`, started with the options the plugin gives and the arguments
// it puts before its plugin.json's, and stopped as the plugin stops, asked and told, and heard:
// `orior.server.on(method, handler)` hears what the server says on its own, the handler's value the
// answer where it asks; a request no handler hears is answered with null. `orior.commands.on(id, handler)` hears a command its plugin.json's `commands`
// put in the command palette pressed. `orior.secrets` keeps a plugin's secrets, a token, in the
// system's keychain, on this machine alone and never with the settings carried to another;
// `orior.browse(address)` opens an address in the browser; `orior.git.ignored(path)` says whether git
// ignores a file of the tree; `orior.tree.files()` lists the tree's files and `orior.tree.relative(path)`
// names a whole path as the tree does, null outside it; `orior.open(path, line, col)` opens a file of the
// tree with the cursor at a place of it; and `close` is heard as the editor closes a file.

import { asked, invoke, listen } from "./bridge.js";
import { serveAgain } from "./servers.js";

// The fifth form's code, from the fourth's: the server, the commands, the secrets and the browser,
// and a request of the server's no handler hears answered with null.
export function preludeFive(fourth) {
  const calls = [
    "    server: Object.freeze({",
    '      start: (options, args) => call("server.start", options ?? null, args ?? []),',
    '      stop: () => call("server.stop"),',
    '      request: (method, params) => call("server.request", method, params ?? null),',
    '      notify: (method, params) => call("server.notify", method, params ?? null),',
    "      on: (method, handler) =>",
    '        (heard["server:" + method] ??= []).push(async ({ params, request }) => {',
    "          const value = await handler(params);",
    '          if (request !== null && request !== undefined) await call("server.answer", request, value ?? null);',
    "        }),",
    "    }),",
    '    commands: Object.freeze({ on: (id, handler) => (heard["command:" + id] ??= []).push(handler) }),',
    '    secrets: Object.freeze({ read: (key) => call("secrets.read", key), write: (key, value) => call("secrets.write", key, value ?? null) }),',
    '    browse: (address) => call("browse", address),',
    '    git: Object.freeze({ ignored: (path) => call("git.ignored", path) }),',
    '    open: (path, line, col) => call("editor.open", path, line ?? 0, col ?? 0),',
    '    tree: Object.freeze({ files: () => call("tree.files"), relative: (path) => call("tree.relative", path) }),',
    "    files: Object.freeze(",
  ].join("\n");
  const unheard = [
    "    } else if (data.event !== undefined) {",
    '      if (!heard[data.event] && data.event.startsWith("server:") && data.detail?.request != null) {',
    '        call("server.answer", data.detail.request, null);',
    "      }",
  ].join("\n");
  return fourth.replace("api: 4,", "api: 5,").replace("    files: Object.freeze(", calls).replace("    } else if (data.event !== undefined) {", unheard);
}

// What the fifth form's calls do, by the call's name, for the plugin `one`.
export const SERVER_CALLS = {
  "server.start": async (one, options, args) => {
    await invoke("plugin_server_start", { id: one.id, options, args: (Array.isArray(args) ? args : []).map(String) });
    one.serving = true;
    // every open tab handed over again, so the server hears each file's changes and closing as the editor makes them
    serveAgain();
  },
  "server.stop": async (one) => {
    one.serving = false;
    return invoke("plugin_server_stop", { id: one.id });
  },
  "server.request": async (one, method, params) => asked("plugin_server_request", { id: one.id, method: String(method), params }, 61000),
  "server.notify": async (one, method, params) => invoke("plugin_server_notify", { id: one.id, method: String(method), params }),
  "server.answer": async (one, request, result) => invoke("plugin_server_answer", { id: one.id, request, result }),
  "secrets.read": async (one, key) => invoke("plugin_secret_read", { id: one.id, key: String(key) }),
  "secrets.write": async (one, key, value) => invoke("plugin_secret_write", { id: one.id, key: String(key), value: value === null ? null : String(value) }),
  "browse": async (one, address) => invoke("link_open", { url: String(address) }),
  "git.ignored": async (one, path) => invoke("file_ignored", { path: String(path) }),
  "tree.files": async () => invoke("tree_files"),
  "tree.relative": async (one, path) => invoke("tree_relative", { path: String(path) }),
};

// The plugin `one`'s server stopped where it started one.
export function serverStopped(one) {
  if (one.serving) {
    one.serving = false;
    invoke("plugin_server_stop", { id: one.id }).catch(() => {});
  }
}

// What a plugin's server says on its own handed to its plugin of those `running`, as `server:` and
// the method, a request of a plugin not running answered with null.
export function hearServers(running) {
  listen("plugin-server", (event) => {
    const { plugin, method, params, request } = event.payload;
    const one = running.get(plugin);
    if (one) {
      one.worker.postMessage({ event: `server:${method}`, detail: { params, request } });
    } else if (request !== null && request !== undefined) {
      invoke("plugin_server_answer", { id: plugin, request, result: null }).catch(() => {});
    }
  });
}

// The commands the plugins `running` put in the command palette, each pressed heard by its plugin.
export function pluginCommands(running) {
  return [...running.values()].flatMap((one) =>
    (one.commands ?? []).map((command) => ({
      key: `${one.name}/${command.id}`,
      menu: one.name,
      label: command.title,
      run: () => one.worker.postMessage({ event: `command:${command.id}`, detail: {} }),
    })),
  );
}
