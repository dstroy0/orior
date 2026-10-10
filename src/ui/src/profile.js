// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Profile panel: a Python run measured as it goes, on this machine or on another one reached by
// ssh, its time and its memory by function drawn as a flame graph that grows as the counts come in.
// Each function is a bar as wide as its share of the run's samples, or of the memory held, its
// callers above it and what it called under it. The pointer on a bar says what it is, how much it
// took and where it is written; a press opens the function there; the panel's menu zooms to a bar
// and back. Run, Profile File profiles the file in the editor here, and Profile File on Another
// Machine profiles it in a folder of another machine, by the same path.

import { invoke, listen } from "./bridge.js";
import { showMenu } from "./menu.js";

// How tall each bar is, and the narrowest a bar is drawn.
const ROW = 18;
const NARROWEST = 0.6;

const state = { hooks: null, counts: null, view: "time", zoom: [], file: null, remote: null, running: false, said: "", hits: [], output: [] };
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The stacks of the view shown, as a tree of frames: each its function's name, its file, the line
// it starts at, how much it and what it called took, and what it called.
function treeOf(stacks) {
  const root = { name: "all", file: "", line: 0, key: "", value: 0, children: new Map() };
  for (const [stack, value] of Object.entries(stacks ?? {})) {
    root.value += value;
    let node = root;
    for (const frame of stack.split("\n")) {
      let child = node.children.get(frame);
      if (!child) {
        const [name, file, line] = frame.split("\t");
        child = { name, file, line: Number(line) || 1, key: frame, value: 0, children: new Map() };
        node.children.set(frame, child);
      }
      child.value += value;
      node = child;
    }
  }
  return root;
}

// The frame the panel is zoomed to, or the whole run.
function zoomed(root) {
  let node = root;
  for (const key of state.zoom) {
    const next = node.children.get(key);
    if (!next) {
      state.zoom = [];
      return root;
    }
    node = next;
  }
  return node;
}

// A color of the theme, as red, green and blue.
function rgbOf(name) {
  const value = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  const hex = value.match(/^#([0-9a-f]{6})$/i)?.[1];
  return hex ? [0, 2, 4].map((at) => parseInt(hex.slice(at, at + 2), 16)) : [200, 120, 60];
}

const mixed = (a, b, share) => a.map((one, at) => Math.round(one * share + b[at] * (1 - share)));

// Each function's color: one of the theme's warm colors for time and cool ones for memory, chosen
// by its name and lightened a little by its file; the same function is the same color in each.
function colorOf(node) {
  const names = state.view === "time" ? ["--t1", "--t3", "--t9", "--t11"] : ["--t4", "--t6", "--t12", "--t14"];
  let hash = 0;
  for (const char of node.name + node.file) {
    hash = (hash * 31 + char.charCodeAt(0)) >>> 0;
  }
  const base = rgbOf(names[hash % names.length]);
  const rgb = mixed(base, rgbOf("--surface"), 0.62 + ((hash >> 8) % 30) / 100);
  return { fill: `rgb(${rgb.join(",")})`, ink: rgb[0] * 0.3 + rgb[1] * 0.59 + rgb[2] * 0.11 > 140 ? "#131331" : "#f4f4fb" };
}

// How much a bar took, as the view counts it.
function amountOf(value) {
  if (state.view === "memory") {
    return value >= 1048576 ? `${(value / 1048576).toFixed(1)} MB` : `${Math.max(1, Math.round(value / 1024))} KB`;
  }
  const counts = state.counts;
  const each = counts?.samples ? (counts.seconds * 1000) / counts.samples : 5;
  return `${Math.round(value * each)} ms, ${value} sample${value === 1 ? "" : "s"}`;
}

function draw() {
  const canvas = parts.graph;
  const box = parts.body.getBoundingClientRect();
  const stacks = state.view === "time" ? state.counts?.time : state.counts?.memory;
  const root = zoomed(treeOf(stacks));
  const depthOf = (node) => 1 + Math.max(0, ...[...node.children.values()].map(depthOf));
  const rows = root.value ? depthOf(root) : 0;
  const width = Math.max(1, box.width - 2);
  const height = Math.max(box.height - 2, rows * ROW);
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  canvas.style.width = `${width}px`;
  canvas.style.height = `${height}px`;
  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.clearRect(0, 0, width, height);
  state.hits = [];
  parts.empty.hidden = Boolean(root.value);
  if (!root.value) {
    return;
  }
  ctx.font = `11.5px ${getComputedStyle(document.documentElement).getPropertyValue("--code") || "monospace"}`;
  ctx.textBaseline = "middle";
  const total = root.value;
  const place = (node, x, w, depth) => {
    if (w < NARROWEST) {
      return;
    }
    const y = depth * ROW;
    const { fill, ink } = colorOf(node);
    ctx.fillStyle = fill;
    ctx.fillRect(x, y, Math.max(0, w - 1), ROW - 1);
    if (w > 28) {
      ctx.fillStyle = ink;
      const label = node === root && !state.zoom.length ? `all: ${amountOf(total)}` : node.name;
      let text = label;
      while (text.length > 1 && ctx.measureText(text).width > w - 8) {
        text = text.slice(0, -2);
      }
      ctx.fillText(text === label ? text : `${text}…`, x + 4, y + ROW / 2);
    }
    state.hits.push({ x, y, w, h: ROW, node, share: node.value / total });
    let at = x;
    for (const child of [...node.children.values()].sort((a, b) => b.value - a.value)) {
      const part = (w * child.value) / node.value;
      place(child, at, part, depth + 1);
      at += part;
    }
  };
  place(root, 0, width, 0);
}

function hitAt(event) {
  const box = parts.graph.getBoundingClientRect();
  const x = event.clientX - box.left;
  const y = event.clientY - box.top;
  return state.hits.find((hit) => x >= hit.x && x < hit.x + hit.w && y >= hit.y && y < hit.y + hit.h) ?? null;
}

// The bar under the pointer, said beside it.
function showTip(event) {
  const hit = hitAt(event);
  if (!hit || !hit.node.file) {
    parts.tip.hidden = true;
    return;
  }
  const { node } = hit;
  parts.tip.textContent = `${node.name} — ${node.file}:${node.line} — ${(hit.share * 100).toFixed(1)}%, ${amountOf(node.value)}`;
  parts.tip.hidden = false;
  const box = parts.body.getBoundingClientRect();
  parts.tip.style.left = `${Math.min(event.clientX - box.left + 12, box.width - parts.tip.offsetWidth - 8)}px`;
  parts.tip.style.top = `${event.clientY - box.top + parts.body.scrollTop + 14}px`;
}

// Opens a bar's function where it is written, where it is a file of the tree.
function openFunction(node) {
  if (node.file && !/^[a-zA-Z]:|^\/|^</.test(node.file)) {
    state.hooks.openAt(node.file, Math.max(0, node.line - 1), 0);
  }
}

function pathTo(target) {
  const root = treeOf(state.view === "time" ? state.counts?.time : state.counts?.memory);
  const find = (node, path) => {
    if (node === target || node.key === target.key) {
      return path;
    }
    for (const child of node.children.values()) {
      const found = find(child, [...path, child.key]);
      if (found) {
        return found;
      }
    }
    return null;
  };
  return find(zoomed(root), []) ?? [];
}

function graphMenu(event) {
  const hit = hitAt(event);
  showMenu(event.clientX, event.clientY, [
    { label: hit?.node.file ? `Open ${hit.node.name}` : "Open", disabled: !hit?.node.file, run: () => openFunction(hit.node) },
    { label: hit?.node.file ? `Zoom to ${hit.node.name}` : "Zoom to", disabled: !hit?.node.file, run: () => ((state.zoom = [...state.zoom, ...pathTo(hit.node)]), draw()) },
    { label: "Zoom Out", disabled: !state.zoom.length, run: () => (state.zoom.pop(), draw()) },
    { label: "Show the Whole Run", disabled: !state.zoom.length, run: () => ((state.zoom = []), draw()) },
  ]);
}

function drawBar() {
  for (const button of parts.bar.querySelectorAll("[data-view]")) {
    button.setAttribute("aria-pressed", String(button.dataset.view === state.view));
  }
  parts.stop.disabled = !state.running;
  parts.again.disabled = state.running || !state.file;
  parts.output.setAttribute("aria-pressed", String(!parts.out.hidden));
  const counts = state.counts;
  const held = counts?.memory ? Object.values(counts.memory).reduce((sum, one) => sum + one, 0) : 0;
  const where = state.remote ? ` on ${state.remote.split(":")[0]}` : "";
  const measured = counts ? `${counts.seconds.toFixed(1)} s, ${counts.samples} samples, ${(held / 1048576).toFixed(1)} MB held` : "";
  parts.said.textContent = state.running ? `Profiling ${state.file}${where}${measured ? `: ${measured}` : ""}` : state.file ? `${state.said} ${state.file}${where}${measured ? `: ${measured}` : ""}` : "Run, Profile File profiles the file in the editor.";
}

function write(text, error) {
  parts.out.append(element("div", { className: error ? "profile-line failed" : "profile-line", textContent: text }));
  while (parts.out.childElementCount > 2000) {
    parts.out.firstElementChild.remove();
  }
  parts.out.scrollTop = parts.out.scrollHeight;
}

export function toggleProfile(open = parts.panel.hidden) {
  parts.panel.hidden = !open;
  if (open) {
    requestAnimationFrame(draw);
  }
}

export const profiling = () => state.running;

// The bars drawn, each its function, its file and line, its share, and where it stands on the graph.
export const profileBars = () => state.hits.map((hit) => ({ name: hit.node.name, file: hit.node.file, line: hit.node.line, share: hit.share, x: hit.x, y: hit.y, w: hit.w }));

// Run, Profile File, and on Another Machine: the Python file in the editor profiled here, or where
// `remote`, user@host:folder, names, in that folder by the same path.
export async function profileFile(remote = null) {
  const file = state.hooks.file();
  if (!file || !/\.pyw?$/.test(file)) {
    state.hooks.say("Open a Python file to profile it.");
    return;
  }
  await state.hooks.save();
  state.file = file;
  state.remote = remote;
  state.counts = null;
  state.zoom = [];
  state.running = true;
  state.said = "";
  parts.out.replaceChildren();
  toggleProfile(true);
  drawBar();
  draw();
  try {
    await invoke("profile_start", { path: file, remote });
  } catch (error) {
    state.running = false;
    state.said = String(error);
    write(String(error), true);
    drawBar();
  }
}

export async function profileRemote(given) {
  const { askFor } = await import("./menubar.js");
  const remote = (given ?? (await askFor("The machine to profile on and the tree's folder there, user@host:folder", state.remote ?? "")))?.trim();
  if (remote) {
    await profileFile(remote);
  }
}

export function stopProfile() {
  invoke("profile_stop").catch(() => {});
}

// `hooks` gives the tree path of the file in the editor, saves it, opens a file at a line and
// column, and says a word on the status bar.
export async function startProfile(hooks) {
  state.hooks = hooks;
  parts.panel = document.getElementById("profile");
  const view = (name, label) => {
    const button = element("button", { className: "profile-tab", type: "button", textContent: label });
    button.dataset.view = name;
    button.addEventListener("click", () => {
      state.view = name;
      state.zoom = [];
      parts.out.hidden = true;
      parts.graph.hidden = false;
      drawBar();
      draw();
    });
    return button;
  };
  const action = (label, run) => {
    const button = element("button", { className: "profile-action", type: "button", textContent: label });
    button.addEventListener("click", run);
    return button;
  };
  parts.said = element("span", { className: "profile-said", ariaLive: "polite" });
  parts.output = element("button", { className: "profile-tab", type: "button", textContent: "Output" });
  parts.output.addEventListener("click", () => {
    parts.out.hidden = !parts.out.hidden;
    parts.graph.hidden = !parts.out.hidden;
    drawBar();
  });
  parts.stop = action("Stop", stopProfile);
  parts.again = action("Profile Again", () => profileFile(state.remote));
  parts.bar = element("div", { className: "profile-bar" }, view("time", "Time"), view("memory", "Memory"), parts.output, parts.said, parts.again, parts.stop);
  parts.graph = element("canvas", { className: "profile-graph" });
  parts.graph.setAttribute("aria-label", "Flame graph");
  parts.out = element("div", { className: "profile-out", hidden: true });
  parts.tip = element("div", { className: "profile-tip", hidden: true });
  parts.empty = element("p", { className: "profile-empty", textContent: "The flame graph grows here as the run's samples come in." });
  parts.body = element("div", { className: "profile-body" }, parts.graph, parts.out, parts.tip, parts.empty);
  parts.panel.append(parts.bar, parts.body);
  parts.graph.addEventListener("mousemove", showTip);
  parts.graph.addEventListener("mouseleave", () => (parts.tip.hidden = true));
  parts.graph.addEventListener("click", (event) => {
    const hit = hitAt(event);
    if (hit) {
      openFunction(hit.node);
    }
  });
  parts.graph.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    event.stopPropagation();
    graphMenu(event);
  });
  new ResizeObserver(() => !parts.panel.hidden && requestAnimationFrame(draw)).observe(parts.body);
  await listen("profile", ({ payload }) => {
    if (payload.kind === "counts") {
      state.counts = payload.counts;
      if (!parts.panel.hidden) {
        draw();
      }
    } else if (payload.kind === "output") {
      write(payload.text, payload.error);
    } else if (payload.kind === "done") {
      state.running = false;
      state.said = payload.code === 0 ? "Profiled" : payload.code === null ? "Stopped profiling" : `Profiled, exit code ${payload.code},`;
    }
    drawBar();
  });
  drawBar();
}
