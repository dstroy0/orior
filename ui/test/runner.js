// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The test runner inside the window, put into the page by the harness, utils/test/harness_page.py. A test file of test/src/ takes
// what it needs from window.__harness:
//
//   const { test, assert, ui, calls } = window.__harness;
//   test("Save writes the file", async () => { await ui.key("Ctrl+S"); assert.equal(...); });
//
// `ui` drives the window as a person does: each press, key, drag, hover and wheel goes to the
// harness, which sends it through the debugging port as real input, the page receiving it as it
// would from the mouse and keyboard. A step waits for the harness to have sent it. `calls` keeps the
// calls the page makes and lets a test answer one itself, as the system's picker.

(() => {
  if (window.__harness) {
    return "kept";
  }
  const harness = { tests: [], waiting: new Map(), next: 1 };
  window.__harness = harness;

  const describe = (value) => {
    try {
      return typeof value === "string" ? JSON.stringify(value) : JSON.stringify(value) ?? String(value);
    } catch {
      return String(value);
    }
  };

  class Failure extends Error {}

  const assert = {
    ok(value, message = "expected a true value") {
      if (!value) {
        throw new Failure(`${message}: got ${describe(value)}`);
      }
    },
    equal(actual, expected, message = "not equal") {
      if (!Object.is(actual, expected)) {
        throw new Failure(`${message}: got ${describe(actual)}, expected ${describe(expected)}`);
      }
    },
    notEqual(actual, unexpected, message = "equal") {
      if (Object.is(actual, unexpected)) {
        throw new Failure(`${message}: both ${describe(actual)}`);
      }
    },
    deepEqual(actual, expected, message = "not the same") {
      if (describe(actual) !== describe(expected)) {
        throw new Failure(`${message}: got ${describe(actual)}, expected ${describe(expected)}`);
      }
    },
    match(text, pattern, message = "no match") {
      if (!pattern.test(String(text))) {
        throw new Failure(`${message}: ${describe(text)} against ${pattern}`);
      }
    },
    near(actual, expected, within, message = "not near") {
      if (!(Math.abs(actual - expected) <= within)) {
        throw new Failure(`${message}: got ${actual}, expected ${expected} within ${within}`);
      }
    },
    async rejects(work, pattern = /./, message = "did not fail") {
      try {
        await work();
      } catch (error) {
        if (!pattern.test(String(error?.message ?? error))) {
          throw new Failure(`${message}: failed with ${describe(String(error))}, not ${pattern}`);
        }
        return;
      }
      throw new Failure(message);
    },
  };

  // A step the harness performs through the debugging port.
  const ask = (step) =>
    new Promise((done, fail) => {
      const id = harness.next++;
      harness.waiting.set(id, { done, fail });
      window.__harnessCall(JSON.stringify({ id, ...step }));
    });
  harness.answer = (id, error) => {
    const waiting = harness.waiting.get(id);
    harness.waiting.delete(id);
    if (error) {
      waiting?.fail(new Error(error));
    } else {
      waiting?.done();
    }
  };

  const elementOf = (target) => {
    if (target instanceof Element) {
      return target;
    }
    const found = document.querySelector(target);
    if (!found) {
      throw new Failure(`nothing on the page answers ${target}`);
    }
    return found;
  };

  const frame = () => new Promise((next) => requestAnimationFrame(() => next()));

  const named = (node) => (node ? `${node.tagName.toLowerCase()}${node.id ? `#${node.id}` : ""}${typeof node.className === "string" && node.className ? `.${node.className.trim().split(/\s+/).join(".")}` : ""}` : "nothing");

  // The middle of a target in the window, or the point given, once a person could press it: the
  // target shown, in view, scrolled into it where it is not, standing still from one frame to the
  // next, and the topmost thing at its middle, nothing laid over it. Within 5 s, or the step fails
  // saying which of these it waited for.
  const pointOf = async (target) => {
    if (target && typeof target === "object" && "x" in target && "y" in target && !(target instanceof Element)) {
      return { x: target.x, y: target.y };
    }
    const label = typeof target === "string" ? target : named(target);
    const until = performance.now() + 5000;
    let last = null;
    let why = "";
    // An element a scrolling part clips, as a sheet's own scroll does, is scrolled into view once.
    let scrolled = false;
    // A scroll made here in a modal dialog is followed by the pointer moved onto the element and the
    // element measured again: WebKitGTK undoes the first scroll a script makes in a modal dialog just
    // opened when the pointer next moves. A move elsewhere would undo scrolls a test made itself.
    let moveDue = false;
    let moves = 0;
    while (performance.now() < until) {
      const node = elementOf(target);
      const box = node.getBoundingClientRect();
      if (box.bottom < 0 || box.top > innerHeight || box.right < 0 || box.left > innerWidth) {
        node.scrollIntoView({ block: "center", inline: "center" });
        moveDue = Boolean(node.closest("dialog[open]"));
      }
      const seen = node.getBoundingClientRect();
      const point = { x: seen.left + seen.width / 2, y: seen.top + seen.height / 2 };
      // Still once it stands where it stood a frame before and nothing holding it is animating: two
      // slow frames can fall on one moment of a menu's growth, and its items move after.
      const animating = (() => {
        for (let one = node; one; one = one.parentElement) {
          if (one.getAnimations().some((animation) => animation.playState === "running")) {
            return true;
          }
        }
        return false;
      })();
      const still = last && last.x === point.x && last.y === point.y && !animating;
      last = point;
      const hit = document.elementFromPoint(point.x, point.y);
      if (seen.width === 0 || seen.height === 0 || getComputedStyle(node).visibility === "hidden") {
        why = "is not shown";
      } else if (!still) {
        why = "keeps moving";
      } else if (!hit || !(hit === node || node.contains(hit))) {
        why = `is under ${named(hit)}`;
        if (!scrolled) {
          scrolled = true;
          node.scrollIntoView({ block: "center", inline: "nearest" });
          moveDue = Boolean(node.closest("dialog[open]"));
        }
      } else if (moveDue && moves < 2) {
        // The scroll the move undoes is made again, once.
        moveDue = false;
        moves += 1;
        scrolled = false;
        last = null;
        await ask({ op: "move", ...point });
      } else {
        return point;
      }
      await frame();
    }
    throw new Failure(`${label} ${why}`);
  };

  const ui = {
    click: async (target, { button = "left", count = 1, keys = "" } = {}) => ask({ op: "click", ...(await pointOf(target)), button, count, keys }),
    double: async (target) => ask({ op: "click", ...(await pointOf(target)), button: "left", count: 2, keys: "" }),
    rightClick: async (target) => ask({ op: "click", ...(await pointOf(target)), button: "right", count: 1, keys: "" }),
    hover: async (target) => ask({ op: "move", ...(await pointOf(target)) }),
    drag: async (from, to, { steps = 12 } = {}) => {
      const start = await pointOf(from);
      const end = to && typeof to === "object" && "x" in to && !(to instanceof Element) ? to : (() => {
        const box = elementOf(to).getBoundingClientRect();
        return { x: box.left + box.width / 2, y: box.top + box.height / 2 };
      })();
      await ask({ op: "drag", x: start.x, y: start.y, toX: end.x, toY: end.y, steps });
    },
    wheel: async (target, deltaY, deltaX = 0) => ask({ op: "wheel", ...(await pointOf(target)), deltaX, deltaY }),
    // Keys as the menus write them: Ctrl+Shift+P, F5, Escape, Alt+Enter.
    key: async (keys) => ask({ op: "key", keys }),
    // Modifier keys held down until let go: Ctrl, Alt, Shift, or more of them joined, Ctrl+Shift.
    hold: async (keys) => ask({ op: "hold", keys }),
    release: async (keys) => ask({ op: "release", keys }),
    // A key held down for `count` presses, one every `every` milliseconds, as the system's repeat sends it.
    repeat: async (keys, count, every = 33) => ask({ op: "repeat", keys, count, every }),
    // The page is told it has the system's focus while the harness runs it; off, it has the focus
    // the window truly has, which a window behind another lacks.
    focusTold: async (on) => ask({ op: "focus", on }),
    // A media feature the system sets, as prefers-reduced-motion, told to the page as `value`; with
    // no value, the page has what the system truly sets.
    media: async (name, value = "") => ask({ op: "media", name, value }),
    // A program started apart from the window, waited for unless `wait` is false: its words, its
    // environment and what its standard input reads.
    run: async (program, args = [], { env = {}, input = "", wait = true } = {}) => ask({ op: "run", program, args, env, input, wait }),
    // The browser's own trace of the window's frames, started, and ended with its summary handed to the
    // query run `qry` as the file `name`, a path under the run's top, by the run's `writer`.
    browserTrace: async () => ask({ op: "browser_trace" }),
    browserTraced: async (writer, qry, name) => ask({ op: "browser_trace", end: true, writer, qry, name }),
    type: async (text) => ask({ op: "type", text }),
    // Waits for `what`, a selector shown or a function's true value, and gives it.
    // Waits for `what`, a test or a selector, at most `ms`; `said` names it where it never comes.
    waitFor: async (what, ms = 5000, said = null) => {
      const until = performance.now() + ms;
      while (performance.now() < until) {
        const value = typeof what === "function" ? await what() : document.querySelector(what);
        if (value && !(value instanceof Element && value.getBoundingClientRect().width === 0 && value.getBoundingClientRect().height === 0)) {
          return value;
        }
        await new Promise((next) => setTimeout(next, 30));
      }
      throw new Failure(`waited ${ms} ms for ${said ?? (typeof what === "function" ? "the condition" : what)}`);
    },
    // The gaps between the frames drawn over `ms`, to say how smoothly something moved.
    frames: async (ms) => {
      const gaps = [];
      let last = performance.now();
      const until = last + ms;
      while (performance.now() < until) {
        await frame();
        const now = performance.now();
        gaps.push(now - last);
        last = now;
      }
      return gaps;
    },
    frame,
    rest: (ms) => new Promise((next) => setTimeout(next, ms)),
    text: (target) => elementOf(target).textContent.trim(),
    shown: (target) => {
      const node = typeof target === "string" ? document.querySelector(target) : target;
      return Boolean(node && node.getClientRects().length && getComputedStyle(node).visibility !== "hidden");
    },
  };

  // Every call the page makes goes through the watch bridge.js keeps, which this sets. A call a test
  // answers itself is answered by it; one that would leave the window, by quitting or moving it,
  // opening another, the system's picker, the browser or the desktop, the network, a report, an
  // install, or a plugin of the system's other than its events, fails with HELD in its message, the
  // picker answering as the reader would who closed it; every other call is made. Each call a test
  // causes is kept in `calls.made`, and what a test answers is forgotten once it ends.
  const HELD = "held by the test harness";
  const REFUSED = new Set([
    "app_exit", "window_open", "window_act", "view_open", "link_open", "home_reveal", "templates_reveal",
    "report_bug", "report_open", "report_error", "report_auto_set", "toolchain_install", "repo_clone", "clone_start",
    "remote_get", "remote_rejoin", "git_push", "git_pull", "git_fetch", "template_keep", "plugin_create", "project_create",
    "walker_probe", "print_page",
  ]);
  const refused = (name) => REFUSED.has(name) || name.startsWith("deploy") || name === "pick" || (name.startsWith("plugin:") && !name.startsWith("plugin:event|"));
  const calls = { answers: new Map(), made: [] };
  calls.answer = (name, answer) => calls.answers.set(name, answer);
  calls.forget = () => {
    calls.answers.clear();
    calls.made.length = 0;
  };
  calls.watcher = (name, args, send) => {
    if (!name.startsWith("plugin:event")) {
      calls.made.push({ name, args });
    }
    if (calls.answers.has(name)) {
      return (async () => calls.answers.get(name)(args))();
    }
    if (refused(name)) {
      return name === "pick" ? Promise.resolve(null) : Promise.reject(new Error(`${name}: ${HELD}`));
    }
    return send(name, args);
  };
  harness.watching = import(`${location.origin}/bridge.js`).then((bridge) => bridge.watchCalls(calls.watcher));

  // A test is run again where it fails, up to `tries` runs in all, so the frames a screenshot, a
  // notification or any other foreground work takes from one run do not fail it: a test passes on
  // the first run that passes, and fails with the last run's error.
  harness.test = (name, work, { timeout = 30000, tries = 3 } = {}) => harness.tests.push({ name, work, timeout, tries });
  harness.assert = assert;
  harness.ui = ui;
  harness.calls = calls;
  harness.Failure = Failure;
  // What a held call's error says, for a test that sets those errors aside.
  harness.held = HELD;

  // The memory after each test, the system's account of the app and the cellblock's own, a row of
  // memory.tsv beside the run's report: the test, the bytes in RAM and reserved over every process,
  // the app's own process's, the cellblock's pool and its two peaks, and the share of the app's
  // reserved bytes the pool accounts for.
  const memoryRows = ["test\tworking\tcommit\town_working\town_commit\tpool\tpersistent\ttemporary\taccounted\ttallies\tkept_calls"];
  let counting = false;
  // The rows the files before this one wrote, read once, so the table runs across the whole run and
  // not the last file alone.
  let kept_before = null;
  async function memorySampled(name) {
    const { invoke } = await import(`${location.origin}/bridge.js`);
    const home = harness.servers?.ORIOR_TEST_HOME;
    if (kept_before === null && home) {
      const path = `${String(home).replace(/\\/g, "/")}/../memory.tsv`;
      const text = await invoke("file_read_any", { path }).catch(() => "");
      kept_before = text.split("\n").filter((line) => line.trim() && !line.startsWith("test\t"));
      memoryRows.push(...kept_before);
    }
    const read = await invoke("memory_use").catch(() => null);
    const cells = await invoke("cells_said").catch(() => null);
    // the three modules holding the most at their peaks, each as name:bytes, for the audit of who holds what
    const listed = await invoke("cells_tallies").catch(() => null);
    const tallies = (listed?.tallies ?? []).sort((a, b) => b.hw - a.hw).slice(0, 3).map((one) => `${one.name}:${one.hw}`).join(",");
    // the three calls of the tree's side that kept the most bytes over the test, each as name:bytes, the count
    // turned on at the first sample and reset at each
    const calls = await invoke("memory_calls", { on: counting ? undefined : true, reset: true }).catch(() => null);
    counting = true;
    const kept = Object.entries(calls ?? {}).filter(([, one]) => one.kept > 0).sort((a, b) => b[1].kept - a[1].kept).slice(0, 3).map(([call, one]) => `${call}:${one.kept}`).join(",");
    if (!read || !cells || !home) {
      return;
    }
    const own = read.parts[0] ?? { working: 0, commit: 0 };
    const accounted = own.commit > 0 ? (cells.pool / own.commit).toFixed(3) : "";
    memoryRows.push([name, read.working, read.commit, own.working, own.commit, cells.pool, cells.persistent, cells.temporary, accounted, tallies, kept].join("\t"));
    await invoke("file_write_any", { path: `${String(home).replace(/\\/g, "/")}/../memory.tsv`, text: `${memoryRows.join("\n")}\n` }).catch(() => {});
  }

  // Runs the tests a file gave, in order, and gives each one's name, whether it passed, why not, and
  // how long it took; the file's tests are taken away once run.
  harness.run = async () => {
    await harness.watching;
    const tests = harness.tests.splice(0);
    const results = [];
    for (const one of tests) {
      const started = performance.now();
      for (let ran = 1; ran <= one.tries; ran += 1) {
        calls.forget();
        try {
          await Promise.race([one.work(), new Promise((_, fail) => setTimeout(() => fail(new Failure(`it ran past ${one.timeout} ms`)), one.timeout))]);
          results.push({ name: one.name, ok: true, ms: Math.round(performance.now() - started) });
          break;
        } catch (error) {
          // A stack of V8's opens with the message; one of WebKit's holds the frames alone.
          const message = String(error?.message ?? error);
          const stack = String(error?.stack ?? "");
          const told = stack.includes(message) ? stack : `${message}\n${stack}`;
          // A step only the window with its debugging port can take, met under WebDriver: the test is
          // not run here, and says why, where it would fail for what the window cannot be asked.
          const notHere = message.startsWith("not run on a WebDriver window:");
          if (notHere || ran === one.tries) {
            const tried = ran > 1 ? `after ${ran} runs: ` : "";
            results.push({ name: one.name, ok: notHere, ms: Math.round(performance.now() - started), error: notHere ? message : `${tried}${told.split("\n").slice(0, 4).join("\n")}` });
            break;
          }
        } finally {
          await memorySampled(one.name);
          await ui.focusTold(true).catch(() => {});
          document.querySelectorAll("dialog[open]").forEach((one) => one.close());
          try {
            (await import(`${location.origin}/menu.js`)).closeMenu?.(false);
          } catch {}
        }
      }
    }
    return results;
  };
  return "ready";
})();
//# sourceURL=harness-runner.js
