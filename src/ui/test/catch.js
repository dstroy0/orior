// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the page throws and writes as an error, kept for the harness where the window is driven
// through WebDriver, which tells the harness of neither. driven.rs puts it first in the page the
// window is built with, ahead of every module, and reads window.__harnessErrors after each file:
// each entry "threw ..." or "console.error ...", as the debugging port's events give them.

(() => {
  const kept = (window.__harnessErrors = []);
  const words = (said) => said.map((one) => (one instanceof Error ? one.stack || String(one) : typeof one === "string" ? one : JSON.stringify(one) ?? String(one))).join(" ");
  addEventListener("error", (event) => kept.push(`threw ${event.error?.stack || event.message}`));
  addEventListener("unhandledrejection", (event) => kept.push(`threw ${event.reason?.stack || event.reason}`));
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
