// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Plugins that run code of their own, kind "script" (plugins.rs). Each runs in a worker of its own,
// apart from the editor and from every other plugin: one that is slow or stuck holds up neither the
// keys nor the others. Every second each is asked whether it is there; one that has not answered for
// STUCK is named on the status bar with how long, and Stop there ends it. Find the Slow Plugin turns
// half of the plugins on off at a time, asking the reader each time whether the window is still slow,
// until one is left, and turns them all back as they were.
//
// A plugin draws through orior's calls, `orior` in its worker, and never reaches into the editor's
// own parts. It lays marks on the lines of a file, each in the gutter with a word at the line's end;
// lays an overlay of text over the editor at a line; shows panels of its own over the window, each
// its HTML in a frame that runs none of its scripts; sets the label of a file's tab; reads the
// theme's colors; and changes the window's chrome, a badge on the top bar and the color of its
// signal. It hears when a file opens, changes, saves and the cursor moves, and reads and writes
// files, from the second form on keeps settings of its own, and from the third sends requests to
// hosts. A plugin runs only once the reader has let it read, write and reach what its plugin.json
// asks for, and each call is held to that: a file it did not ask to read or write, or a host it did
// not ask to reach, is refused; one that asks for more after an update waits to be let again. A
// plugin names the form of the calls it was written for, `api` in its plugin.json, and orior
// keeps each form it has given: a plugin of an older form gets that form's calls, each answered as
// it was.

import { invoke, onCalls } from "./bridge.js";
import { motionReduced } from "./motion.js";
import { pluginReadings, pluginsOff, setPluginOn } from "./plugins.js";
import { say } from "./statusbar.js";

// How long a plugin may go without answering before it is named stuck, and how often each is asked.
const STUCK = 2000;

// What a plugin's panel is drawn with where less motion is asked for.
const STILL_PANEL = "<style>*,*::before,*::after{animation:none!important;transition:none!important;scroll-behavior:auto!important}</style>";
const ASK = 1000;

// The forms of the calls, each the code that sets `orior` in a plugin's worker before the plugin's own.
const PRELUDES = {
  1: `"use strict";
const orior = (() => {
  let next = 0;
  const waiting = new Map();
  const heard = {};
  const call = (name, ...args) => new Promise((resolve, reject) => {
    next += 1;
    waiting.set(next, { resolve, reject });
    postMessage({ call: name, id: next, args });
  });
  self.onmessage = async ({ data }) => {
    if (data.ping !== undefined) {
      postMessage({ pong: data.ping });
    } else if (data.answer !== undefined) {
      const one = waiting.get(data.answer);
      waiting.delete(data.answer);
      if (data.failed !== undefined) {
        one?.reject(Object.assign(new Error(data.failed.text), data.failed));
      } else {
        one?.resolve(data.value);
      }
    } else if (data.event !== undefined) {
      for (const handler of heard[data.event] ?? []) {
        try {
          await handler(data.detail);
        } catch (error) {
          postMessage({ threw: String(error) });
        }
      }
    }
  };
  return Object.freeze({
    api: 1,
    on: (event, handler) => (heard[event] ??= []).push(handler),
    marks: Object.freeze({ set: (path, marks) => call("marks.set", path, marks) }),
    overlays: Object.freeze({ show: (id, overlay) => call("overlays.show", id, overlay), hide: (id) => call("overlays.hide", id) }),
    panels: Object.freeze({ show: (id, panel) => call("panels.show", id, panel), hide: (id) => call("panels.hide", id) }),
    tabs: Object.freeze({ label: (path, label) => call("tabs.label", path, label) }),
    theme: Object.freeze({ colors: () => call("theme.colors") }),
    chrome: Object.freeze({ set: (chrome) => call("chrome.set", chrome) }),
    editor: Object.freeze({ text: (path) => call("editor.text", path), active: () => call("editor.active") }),
    files: Object.freeze({ read: (path) => call("files.read", path), write: (path, text) => call("files.write", path, text) }),
  });
})();
`,
};

// The second form: the first's calls, and the plugin's own settings, read and written whole, kept
// apart from every other plugin's and carried with the reader's settings to another machine.
PRELUDES[2] = PRELUDES[1].replace("api: 1,", "api: 2,").replace("    files: Object.freeze(", '    settings: Object.freeze({ read: () => call("settings.read"), write: (values) => call("settings.write", values) }),\n    files: Object.freeze(');

// The third form: the second's calls, and requests to the hosts the plugin asked to reach.
PRELUDES[3] = PRELUDES[2].replace("api: 2,", "api: 3,").replace("    files: Object.freeze(", '    net: Object.freeze({ request: (request) => call("net.request", request) }),\n    files: Object.freeze(');

// The fourth form: the third's calls, and a part of a file's lines, from a line to before another, the
// one way a file held as pages is read (paged.js).
PRELUDES[4] = PRELUDES[3].replace("api: 3,", "api: 4,").replace('active: () => call("editor.active") }),', 'active: () => call("editor.active"), lines: (path, from, to) => call("editor.lines", path, from, to) }),');

// What each plugin was let read, write and reach, by its id, kept as the setting plugins.allowed.
const ALLOWED = "orior.plugins.allowed";

function allowedAll() {
  try {
    const kept = JSON.parse(localStorage.getItem(ALLOWED) ?? "{}");
    return kept && typeof kept === "object" ? kept : {};
  } catch {
    return {};
  }
}

// What the plugin `reading` asks for that it has not been let: each of reads, writes and reaches.
export function askedMore(reading) {
  const asks = reading.asks ?? { reads: [], writes: [], reaches: [] };
  const granted = allowedAll()[reading.id] ?? {};
  const more = Object.fromEntries(["reads", "writes", "reaches"].map((kind) => [kind, asks[kind].filter((one) => !(granted[kind] ?? []).includes(one))]));
  return Object.values(more).some((list) => list.length) ? more : null;
}

// Lets the plugin `id` read, write and reach what it asks for, and runs it where it is on.
export function allowPlugin(id) {
  const reading = pluginReadings().find((one) => one.id === id);
  if (!reading) {
    return;
  }
  localStorage.setItem(ALLOWED, JSON.stringify({ ...allowedAll(), [id]: reading.asks ?? { reads: [], writes: [], reaches: [] } }));
  return syncPlugins();
}

// What a plugin asks for, said in a line: "reads *.md; writes notes/**; reaches api.example.com".
export function asksText(asks) {
  const part = (word, list) => (list?.length ? `${word} ${list.join(", ")}` : "");
  return [part("reads", asks?.reads), part("writes", asks?.writes), part("reaches", asks?.reaches)].filter(Boolean).join("; ") || "asks for nothing";
}

const globs = new Map();

// Whether the tree's path `path` matches the pattern `pattern`: `**` any folders, `*` any letters but
// `/`, and `?` one.
function matches(pattern, path) {
  let found = globs.get(pattern);
  if (!found) {
    let source = "";
    for (let at = 0; at < pattern.length; at += 1) {
      const letter = pattern[at];
      if (letter === "*" && pattern[at + 1] === "*") {
        source += pattern[at + 2] === "/" ? "(?:.*/)?" : ".*";
        at += pattern[at + 2] === "/" ? 2 : 1;
      } else if (letter === "*") {
        source += "[^/]*";
      } else if (letter === "?") {
        source += "[^/]";
      } else {
        source += letter.replace(/[\\^$.|+()[\]{}]/g, "\\$&");
      }
    }
    found = new RegExp(`^${source}$`);
    globs.set(pattern, found);
  }
  return found.test(path.replace(/\\/g, "/"));
}

// Refuses a plugin's call on `path` it did not ask for, as `kind`, reads or writes.
function hold(one, kind, path) {
  const patterns = one.asks?.[kind] ?? [];
  if (!patterns.some((pattern) => matches(pattern, path))) {
    throw new Error(`${one.name} did not ask to ${kind === "reads" ? "read" : "write"} ${path}`);
  }
}

// Whether `host` is one of `hosts`, or under one named with `*.` before it.
const reachable = (hosts, host) => hosts.some((one) => one === host || (one.startsWith("*.") && host.endsWith(one.slice(1))));

// The theme's colors a plugin reads, by the names of the window's own.
const COLORS = ["--bg", "--surface", "--well", "--fg", "--fg-2", "--fg-3", "--line", "--signal", "--link", "--fail", "--ed-bg", "--ed-fg", "--ed-added", "--ed-changed", "--ed-removed"];

const state = { running: new Map(), hooks: null, timer: 0, finding: null, node: null, told: new Set() };

// The marks the running plugins lay on the file at `path`: each line's, from every plugin, as
// [{ text, kind, title, plugin }].
export function marksOf(path) {
  const out = new Map();
  for (const one of state.running.values()) {
    for (const mark of one.marks.get(path) ?? []) {
      const line = Number(mark.line);
      if (!out.has(line)) {
        out.set(line, []);
      }
      out.get(line).push({ text: String(mark.text ?? ""), kind: ["info", "warn", "error", "note"].includes(mark.kind) ? mark.kind : "info", title: `${one.name}${mark.title ? `: ${mark.title}` : ""}` });
    }
  }
  return out.size ? out : null;
}

// The label the running plugins set on the tab of the file at `path`, or "".
export function tabLabelOf(path) {
  return [...state.running.values()].map((one) => one.labels.get(path)).filter(Boolean).join(" ");
}

// The plugins running, for the tests: each by its id, with what it drew and when it last answered.
export const running = () => state.running;

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function repaint() {
  state.hooks?.repaint();
}

function drawPanels() {
  const host = document.getElementById("plugin-panels");
  const shown = [...state.running.values()].flatMap((one) => [...one.panels].map(([id, panel]) => ({ one, id, panel })));
  host.hidden = !shown.length;
  const kept = new Map([...host.children].map((node) => [node.dataset.key, node]));
  host.replaceChildren(
    ...shown.map(({ one, id, panel }) => {
      const key = `${one.id}/${id}`;
      const made = kept.get(key) ?? element("section", { className: "plugin-panel", ariaLabel: String(panel.title ?? id) });
      made.dataset.key = key;
      const close = element("button", { type: "button", className: "plugin-panel-close", textContent: "×", title: "Close" });
      close.addEventListener("click", () => {
        one.panels.delete(id);
        drawPanels();
      });
      let frame = made.querySelector("iframe");
      if (!frame) {
        frame = element("iframe", { className: "plugin-panel-frame", title: String(panel.title ?? id) });
        frame.setAttribute("sandbox", "");
      }
      // A panel draws still where less motion is asked for, its animations and transitions ended.
      const html = String(panel.html ?? "") + (motionReduced() ? STILL_PANEL : "");
      if (frame.dataset.html !== html) {
        frame.dataset.html = html;
        frame.srcdoc = html;
      }
      made.replaceChildren(element("header", {}, element("span", { textContent: `${panel.title ?? id}`, title: one.name }), close), frame);
      return made;
    }),
  );
}

function drawOverlays() {
  const host = document.getElementById("editor");
  host.querySelectorAll(".plugin-overlay").forEach((node) => node.remove());
  const active = state.hooks?.active();
  const editor = state.hooks?.editor();
  if (!active || !editor?.s) {
    return;
  }
  for (const one of state.running.values()) {
    for (const [id, overlay] of one.overlays) {
      if (overlay.path !== active) {
        continue;
      }
      const line = Math.max(0, Math.min(Number(overlay.line) - editor.s.base, editor.s.doc.count - 1));
      const box = host.getBoundingClientRect();
      const top = editor.rectOf({ line, col: 0 }).bottom - box.top;
      const made = element("div", { className: "plugin-overlay", textContent: String(overlay.text ?? ""), title: one.name });
      made.dataset.key = `${one.id}/${id}`;
      made.style.top = `${top}px`;
      made.style.left = `${editor.gutter.offsetWidth + 8}px`;
      host.append(made);
    }
  }
}

function drawChrome() {
  const badge = document.getElementById("plugin-badge");
  const chromes = [...state.running.values()].map((one) => one.chrome).filter(Boolean);
  const text = chromes.map((one) => one.badge).filter(Boolean).join(" ");
  badge.hidden = !text;
  badge.textContent = text;
  const accent = chromes.map((one) => one.accent).filter(Boolean).at(-1);
  if (accent && CSS.supports("color", accent)) {
    document.documentElement.style.setProperty("--signal", accent);
  } else {
    document.documentElement.style.removeProperty("--signal");
  }
}

// What a plugin's call does, by the call's name.
const CALLS = {
  "marks.set": (one, path, marks) => {
    one.marks.set(String(path), Array.isArray(marks) ? marks.slice(0, 10000) : []);
    repaint();
  },
  "overlays.show": (one, id, overlay) => {
    one.overlays.set(String(id), { path: String(overlay?.path ?? ""), line: Number(overlay?.line ?? 0), text: String(overlay?.text ?? "") });
    drawOverlays();
  },
  "overlays.hide": (one, id) => {
    one.overlays.delete(String(id));
    drawOverlays();
  },
  "panels.show": (one, id, panel) => {
    one.panels.set(String(id), { title: String(panel?.title ?? id), html: String(panel?.html ?? "") });
    drawPanels();
  },
  "panels.hide": (one, id) => {
    one.panels.delete(String(id));
    drawPanels();
  },
  "tabs.label": (one, path, label) => {
    if (label) {
      one.labels.set(String(path), String(label).slice(0, 40));
    } else {
      one.labels.delete(String(path));
    }
    state.hooks?.tabsChanged();
  },
  "theme.colors": () => {
    const style = getComputedStyle(document.documentElement);
    return Object.fromEntries(COLORS.map((name) => [name.slice(2), style.getPropertyValue(name).trim()]));
  },
  "chrome.set": (one, chrome) => {
    one.chrome = { badge: String(chrome?.badge ?? "").slice(0, 40), accent: String(chrome?.accent ?? "") };
    drawChrome();
  },
  "editor.text": async (one, path) => {
    hold(one, "reads", String(path));
    return state.hooks.textOf(String(path));
  },
  "editor.lines": async (one, path, from, to) => {
    hold(one, "reads", String(path));
    return state.hooks.linesOf(String(path), Math.max(0, Number(from) || 0), Math.max(0, Number(to) || 0));
  },
  "editor.active": () => state.hooks?.active() ?? null,
  "files.read": async (one, path) => {
    hold(one, "reads", String(path));
    return (await invoke("file_read", { path: String(path) })).text;
  },
  "files.write": async (one, path, text) => {
    hold(one, "writes", String(path));
    return state.hooks.write(one, String(path), String(text));
  },
  "net.request": async (one, request) => {
    const url = String(request?.url ?? "");
    let host = "";
    try {
      host = new URL(url).hostname;
    } catch {
      throw new Error(`${url} is no address`);
    }
    if (!reachable(one.asks?.reaches ?? [], host)) {
      throw new Error(`${one.name} did not ask to reach ${host}`);
    }
    const headers = Object.entries(request?.headers ?? {}).map(([name, value]) => [String(name), String(value)]);
    return invoke("plugin_request", { id: one.id, method: String(request?.method ?? "GET"), url, headers, body: String(request?.body ?? "") });
  },
  "settings.read": async (one) => invoke("plugin_settings_read", { id: one.id }),
  "settings.write": async (one, values) => invoke("plugin_settings_write", { id: one.id, values }),
};

async function answer(one, data) {
  if (data.pong !== undefined) {
    one.answered = performance.now();
    return;
  }
  if (data.threw !== undefined) {
    say(`${one.name}: ${data.threw}`, { failed: true });
    return;
  }
  if (data.call === undefined) {
    return;
  }
  const run = CALLS[data.call];
  try {
    if (!run) {
      throw new Error(`orior has no call ${data.call}`);
    }
    const value = await run(one, ...(data.args ?? []));
    one.worker?.postMessage({ answer: data.id, value: value ?? null });
  } catch (error) {
    one.worker?.postMessage({ answer: data.id, failed: { text: String(error?.message ?? error), refused: Boolean(error?.refused) } });
  }
}

// Ends the plugin `id`, and takes away what it drew.
export function stopPlugin(id) {
  const one = state.running.get(id);
  if (!one) {
    return;
  }
  one.worker.terminate();
  state.running.delete(id);
  drawPanels();
  drawOverlays();
  drawChrome();
  state.hooks?.tabsChanged();
  repaint();
  drawStuck();
}

async function startPlugin(reading) {
  const id = reading.id;
  let code;
  try {
    code = await invoke("plugin_code", { id });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const api = Number(reading.api ?? 1);
  const prelude = PRELUDES[api] ?? PRELUDES[1];
  const url = URL.createObjectURL(new Blob([prelude, "\n", code], { type: "text/javascript" }));
  const worker = new Worker(url, { name: id });
  URL.revokeObjectURL(url);
  const one = { id, name: reading.name ?? id, api, asks: reading.asks, worker, answered: performance.now(), asked: 0, marks: new Map(), overlays: new Map(), panels: new Map(), labels: new Map(), chrome: null };
  worker.addEventListener("message", ({ data }) => answer(one, data));
  worker.addEventListener("error", (event) => say(`${one.name}: ${event.message}`, { failed: true }));
  state.running.set(id, one);
}

// Tells every plugin running of the event `event`, with `detail`.
export function tell(event, detail) {
  for (const one of state.running.values()) {
    one.worker.postMessage({ event, detail });
  }
  if (event === "open") {
    drawOverlays();
  }
}

function drawStuck() {
  const node = state.node;
  if (!node) {
    return;
  }
  const now = performance.now();
  const stuck = [...state.running.values()].filter((one) => now - one.answered > STUCK);
  node.hidden = !stuck.length;
  if (!stuck.length) {
    node.replaceChildren();
    return;
  }
  const one = stuck.reduce((a, b) => (a.answered < b.answered ? a : b));
  const stop = element("button", { type: "button", className: "status-plugin-stop", textContent: "Stop" });
  stop.addEventListener("click", () => stopPlugin(one.id));
  node.replaceChildren(`${one.name} has not answered for ${Math.round((now - one.answered) / 1000)} s`, stop);
}

function ask() {
  const now = performance.now();
  for (const one of state.running.values()) {
    one.worker.postMessage({ ping: now });
  }
  drawStuck();
}

// Starts each script plugin that is on and not running, and ends each running that is off or gone.
// Starts each script plugin on and let run, and stops each no longer, once the frame being drawn has
// been drawn: no plugin is waited on before the window takes keys, and each starts on its own, none
// waiting on another's code to be read.
export async function syncPlugins() {
  await new Promise((drawn) => requestAnimationFrame(() => setTimeout(drawn, 0)));
  const off = pluginsOff();
  const scripts = pluginReadings().filter((reading) => reading.kind === "script" && !reading.error && !reading.replaced);
  for (const id of [...state.running.keys()]) {
    if (off.has(id) || !scripts.some((reading) => reading.id === id)) {
      stopPlugin(id);
    }
  }
  const starting = [];
  for (const reading of scripts) {
    const more = askedMore(reading);
    if (more && state.running.has(reading.id)) {
      stopPlugin(reading.id);
    }
    if (more && !off.has(reading.id)) {
      if (!state.told.has(`${reading.id} ${reading.version}`)) {
        state.told.add(`${reading.id} ${reading.version}`);
        say(`${reading.name} waits for you to let it run: it ${asksText(more)}. File, Plugins lets it.`);
      }
      continue;
    }
    if (!off.has(reading.id) && !state.running.has(reading.id)) {
      starting.push(startPlugin(reading));
    }
  }
  await Promise.all(starting);
}

// Find the Slow Plugin: the plugins on halved, half of them off at a time, the reader asked each time
// whether the window is still slow, until one is left; then each set back as it was.
export function findSlowPlugin() {
  const off = pluginsOff();
  const on = pluginReadings().map((reading) => reading.id).filter((id) => !off.has(id));
  if (!on.length) {
    say("No plugin is on");
    return;
  }
  state.finding = { suspects: on, was: off, step: 0 };
  stepFinding();
}

function endFinding(named) {
  const finding = state.finding;
  state.finding = null;
  document.getElementById("slow-finder").hidden = true;
  for (const reading of pluginReadings()) {
    setPluginOn(reading.id, !finding.was.has(reading.id));
  }
  syncPlugins();
  if (named) {
    say(`${named} is the plugin that slows the window: File, Plugins turns it off`);
  }
}

function stepFinding() {
  const finding = state.finding;
  const node = document.getElementById("slow-finder");
  if (finding.suspects.length === 1) {
    const [found] = finding.suspects;
    const name = pluginReadings().find((reading) => reading.id === found)?.name ?? found;
    endFinding(name);
    return;
  }
  finding.step += 1;
  finding.half = finding.suspects.slice(0, Math.ceil(finding.suspects.length / 2));
  for (const reading of pluginReadings()) {
    setPluginOn(reading.id, !finding.was.has(reading.id) && !finding.half.includes(reading.id));
  }
  syncPlugins();
  const button = (text, run) => {
    const made = element("button", { type: "button", className: "slow-finder-button", textContent: text });
    made.addEventListener("click", run);
    return made;
  };
  node.hidden = false;
  node.replaceChildren(
    element("p", { className: "slow-finder-said", textContent: `Step ${finding.step}: ${finding.half.length} of ${finding.suspects.length} plugins are off. Is the window still slow?` }),
    element(
      "div",
      { className: "slow-finder-buttons" },
      button("Still Slow", () => {
        finding.suspects = finding.suspects.slice(finding.half.length);
        stepFinding();
      }),
      button("No Longer Slow", () => {
        finding.suspects = finding.half;
        stepFinding();
      }),
      button("Stop", () => endFinding(null)),
    ),
  );
}

// What the finding asks of the reader now, for the tests.
export const finding = () => state.finding;

export function startScripts(hooks) {
  state.hooks = hooks;
  state.node = document.getElementById("status-plugin");
  window.clearInterval(state.timer);
  state.timer = window.setInterval(ask, ASK);
  hooks.editor()?.scroller.addEventListener("scroll", () => state.running.size && drawOverlays(), { passive: true });
  onCalls((command, args, promise) => {
    if (command === "file_write" || command === "file_write_raised") {
      promise.then(() => tell("save", { path: args.path }), () => {});
    }
  });
}
