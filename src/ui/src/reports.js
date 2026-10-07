// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Error reports from the window. An error no part of the window caught goes to the Rust side, which
// files it as an issue on its own where the reader lets errors file; Help, Report a Bug opens a form
// for a report the reader writes. Where a report went shows in the status bar, its issue a click away.

import { invoke } from "./bridge.js";

// A script's address, struck from what it says: a report reads the same from any window.
const ORIGIN = new RegExp(location.origin.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"), "g");

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// Shows where a report went: the issue it filed or found, or the page opened for the reader.
function drawFiled(filed) {
  const node = document.getElementById("status-report");
  if (!filed || !node) {
    return;
  }
  const number = filed.url.match(/\/issues\/(\d+)/)?.[1];
  node.hidden = false;
  node.textContent = number ? `⚠ #${number}` : "⚠";
  node.title = filed.url;
  node.onclick = () => invoke("report_open", { url: filed.url }).catch(() => {});
}

async function reportError(message, detail) {
  const filed = await invoke("report_error", { category: "ui", message: message.replace(ORIGIN, ""), detail: detail.replace(ORIGIN, "") }).catch(() => null);
  drawFiled(filed);
}

export function catchErrors() {
  window.addEventListener("error", (event) => {
    const where = event.filename ? `${event.filename}:${event.lineno}:${event.colno}` : "";
    reportError(event.message || "an error", [where, event.error?.stack ?? ""].filter(Boolean).join("\n"));
  });
  window.addEventListener("unhandledrejection", (event) => {
    const reason = event.reason;
    const message = reason instanceof Error ? reason.message : String(reason);
    reportError(`unhandled rejection: ${message}`, reason instanceof Error ? (reason.stack ?? "") : "");
  });
}

// The form for a report the reader writes, in a sheet over the app.
export function reportForm(sheet, given = []) {
  const categories = ["library", "cli", "ui", "unknown"];
  const firstCategory = categories.includes(given[0]) ? given[0] : "unknown";
  const category = element("select", { className: "report-field", name: "category" }, ...categories.map((name) => element("option", { value: name, textContent: name, selected: name === firstCategory })));
  const title = element("input", { className: "report-field", name: "title", type: "text", value: (categories.includes(given[0]) ? given.slice(1) : given).join(" "), spellcheck: true });
  const what = element("textarea", { className: "report-field", name: "what", rows: 5, spellcheck: true });
  const steps = element("textarea", { className: "report-field", name: "steps", rows: 4, spellcheck: true });
  const withErrors = element("input", { type: "checkbox", checked: true });
  const said = element("p", { className: "report-said" });
  const send = element("button", { className: "primary", type: "submit", textContent: "Submit" });
  const label = (text, field) => element("label", { className: "report-row" }, element("span", { textContent: text }), field);
  const form = element(
    "form",
    { className: "sheet-report" },
    element("h2", { textContent: "Report a Bug" }),
    label("Category", category),
    label("Title", title),
    label("What happened", what),
    label("Steps to reproduce", steps),
    element("label", { className: "report-check" }, withErrors, element("span", { textContent: "Include recent errors" })),
    element("div", { className: "report-foot" }, said, send)
  );
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (!title.value.trim() || !what.value.trim()) {
      said.textContent = !title.value.trim() ? "Title is required." : "What happened is required.";
      (title.value.trim() ? what : title).focus();
      return;
    }
    send.disabled = true;
    said.textContent = "Submitting…";
    const report = { category: category.value, title: title.value.trim(), what: what.value, steps: steps.value, logs: "" };
    try {
      const filed = await invoke("report_bug", { report, withErrors: withErrors.checked });
      drawFiled(filed);
      said.textContent = filed.kind === "page" ? "Opened in the browser to submit there." : filed.kind === "known" ? `Already reported: ${filed.url}` : `Reported: ${filed.url}`;
    } catch (error) {
      said.textContent = String(error);
      send.disabled = false;
    }
  });
  sheet(form);
  (title.value ? what : title).focus();
}

// The question asked once, on the first run where the installer did not ask it: whether errors file
// on their own. Yes is the answer Enter gives, and a sheet closed with no answer leaves them on.
export function askReports(sheet, question, answered) {
  let given = null;
  const yes = element("button", { className: "primary", type: "submit", textContent: "File them", autofocus: true });
  const no = element("button", { type: "button", textContent: "Don't" });
  const form = element(
    "form",
    { className: "sheet-report" },
    element("h2", { textContent: "Automatic Error Reports" }),
    element("p", { textContent: question }),
    element("div", { className: "report-foot" }, no, yes)
  );
  const dialog = sheet(form);
  const answer = (on) => {
    given = on;
    dialog.close();
  };
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    answer(true);
  });
  no.addEventListener("click", () => answer(false));
  dialog.addEventListener("close", async () => {
    const on = given ?? true;
    await invoke("report_auto_set", { on }).catch(() => {});
    answered(on);
  });
  yes.focus();
}

export function autoReports() {
  return invoke("report_auto");
}
