// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Comms tool window: every part of orior's container that speaks or listens through the comms
// layer, with the channels its key grants and how much it has said, and every channel that has
// carried or refused a message, with its traffic, the bytes of it that went through the transfer
// layer, and the parts that hear it, read again every REFRESH while the window shows. A press on a
// channel shows its last messages in the pane beside the list.

import { invoke } from "./bridge.js";

// How often the lists are read again while the window shows, in milliseconds.
const REFRESH = 1000;

const state = { timer: 0, shown: null, channels: [] };
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function toggleComms(open = parts.panel.hidden) {
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
  try {
    const [listed, channels] = await Promise.all([invoke("comms_parts"), invoke("comms_channels")]);
    draw(listed, channels);
  } catch (error) {
    parts.list.replaceChildren(element("p", { className: "containers-problem", textContent: String(error) }));
  }
  schedule();
}

function group(label, count) {
  return element("div", { className: "containers-group" }, element("span", { textContent: label }), element("span", { className: "count", textContent: String(count) }));
}

const granted = (grants) => (grants.length ? grants.join(", ") : "nothing");

const plural = (count, word) => `${count} ${word}${count === 1 ? "" : "s"}`;

function partRow(part) {
  const status = part.refused ? `${part.said} said, ${part.refused} refused` : `${part.said} said`;
  const detail = `${part.process ? `process ${part.process}` : "in orior"}; listens on ${granted(part.listen)}; speaks on ${granted(part.speak)}`;
  const row = element("div", { className: `containers-row comms-part ${part.refused ? "dead" : "running"}`, title: detail }, element("span", { className: "containers-dot" }), element("span", { className: "name", textContent: part.name }), element("span", { className: "status", textContent: status }), element("span", { className: "detail", textContent: detail }));
  row.dataset.key = part.name;
  return row;
}

function channelRow(channel) {
  const status = channel.refused ? `${plural(channel.messages, "message")}, ${channel.refused} refused` : plural(channel.messages, "message");
  const moved = channel.transferred ? `, ${plural(channel.transferred, "byte")} of them through the transfer layer` : "";
  const detail = `${plural(channel.bytes, "byte")}${moved}; heard by ${channel.listeners.length ? [...new Set(channel.listeners)].join(", ") : "no part"}`;
  const button = element("button", { className: `containers-row comms-channel ${channel.refused ? "dead" : "running"}`, type: "button", title: detail }, element("span", { className: "containers-dot" }), element("span", { className: "name", textContent: channel.name }), element("span", { className: "status", textContent: status }), element("span", { className: "detail", textContent: detail }));
  button.dataset.key = channel.name;
  if (state.shown === channel.name) {
    button.setAttribute("aria-current", "true");
  }
  button.addEventListener("click", () => {
    state.shown = channel.name;
    parts.list.querySelectorAll(".comms-channel").forEach((one) => one.toggleAttribute("aria-current", one.dataset.key === channel.name));
    showLast();
  });
  return button;
}

// Puts the shown channel's last messages in the pane beside the list, the newest last.
function showLast() {
  const channel = state.channels.find((one) => one.name === state.shown);
  parts.title.textContent = channel ? `Last messages on ${channel.name}` : "";
  parts.out.textContent = channel ? channel.last.map((message) => `${message.from}: ${JSON.stringify(message.body)}`).join("\n") : "";
  parts.out.scrollTop = parts.out.scrollHeight;
}

function draw(listed, channels) {
  state.channels = channels;
  parts.list.replaceChildren(group("Parts", listed.length), ...listed.map(partRow), group("Channels", channels.length), ...channels.map(channelRow));
  parts.said.textContent = `${plural(listed.length, "part")}, ${plural(channels.length, "channel")}`;
  showLast();
}

// What the window shows, for the tests: each part's and each channel's row, and the pane's title
// and text.
export function commsShown() {
  const rows = (kind) => [...parts.list.querySelectorAll(`.comms-${kind}`)].map((one) => ({ name: one.dataset.key, status: one.querySelector(".status").textContent, detail: one.querySelector(".detail").textContent }));
  return { parts: rows("part"), channels: rows("channel"), title: parts.title.textContent, out: parts.out.textContent };
}

export function startComms() {
  parts.panel = document.getElementById("comms");
  const again = element("button", { className: "profile-action", type: "button", textContent: "Read Again" });
  again.addEventListener("click", refresh);
  parts.said = element("span", { className: "profile-said" });
  parts.bar = element("div", { className: "profile-bar" }, parts.said, again);
  parts.list = element("div", { className: "containers-list" });
  parts.title = element("div", { className: "containers-title" });
  parts.out = element("pre", { className: "containers-out" });
  parts.panel.append(parts.bar, element("div", { className: "containers-body" }, parts.list, element("div", { className: "containers-side" }, parts.title, parts.out)));
}
