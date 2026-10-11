// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The network, as net.rs in the command line's crate holds it: orior reaches an address only for
// what the reader asked of it, and sends nothing about the reader. Help, Off the Network holds it off
// the network entirely, kept as the setting `network`, which a tree's settings file does not set; each
// reach is then refused, saying what it was for. Help, Network… lists every address orior has
// reached, when and what for, this machine's own left out.

import { invoke } from "./bridge.js";
import { onSettings } from "./settings.js";

const KEY = "orior.network";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// Whether orior is held off the network.
export function offNetwork() {
  return localStorage.getItem(KEY) === "off";
}

export function setOffNetwork(off) {
  localStorage.setItem(KEY, off ? "off" : "on");
  return invoke("network_hold", { off }).catch(() => {});
}

// Tells orior's own side the setting as the window starts, and again each time a settings file
// changes it.
export function startNetwork() {
  invoke("network_hold", { off: offNetwork() }).catch(() => {});
  onSettings((changed) => changed.has("network") && invoke("network_hold", { off: offNetwork() }).catch(() => {}));
}

// Help, Network…: Off the Network, and every address reached, the last first, with when and what for.
export async function showNetwork(sheet) {
  const body = element("div", { className: "sheet-network" });
  const off = element("input", { type: "checkbox", checked: offNetwork() });
  const said = element("p", { className: "network-said" });
  const rows = element("div", { className: "network-rows" });
  const draw = async () => {
    said.textContent = offNetwork() ? "orior is held off the network: each address it would reach is refused, saying what for." : "orior reaches an address only for what you ask of it.";
    const reached = await invoke("network_reached").catch(() => []);
    rows.replaceChildren(
      ...(reached.length
        ? reached.map((one) => {
            const row = element(
              "div",
              { className: "network-row" },
              element("span", { className: "network-at", textContent: new Date(one.at).toLocaleString() }),
              element("code", { className: "network-address", textContent: one.address, title: one.address }),
              element("span", { className: "network-for", textContent: one.for }),
            );
            row.dataset.address = one.address;
            return row;
          })
        : [element("p", { className: "network-none", textContent: "orior has reached no address off this machine." })]),
    );
  };
  off.addEventListener("change", async () => {
    await setOffNetwork(off.checked);
    await draw();
  });
  body.append(element("h2", { textContent: "Network" }), element("label", { className: "report-check" }, off, element("span", { textContent: "Off the Network" })), said, rows);
  await draw();
  sheet(body);
}
