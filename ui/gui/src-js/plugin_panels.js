// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panels script plugins show over the window, each its HTML in a frame that runs none of its
// scripts. An element of a panel that names a command in `data-command`, pressed, and Enter in a
// field of a form that holds one, is heard by the plugin as `panel`: `{ panel, command, value,
// fields }`, the panel's id, the element's `data-command` and `data-value`, and the fields of the form
// it stands in, by their names. A link pressed in a panel is heard the same, its command `link` and
// its value its address.

import { motionReduced } from "./motion.js";

// What a plugin's panel is drawn with where less motion is asked for.
const STILL_PANEL = "<style>*,*::before,*::after{animation:none!important;transition:none!important;scroll-behavior:auto!important}</style>";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The fields of the form `form` stands in, by their names: a check's whether it is checked.
function fieldsOf(form) {
  const fields = {};
  for (const field of form?.querySelectorAll("input[name], select[name], textarea[name]") ?? []) {
    fields[field.name] = field.type === "checkbox" ? field.checked : field.value;
  }
  return fields;
}

// What a panel's document does heard by its plugin `one` as `panel`.
function heardIn(one, id, frame) {
  const page = frame.contentDocument;
  if (!page) {
    return;
  }
  const tell = (command, value, form) => one.worker.postMessage({ event: "panel", detail: { panel: id, command, value, fields: fieldsOf(form) } });
  page.addEventListener("click", (event) => {
    const link = event.target.closest?.("a[href]");
    const named = event.target.closest?.("[data-command]");
    if (named) {
      event.preventDefault();
      tell(named.dataset.command, named.dataset.value ?? null, named.closest("form"));
    } else if (link) {
      event.preventDefault();
      tell("link", link.getAttribute("href"), null);
    }
  });
  page.addEventListener("keydown", (event) => {
    const form = event.target.closest?.("form");
    const named = form?.querySelector("[data-command]");
    if (event.key === "Enter" && named && event.target.tagName !== "TEXTAREA") {
      event.preventDefault();
      tell(named.dataset.command, named.dataset.value ?? null, form);
    }
  });
  // the document marked as heard, so what waits on a panel knows a press in it now reaches the plugin
  page.documentElement.dataset.heard = "";
}

// The panels of the plugins `running` drawn, each kept as it was where its HTML is the same.
export function drawPanels(running) {
  const host = document.getElementById("plugin-panels");
  const shown = [...running.values()].flatMap((one) => [...one.panels].map(([id, panel]) => ({ one, id, panel })));
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
        drawPanels(running);
      });
      let frame = made.querySelector("iframe");
      if (!frame) {
        frame = element("iframe", { className: "plugin-panel-frame", title: String(panel.title ?? id) });
        // The frame's document is the window's to read, for what is pressed in it, and runs no script.
        frame.setAttribute("sandbox", "allow-same-origin");
        frame.addEventListener("load", () => heardIn(one, id, frame));
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
