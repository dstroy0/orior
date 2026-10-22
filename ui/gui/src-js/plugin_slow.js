// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find the Slow Plugin: the plugins on halved, half of them off at a time, the reader asked each time
// whether the window is still slow, until one is left; then each set back as it was.

import { pluginReadings, pluginsOff, setPluginOn } from "./plugins.js";
import { syncPlugins } from "./scripts.js";
import { say } from "./statusbar.js";

const state = { finding: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

export function findSlowPlugin() {
  // A finding begun again starts from the plugins the reader had off before the first began.
  const off = state.finding?.was ?? pluginsOff();
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
