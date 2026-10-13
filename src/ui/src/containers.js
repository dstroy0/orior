// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Containers tool window: Docker's containers on the tree's machine, each under its Compose
// project where it belongs to one, and a Kubernetes cluster's pods under their namespaces, read again
// every REFRESH while the window shows. A press on a row reads its logs into the pane beside the list.
// A container's menu starts, stops or restarts it, runs a command in it, opens a terminal in it, or
// opens a folder of it in a window of its own, where the editor, the jobs and the debugger work in it;
// a pod's menu reads each of its containers' logs or opens a terminal in one.

import { invoke } from "./bridge.js";
import { menuOn } from "./menu.js";

// How often the list is read again while the window shows, in milliseconds.
const REFRESH = 3000;

const state = { hooks: null, timer: 0, kind: "docker", shown: null };
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function toggleContainers(open = parts.panel.hidden) {
  parts.panel.hidden = !open;
  if (open) {
    refresh();
  } else {
    window.clearTimeout(state.timer);
  }
}

function schedule() {
  window.clearTimeout(state.timer);
  if (!parts.panel.hidden) {
    state.timer = window.setTimeout(refresh, REFRESH);
  }
}

async function refresh() {
  const kind = state.kind;
  const listed = await invoke(kind === "docker" ? "containers_list" : "pods_list").catch((error) => ({ items: [], problem: String(error) }));
  if (kind === state.kind) {
    draw(listed);
  }
  schedule();
}

// Puts `text` in the pane beside the list under `title`.
function show(title, text) {
  parts.title.textContent = title;
  parts.out.textContent = text;
  parts.out.scrollTop = parts.out.scrollHeight;
}

function group(label, count) {
  return element("div", { className: "containers-group" }, element("span", { textContent: label }), element("span", { className: "count", textContent: String(count) }));
}

function row(key, name, detail, status, mark) {
  const button = element("button", { className: `containers-row ${mark}`, type: "button", title: status }, element("span", { className: "containers-dot" }), element("span", { className: "name", textContent: name }), element("span", { className: "detail", textContent: detail }), element("span", { className: "status", textContent: status }));
  button.dataset.key = key;
  if (state.shown === key) {
    button.setAttribute("aria-current", "true");
  }
  button.addEventListener("click", () => logsOf(key));
  return button;
}

let items = [];

function draw(listed) {
  items = listed.items ?? [];
  const nodes = [];
  if (listed.problem) {
    nodes.push(element("p", { className: "containers-problem", textContent: listed.problem }));
  } else if (!items.length) {
    nodes.push(element("p", { className: "containers-problem", textContent: state.kind === "docker" ? "Docker has no containers." : "The cluster has no pods." }));
  } else if (state.kind === "docker") {
    const projects = [...new Set(items.map((one) => one.project ?? ""))];
    for (const project of projects) {
      const own = items.filter((one) => (one.project ?? "") === project);
      if (project) {
        nodes.push(group(project, own.length));
      }
      own.forEach((one) => nodes.push(row(one.name, one.name, one.service ? `${one.service} · ${one.image}` : one.image, one.status, one.state)));
    }
  } else {
    const spaces = [...new Set(items.map((one) => one.namespace))];
    for (const space of spaces) {
      const own = items.filter((one) => one.namespace === space);
      nodes.push(group(space, own.length));
      own.forEach((one) => nodes.push(row(`${one.namespace}/${one.name}`, one.name, `${one.ready} ready`, one.phase, one.phase === "Running" ? "running" : one.phase === "Failed" ? "dead" : "exited")));
    }
  }
  parts.list.replaceChildren(...nodes);
  parts.said.textContent = listed.problem ? "" : `${items.length} ${state.kind === "docker" ? "container" : "pod"}${items.length === 1 ? "" : "s"}`;
}

const podOf = (key) => items.find((one) => `${one.namespace}/${one.name}` === key);
const containerOf = (key) => items.find((one) => one.name === key);

async function logsOf(key, container = null) {
  state.shown = key;
  parts.list.querySelectorAll(".containers-row").forEach((one) => one.toggleAttribute("aria-current", one.dataset.key === key));
  show(`Logs of ${key}${container ? ` (${container})` : ""}`, "");
  const pod = state.kind === "pods" ? podOf(key) : null;
  const read = pod ? invoke("pod_logs", { namespace: pod.namespace, name: pod.name, container: container ?? pod.containers[0] ?? null }) : invoke("container_logs", { name: key });
  show(`Logs of ${key}${container ? ` (${container})` : ""}`, await read.catch((error) => String(error)));
}

async function act(name, how) {
  show(`${name}: ${how}`, "");
  await invoke("container_act", { name, act: how }).catch((error) => show(`${name}: ${how}`, String(error)));
  refresh();
}

async function runIn(name, given = null) {
  const line = (given ?? (await state.hooks.ask(`A command to run in ${name}`, "")))?.trim();
  if (!line) {
    return;
  }
  show(`${name}: ${line}`, "");
  show(`${name}: ${line}`, await invoke("container_run", { name, line }).catch((error) => String(error)));
}

async function openIn(name, given = null) {
  const folder = (given ?? (await state.hooks.ask(`The folder of ${name} to open`, "/")))?.trim();
  if (folder) {
    await invoke("window_open", { remote: `docker:${name}:${folder}`, words: [] }).catch((error) => state.hooks.say(String(error)));
  }
}

const quoted = (text) => `'${text.replace(/'/g, "'\\''")}'`;

// The window shows `kind`, Docker's containers or Kubernetes' pods, and reads them again.
async function showKind(kind) {
  toggleContainers(true);
  if (state.kind !== kind) {
    state.kind = kind;
    state.shown = null;
    parts.bar.querySelectorAll(".profile-tab").forEach((one, at) => one.setAttribute("aria-pressed", String((at === 0 ? "docker" : "pods") === kind)));
    parts.list.replaceChildren();
    show("", "");
  }
  await refresh();
}

// The line that opens a shell in `key`, a container or a namespace's pod, in `container` of a pod.
function shellLine(key, container = null) {
  const pod = podOf(key);
  if (pod) {
    const inside = container ?? (pod.containers.length > 1 ? pod.containers[0] : null);
    return `kubectl exec -it -n ${quoted(pod.namespace)} ${quoted(pod.name)}${inside ? ` -c ${quoted(inside)}` : ""} -- sh`;
  }
  return `docker exec -it ${quoted(key)} sh`;
}

// The window's menu and its controls as commands, by the names commands.json gives them, each run
// from the window or from the command line with a container's name, or a pod as namespace/name.
const kindOf = (key) => (key.includes("/") ? "pods" : "docker");

export const containerCommands = {
  "containers-kind": ([kind]) => showKind(kind === "pods" ? "pods" : "docker"),
  "containers-read": () => refresh(),
  "container-logs": async ([key, container]) => (await showKind(kindOf(key)), logsOf(key, container ?? null)),
  "container-run": async ([name, ...line]) => (await showKind("docker"), runIn(name, line.length ? line.join(" ") : null)),
  "container-terminal": async ([key, container]) => (await showKind(kindOf(key)), state.hooks.terminal(shellLine(key, container ?? null))),
  "container-open": ([name, folder]) => openIn(name, folder ?? null),
  "container-start": ([name]) => act(name, "start"),
  "container-stop": ([name]) => act(name, "stop"),
  "container-restart": ([name]) => act(name, "restart"),
};

function rowItems(event) {
  const key = event.target.closest(".containers-row")?.dataset.key;
  if (!key) {
    return null;
  }
  if (state.kind === "pods") {
    const pod = podOf(key);
    if (!pod) {
      return null;
    }
    return [
      ...pod.containers.map((one) => ({ label: pod.containers.length > 1 ? `Logs of ${one}` : "Logs", command: "container-logs", args: [key, one], run: () => logsOf(key, one) })),
      "-",
      { label: "Open a Terminal", command: "container-terminal", args: [key], run: () => state.hooks.terminal(shellLine(key)) },
    ];
  }
  const one = containerOf(key);
  if (!one) {
    return null;
  }
  const running = one.state === "running";
  return [
    { label: "Logs", command: "container-logs", args: [key], run: () => logsOf(key) },
    { label: "Run a Command…", command: "container-run", args: [key], disabled: !running, run: () => runIn(key) },
    { label: "Open a Terminal", command: "container-terminal", args: [key], disabled: !running, run: () => state.hooks.terminal(shellLine(key)) },
    { label: "Open a Folder in a Window…", command: "container-open", args: [key], disabled: !running, run: () => openIn(key) },
    "-",
    { label: "Start", command: "container-start", args: [key], disabled: running, run: () => act(key, "start") },
    { label: "Stop", command: "container-stop", args: [key], disabled: !running, run: () => act(key, "stop") },
    { label: "Restart", command: "container-restart", args: [key], disabled: !running, run: () => act(key, "restart") },
  ];
}

// What the window shows, for the tests: each row's name and state, and the pane's title.
export function containersShown() {
  return { kind: state.kind, rows: [...parts.list.querySelectorAll(".containers-row")].map((one) => [one.dataset.key, one.className.replace("containers-row", "").trim()]), title: parts.title.textContent, out: parts.out.textContent };
}

// `hooks` asks for a line, opens a terminal running a line, and says a word on the status bar.
export function startContainers(hooks) {
  state.hooks = hooks;
  parts.panel = document.getElementById("containers");
  const tab = (kind, label) => {
    const button = element("button", { className: "profile-tab", type: "button", textContent: label });
    button.setAttribute("aria-pressed", String(state.kind === kind));
    button.addEventListener("click", () => {
      state.kind = kind;
      state.shown = null;
      parts.bar.querySelectorAll(".profile-tab").forEach((one) => one.setAttribute("aria-pressed", String(one === button)));
      parts.list.replaceChildren();
      show("", "");
      refresh();
    });
    return button;
  };
  const again = element("button", { className: "profile-action", type: "button", textContent: "Read Again" });
  again.addEventListener("click", refresh);
  parts.said = element("span", { className: "profile-said" });
  parts.bar = element("div", { className: "profile-bar" }, tab("docker", "Docker"), tab("pods", "Kubernetes"), parts.said, again);
  parts.list = element("div", { className: "containers-list" });
  parts.title = element("div", { className: "containers-title" });
  parts.out = element("pre", { className: "containers-out" });
  parts.panel.append(parts.bar, element("div", { className: "containers-body" }, parts.list, element("div", { className: "containers-side" }, parts.title, parts.out)));
  menuOn(parts.list, rowItems);
}
