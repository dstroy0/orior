// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The walker's watch inside the window, put into the page by walk.py as a script of its own.
//
// Every call the page makes goes through the watch bridge.js keeps, which this sets: each is timed,
// and a call that would leave the window is answered here and never made: one that quits or moves
// the window, opens another window, the system's picker, the browser or a folder on the desktop,
// sends to the network, files a report the reader writes, installs a tool, keeps a template or a
// plugin in orior's own folder, or reaches a plugin of the system's other than its events. The
// picker answers as the reader would who closed it, and every other such call fails with HELD in
// its message, which the walker does not count as a fault. walker_probe is held too, for the walker
// to see the watch hold before it presses anything. The page's print and its window.open do nothing.
// An error the window meets files as an issue, as each error the walk finds is kept track of.
//
// Each frame's time is kept, and where every element an animation runs on stands, as a series per
// element, an animation that loops without end, as the caret's blink, left out; each layout shift with the elements it moved and from where to where; each long animation
// frame with the scripts it ran; and each error the page throws, rejects or writes to the console.

(() => {
  if (window.__walk) {
    return "kept";
  }
  const HELD = "held by the walker";
  const REFUSED = new Set([
    "app_exit", "window_open", "window_act", "view_open", "link_open", "home_reveal", "templates_reveal",
    "report_bug", "report_open", "report_auto_set", "toolchain_install", "repo_clone", "clone_start",
    "remote_get", "remote_rejoin", "git_push", "git_pull", "git_fetch", "template_keep", "plugin_create", "project_create",
    "walker_probe", "print_page",
  ]);
  // A plugin's call reaches the system itself, a dialog, the shell or the process; only events pass.
  const refused = (name) => REFUSED.has(name) || name.startsWith("deploy") || name === "pick" || (name.startsWith("plugin:") && !name.startsWith("plugin:event|"));

  const walk = {
    frames: [],
    moves: new Map(),
    shifts: [],
    loafs: [],
    errors: [],
    calls: [],
    held: [],
    out: 0,
    lastMotion: 0,
    since: performance.now(),
  };
  window.__walk = walk;

  // A name for a node a person can find: its tag, id and first classes, and for a node of text its
  // parent's.
  const describe = (node) => {
    if (!node) {
      return "";
    }
    const element = node.nodeType === 1 ? node : node.parentElement;
    if (!element) {
      return "#text";
    }
    const classes = typeof element.className === "string" ? element.className.trim().split(/\s+/).filter(Boolean).slice(0, 3) : [];
    const label = element.getAttribute("aria-label") || element.title || "";
    return `${element.tagName.toLowerCase()}${element.id ? `#${element.id}` : ""}${classes.map((one) => `.${one}`).join("")}${label ? ` "${label.slice(0, 40)}"` : ""}`;
  };
  const rect = (box) => (box ? [Math.round(box.x), Math.round(box.y), Math.round(box.width), Math.round(box.height)] : null);

  // The bridge, through its watch: each call timed, each that would leave the window answered here.
  const watcher = (name, args, send) => {
    const at = performance.now();
    if (refused(name)) {
      walk.held.push({ t: at, name });
      return name === "pick" ? Promise.resolve(null) : Promise.reject(new Error(`${name}: ${HELD}`));
    }
    const answer = send(name, args);
    if (!name.startsWith("plugin:event")) {
      walk.out += 1;
      const back = (record) => {
        walk.out -= 1;
        walk.lastMotion = performance.now();
        walk.calls.push({ t: at, name, ms: performance.now() - at, ...record });
      };
      answer.then(
        () => back({ ok: true }),
        (error) => back({ ok: false, error: String(error).slice(0, 300) }),
      );
    }
    return answer;
  };
  walk.guarded = import("/bridge.js").then((bridge) => {
    walk.bridge = bridge;
    return bridge.watchCalls(watcher);
  });
  walk.print = window.print;
  walk.open = window.open;
  window.print = () => walk.held.push({ t: performance.now(), name: "print" });
  window.open = () => (walk.held.push({ t: performance.now(), name: "window.open" }), null);

  // Each frame, and where each element an animation runs on stands in it.
  const frame = (now) => {
    walk.frames.push(now);
    if (walk.frames.length > 6000) {
      walk.frames.splice(0, 3000);
    }
    let moving = false;
    for (const animation of document.getAnimations()) {
      // An animation that loops without end, as the caret's blink, is the window at rest.
      if (animation.playState !== "running" || animation.effect?.getComputedTiming?.().iterations === Infinity) {
        continue;
      }
      moving = true;
      const target = animation.effect?.target;
      if (!(target instanceof Element) || !target.isConnected) {
        continue;
      }
      const key = describe(target);
      let series = walk.moves.get(key);
      if (!series) {
        series = { name: animation.animationName || animation.transitionProperty || "animation", points: [] };
        walk.moves.set(key, series);
      }
      const last = series.points.at(-1);
      if (last?.t !== now) {
        series.points.push({ t: now, box: rect(target.getBoundingClientRect()) });
      }
    }
    if (moving) {
      walk.lastMotion = now;
    }
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);

  new PerformanceObserver((list) => {
    for (const entry of list.getEntries()) {
      walk.lastMotion = performance.now();
      walk.shifts.push({ t: entry.startTime, value: entry.value, sources: (entry.sources ?? []).map((one) => ({ node: describe(one.node), from: rect(one.previousRect), to: rect(one.currentRect) })) });
    }
  }).observe({ type: "layout-shift" });
  try {
    new PerformanceObserver((list) => {
      for (const entry of list.getEntries()) {
        walk.loafs.push({
          t: entry.startTime,
          ms: entry.duration,
          blocking: entry.blockingDuration,
          scripts: (entry.scripts ?? []).map((one) => ({ at: `${(one.sourceURL || "").split("/").pop()}:${one.sourceCharPosition}`, fn: one.sourceFunctionName, invoker: one.invoker, ms: Math.round(one.duration) })),
        });
      }
    }).observe({ type: "long-animation-frame" });
  } catch {
    walk.loafs = null;
  }

  const fault = (kind, text) => walk.errors.push({ t: performance.now(), kind, text: String(text).slice(0, 600) });
  window.addEventListener("error", (event) => fault("error", event.error?.stack || event.message));
  window.addEventListener("unhandledrejection", (event) => fault("rejection", event.reason?.stack || event.reason));
  const written = console.error;
  console.error = (...args) => {
    fault("console", args.map((one) => (one instanceof Error ? one.stack : typeof one === "string" ? one : JSON.stringify(one))).join(" "));
    written.apply(console, args);
  };

  // Starts a command's record afresh.
  walk.begin = () => {
    walk.since = performance.now();
    walk.frames = [];
    walk.moves = new Map();
    walk.shifts = [];
    walk.loafs = walk.loafs === null ? null : [];
    walk.errors = [];
    walk.calls = [];
    walk.held = [];
    return walk.since;
  };

  // How long since anything moved: an animation ran, the layout shifted or a call came back; none
  // while a call is out.
  walk.quiet = () => (walk.out > 0 ? 0 : Math.round(performance.now() - Math.max(walk.lastMotion, walk.since)));

  // Runs a command of the menus as a press of its item does, and says how long its own work took
  // and whether it ended within `wait`.
  walk.run = async (command, args = [], wait = 5000) => {
    const menus = await import("/menubar.js");
    if (menus.hasCommand && !menus.hasCommand(command)) {
      return { missing: true };
    }
    const started = performance.now();
    let threw = null;
    let ended = true;
    try {
      const done = menus.runCommand(command, args);
      const sync = performance.now() - started;
      let prompted = false;
      if (done && typeof done.then === "function") {
        // A command that asks for a line waits on it: the walker answers it once this gives back.
        const asked = new Promise((stop) => {
          const look = () => (document.querySelector("dialog[open] form.sheet-ask") ? ((prompted = true), stop(false)) : setTimeout(look, 40));
          look();
        });
        ended = await Promise.race([done.then(() => true, (error) => ((threw = String(error?.stack || error)), true)), asked, new Promise((stop) => setTimeout(() => stop(false), wait))]);
      }
      return { sync: Math.round(sync), took: Math.round(performance.now() - started), ended: ended || prompted, prompted, threw };
    } catch (error) {
      return { sync: Math.round(performance.now() - started), took: Math.round(performance.now() - started), ended: true, threw: String(error?.stack || error) };
    }
  };

  // What the window shows over its views: open sheets, menus, the palette, a diff or merge, a prompt.
  walk.overlays = () => {
    const shown = [];
    document.querySelectorAll("dialog[open]").forEach((one) => shown.push(`dialog${one.className ? `.${one.className}` : ""}: ${(one.innerText || "").trim().split("\n")[0].slice(0, 60)}`));
    document.querySelectorAll(".menu").forEach((one) => one.offsetParent !== null && shown.push("menu"));
    document.querySelectorAll(".diff-view, .merge-view").forEach((one) => shown.push(describe(one)));
    const palette = document.querySelector(".palette");
    if (palette && palette.offsetParent !== null) {
      shown.push("palette");
    }
    return shown;
  };

  // Answers an open prompt with `text`, or closes it where `text` is null. Says whether one was open.
  walk.answer = (text) => {
    const form = document.querySelector("dialog[open] form.sheet-ask");
    if (!form) {
      return false;
    }
    const field = form.querySelector("input");
    if (text === null) {
      form.closest("dialog").close();
    } else {
      field.value = text;
      form.requestSubmit();
    }
    return true;
  };

  // Closes what a command left over the views: menus, sheets, the palette, a diff or merge.
  walk.clear = async () => {
    const press = (target) => target?.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", code: "Escape", bubbles: true, cancelable: true }));
    for (let round = 0; round < 3; round += 1) {
      press(document.activeElement ?? document.body);
      document.querySelectorAll("dialog[open]").forEach((one) => one.close());
      (await import("/menu.js")).closeMenu?.(false);
      (await import("/diffview.js")).closeDiff?.();
      await new Promise((next) => requestAnimationFrame(next));
    }
    return walk.overlays();
  };

  // What the command did, from the moment `begin` was asked.
  walk.report = () => {
    const frames = walk.frames;
    const gaps = frames.slice(1).map((now, at) => now - frames[at]);
    return {
      active: Math.round(Math.max(walk.lastMotion, walk.since) - walk.since),
      frames: frames.length,
      gaps: gaps.map((gap) => Math.round(gap * 10) / 10),
      firstFrame: frames.length ? Math.round(frames[0] - walk.since) : null,
      moves: [...walk.moves.entries()].map(([node, series]) => ({ node, name: series.name, points: series.points.map((one) => ({ t: Math.round(one.t - walk.since), box: one.box })) })),
      shifts: walk.shifts.map((one) => ({ ...one, t: Math.round(one.t - walk.since) })),
      loafs: walk.loafs?.map((one) => ({ ...one, t: Math.round(one.t - walk.since) })) ?? null,
      errors: walk.errors.filter((one) => !one.text.includes(HELD)).map((one) => ({ ...one, t: Math.round(one.t - walk.since) })),
      calls: walk.calls.map((one) => ({ ...one, t: Math.round(one.t - walk.since), ms: Math.round(one.ms) })),
      held: walk.held.map((one) => ({ ...one, t: Math.round(one.t - walk.since) })),
      overlays: walk.overlays(),
      view: document.querySelector(".mode:not([hidden])")?.id ?? "",
    };
  };

  // Gives the page back its bridge, its print and its window.open.
  walk.restore = () => {
    walk.bridge?.watchCalls(null);
    window.print = walk.print;
    window.open = walk.open;
    console.error = written;
    delete window.__walk;
  };
  return "watching";
})();
//# sourceURL=walk-monitor.js
