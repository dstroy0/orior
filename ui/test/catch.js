// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the page throws and writes as an error, kept for the harness where the window is driven
// through WebDriver, which tells the harness of neither. The harness puts it first in the page the
// window is built with, ahead of every module, and reads window.__harnessErrors after each file:
// each entry "threw ..." or "console.error ...", as the debugging port's events give them.

(() => {
  const kept = (window.__harnessErrors = []);
  // An error's message and its frames: WebKit's stack holds the frames alone.
  const told = (error) => (error instanceof Error ? `${error.message}\n${error.stack ?? ""}`.trim() : String(error));
  const words = (said) => said.map((one) => (one instanceof Error ? told(one) : typeof one === "string" ? one : JSON.stringify(one) ?? String(one))).join(" ");
  addEventListener("error", (event) => kept.push(`threw ${event.error ? told(event.error) : event.message}`));
  // Read once every listener has had it: a rejection a test sets aside, by preventing its default,
  // is not kept.
  addEventListener("unhandledrejection", (event) => setTimeout(() => event.defaultPrevented || kept.push(`threw ${told(event.reason)}`)));
  const error = console.error.bind(console);
  console.error = (...said) => {
    kept.push(`console.error ${words(said)}`);
    error(...said);
  };
  const assert = console.assert.bind(console);
  console.assert = (ok, ...said) => {
    if (!ok) {
      kept.push(`console.error ${words(said)}`);
    }
    assert(ok, ...said);
  };
})();
