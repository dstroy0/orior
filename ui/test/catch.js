// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the page throws and writes as an error, retained for the harness where the window is driven
// through WebDriver, which tells the harness of neither. The harness puts it first in the page the
// window is built with, ahead of every module, and reads window.__harnessErrors after each file:
// each entry "threw ..." or "console.error ...", as the debugging port's events give them.

(() => {
  const retained = (window.__harnessErrors = []);
  // An error's message and its frames: WebKit's stack holds the frames alone.
  const told = (error) => (error instanceof Error ? `${error.message}\n${error.stack ?? ""}`.trim() : String(error));
  const words = (said) => said.map((one) => (one instanceof Error ? told(one) : typeof one === "string" ? one : JSON.stringify(one) ?? String(one))).join(" ");
  // Each resize observer the page makes, named by the line that made it, and its last deliveries: a loop
  // of them, which the page is told of with no word of which, is retained with the observers that ran within
  // RESIZE_RECENT ms before it and the boxes each was passed.
  const RESIZE_RECENT = 50;
  const deliveries = [];
  const madeAt = () => ((new Error().stack ?? "").split("\n").slice(1).find((line) => line.trim() && !line.includes("catch.js")) ?? "an unknown line").trim();
  const boxOf = (entry) => {
    const target = entry.target;
    const named = `${target.tagName.toLowerCase()}${target.id ? `#${target.id}` : ""}${target.classList.length ? `.${[...target.classList].join(".")}` : ""}`;
    return `${named} ${Math.round(entry.contentRect.width)}x${Math.round(entry.contentRect.height)}`;
  };
  const Observer = window.ResizeObserver;
  window.ResizeObserver = class extends Observer {
    constructor(callback) {
      const made = madeAt();
      super((entries, observer) => {
        deliveries.push({ at: performance.now(), said: `${made} for ${entries.map(boxOf).join(", ")}` });
        deliveries.splice(0, Math.max(0, deliveries.length - 16));
        callback(entries, observer);
      });
    }
  };
  const looped = (message) => {
    if (!/ResizeObserver loop/.test(message)) {
      return message;
    }
    const now = performance.now();
    const ran = deliveries.filter((one) => now - one.at < RESIZE_RECENT).map((one) => one.said);
    return `${message}; the observers that ran: ${ran.length ? ran.join("; ") : "none retained"}`;
  };
  addEventListener("error", (event) => retained.push(`threw ${event.error ? told(event.error) : looped(event.message)}`));
  // Read once every listener has had it: a rejection a test sets aside, by preventing its default,
  // is not retained.
  addEventListener("unhandledrejection", (event) => setTimeout(() => event.defaultPrevented || retained.push(`threw ${told(event.reason)}`)));
  const error = console.error.bind(console);
  console.error = (...said) => {
    retained.push(`console.error ${words(said)}`);
    error(...said);
  };
  const assert = console.assert.bind(console);
  console.assert = (ok, ...said) => {
    if (!ok) {
      retained.push(`console.error ${words(said)}`);
    }
    assert(ok, ...said);
  };
})();
