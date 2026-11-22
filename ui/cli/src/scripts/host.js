// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host.js: the host of one script plugin. The window starts it with Node's permission model, and the host reads no
// file but itself. The plugin's code runs in a context of its own. That context holds `orior`, `console` and the
// timers, and no `require`, `process` or `fetch`. Code made from a string is refused there and in the host.
//
// The host and the window speak over the standard input and output, each message framed as the Language Server
// Protocol frames it. The window sends `initialize` with the plugin's id, its code and its stored state. Then it sends
// `orior/ask` for each ask, and `orior/keep` for the state to store before `shutdown`. The host sends `orior/call` for
// each `orior.*` call that the plugin makes. A call that the window refuses answers `{oriorError}` (scripts.c,
// scripts_calls.c)
"use strict";
const vm = require("node:vm");

// the requests that wait on an answer from the window, by id, and the next id
const waiting = new Map();
let next = 1;
// the handlers that the plugin registers: commands by id, events by name, its server's messages by method
const handlers = { command: new Map(), event: new Map(), server: new Map() };
// the function that gives the state to keep, and the state that the last stop stored
let keeper = null;
let stored = null;

// one message written to the window, framed
function written(message) {
  const body = Buffer.from(JSON.stringify(message), "utf8");
  process.stdout.write(`Content-Length: ${body.length}\r\n\r\n`);
  process.stdout.write(body);
}

// one `orior.*` call sent to the window: a promise of its result
function called(name, args) {
  const id = next;
  next += 1;
  return new Promise((resolve, reject) => {
    waiting.set(id, {
      resolve: (result) => (result !== null && typeof result === "object" && "oriorError" in result ? reject(new Error(String(result.oriorError))) : resolve(result)),
      reject,
    });
    written({ jsonrpc: "2.0", id, method: "orior/call", params: { name, args } });
  });
}

// a handler registered under `name` for the asks of the `source` given. A command or a server method has one
// handler, and an event has a list of them
function registered(source, name, fn) {
  if (typeof fn !== "function") {
    return;
  }
  if (source === "event") {
    const list = handlers.event.get(name) ?? [];
    list.push(fn);
    handlers.event.set(name, list);
    return;
  }
  handlers[source].set(String(name), fn);
}

// The families of calls that the window answers. Each call goes to the window under its dotted name
const FAMILIES = {
  server: ["start", "stop", "request", "notify"],
  panels: ["show", "hide"],
  tree: ["files", "relative"],
  files: ["read", "write"],
  secrets: ["read", "write"],
  settings: ["read", "write"],
  marks: ["set"],
  chrome: ["set"],
  editor: ["text", "active"],
  theme: ["colors"],
  git: ["ignored"],
  net: ["request"],
};

// the `orior` that the plugin sees
function oriorMade() {
  const orior = {};
  for (const [family, names] of Object.entries(FAMILIES)) {
    orior[family] = {};
    for (const name of names) {
      orior[family][name] = (...args) => called(`${family}.${name}`, args);
    }
  }
  orior.open = (...args) => called("open", args);
  orior.browse = (...args) => called("browse", args);
  orior.on = (name, fn) => registered("event", name, fn);
  orior.commands = { on: (id, fn) => registered("command", id, fn) };
  orior.server.on = (method, fn) => registered("server", method, fn);
  orior.keep = (fn) => {
    keeper = typeof fn === "function" ? fn : null;
  };
  Object.defineProperty(orior, "state", { get: () => stored, enumerable: true });
  return Object.freeze(orior);
}

// a line that the plugin logs, sent to the window as the protocol's log message
function logged(type, args) {
  const text = args.map((one) => (typeof one === "string" ? one : JSON.stringify(one))).join(" ");
  written({ jsonrpc: "2.0", method: "window/logMessage", params: { type, message: text } });
}

// The plugin's code run as one script in the plugin's context
function loaded(id, code) {
  const context = vm.createContext(
    {
      orior: oriorMade(),
      console: { log: (...a) => logged(4, a), info: (...a) => logged(3, a), warn: (...a) => logged(2, a), error: (...a) => logged(1, a) },
      setTimeout,
      clearTimeout,
      setInterval,
      clearInterval,
      queueMicrotask,
      structuredClone,
      URL,
      TextEncoder,
      TextDecoder,
    },
    { codeGeneration: { strings: false, wasm: false } },
  );
  new vm.Script(code, { filename: `plugin:${id}` }).runInContext(context);
}

// the answer to an ask: the handler of a command or a server method, or each handler of an event in turn
async function asked(params) {
  const source = params?.source;
  const name = String(params?.name ?? "");
  if (source === "event") {
    let result = null;
    for (const fn of handlers.event.get(name) ?? []) {
      result = await fn(params.value);
    }
    return result ?? null;
  }
  const fn = handlers[source]?.get(name);
  if (fn === undefined) {
    throw new Error(`the plugin has no ${source} ${name}`);
  }
  return (await fn(params.value)) ?? null;
}

// the answer to a request of the window's, by its method
async function answered(method, params) {
  if (method === "initialize") {
    const options = params?.initializationOptions ?? {};
    stored = options.state ?? null;
    loaded(String(options.id ?? "plugin"), String(options.code ?? ""));
    return {
      capabilities: {
        orior: {
          commands: [...handlers.command.keys()],
          events: [...handlers.event.keys()],
          server: [...handlers.server.keys()],
        },
      },
    };
  }
  if (method === "orior/ask") {
    return asked(params);
  }
  if (method === "orior/keep") {
    return keeper === null ? null : ((await keeper()) ?? null);
  }
  if (method === "shutdown") {
    return null;
  }
  throw new Error(`the host has no method ${method}`);
}

// one message from the window: an answer to a call of the plugin's, a request, or a notification
async function received(message) {
  if (message.method === undefined) {
    const one = waiting.get(message.id);
    waiting.delete(message.id);
    if (one !== undefined) {
      if (message.error !== undefined) {
        one.reject(new Error(String(message.error.message ?? "the call failed")));
      } else {
        one.resolve(message.result ?? null);
      }
    }
    return;
  }
  if (message.method === "exit") {
    process.exit(0);
  }
  if (message.id === undefined) {
    return;
  }
  try {
    written({ jsonrpc: "2.0", id: message.id, result: await answered(message.method, message.params) });
  } catch (error) {
    written({ jsonrpc: "2.0", id: message.id, error: { code: -32603, message: String(error?.message ?? error) } });
  }
}

// The frames read from the standard input as they come: the header, then the body of its length
let pending = Buffer.alloc(0);
process.stdin.on("data", (piece) => {
  pending = Buffer.concat([pending, piece]);
  for (;;) {
    const end = pending.indexOf("\r\n\r\n");
    if (end < 0) {
      return;
    }
    const length = Number(/Content-Length:\s*(\d+)/i.exec(pending.subarray(0, end).toString("ascii"))?.[1] ?? -1);
    if (length < 0 || pending.length < end + 4 + length) {
      return;
    }
    const body = pending.subarray(end + 4, end + 4 + length).toString("utf8");
    pending = pending.subarray(end + 4 + length);
    let message = null;
    try {
      message = JSON.parse(body);
    } catch {
      continue;
    }
    received(message);
  }
});
process.stdin.on("end", () => process.exit(0));
