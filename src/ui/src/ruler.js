// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The time ruler under the fuse: the run laid out by when each thing happened, read off the
// milliseconds the runner stamps on every line and on the end.
//
// At rest the ruler fits the whole run, and while the run goes it grows to hold it, the latest
// moment HEAD of the way across. The wheel over
// the fuse or the ruler zooms the time about the pointer, as far in as LEAST milliseconds across the
// whole width, and zooming out past the whole run fits it again. With the ruler holding the keys, +
// and - zoom, 0 fits, Left and Right move it a tenth of what it shows, and Home and End go to the
// start and to the latest. A drag moves it, and a double click fits it. A ruler zoomed in on the
// latest part of a run that is still going keeps up with it; moved back, it stays where it was put.
//
// Along the top, each step's start in signal, and a bar for each pixel's worth of output, as tall as
// the log of its lines and in the fail color where any of them went to stderr. Under the line, the
// marks of the scale, labeled at least MARK_GAP pixels apart. The run's latest moment is marked in
// the hover color while it goes, and its end in signal or in the fail color. The pointer over the
// ruler shows the time under it, in place of any label it would cover.

const LEAST = 5;
const ZOOM = 1.5;
const HEAD = 0.92;
const MARK_GAP = 84;
// The output bars stand in the top BARS pixels, the line of the scale sits at BASE, and the labels
// stand on LABEL.
const BARS = 9;
const BASE = 11;
const LABEL = 24;

// The spans a labeled mark stands for, in milliseconds: 1, 2 and 5 of each power of ten below a
// second, then whole seconds, minutes and hours.
const NICE = [
  1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 30000, 60000, 120000, 300000, 600000, 900000, 1800000, 3600000,
  7200000, 10800000, 21600000, 43200000, 86400000,
];

const two = (n) => String(n).padStart(2, "0");

// `ms` as a mark `step` apart from the next labels it.
function markLabel(ms, step) {
  if (step >= 1000) {
    const whole = Math.round(ms / 1000);
    if (whole < 60) {
      return `${whole} s`;
    }
    const hours = Math.floor(whole / 3600);
    const minutes = Math.floor((whole % 3600) / 60);
    return hours ? `${hours}:${two(minutes)}:${two(whole % 60)}` : `${minutes}:${two(whole % 60)}`;
  }
  if (ms < 1000) {
    return `${Math.round(ms)} ms`;
  }
  return `${(ms / 1000).toFixed(step >= 100 ? 1 : step >= 10 ? 2 : 3)} s`;
}

// `ms` under the pointer, to the places a view `span` milliseconds wide tells apart.
function pointLabel(ms, span) {
  if (span < 2000 && ms < 1000) {
    return `${ms.toFixed(span < 50 ? 2 : span < 500 ? 1 : 0)} ms`;
  }
  const places = span < 50 ? 4 : span < 10000 ? 3 : span < 100000 ? 2 : 1;
  const seconds = ms / 1000;
  if (seconds < 60) {
    return `${seconds.toFixed(places)} s`;
  }
  const minutes = Math.floor(seconds / 60);
  const rest = (seconds - minutes * 60).toFixed(places).padStart(places + 3, "0");
  return minutes >= 60 ? `${Math.floor(minutes / 60)}:${two(minutes % 60)}:${rest}` : `${minutes}:${rest}`;
}

// Makes the ruler, under `fuse`: the element holding both, and what points it at a run.
//
// A run here is the run view's record of it: `lines`, each with its `ms` and `stream`; `steps`, the
// milliseconds each step started at; `done` and how it ended; `endMs` once it has; and `clock`, the
// page's time when the run started, which places the latest moment of a run still going.
export function makeRuler(fuse) {
  const canvas = Object.assign(document.createElement("canvas"), { className: "ruler" });
  const element = Object.assign(document.createElement("div"), {
    className: "fuse-box",
    tabIndex: 0,
    title: "The wheel or + and - zoom the time, a drag or Left and Right move it, and 0 fits the run",
  });
  element.setAttribute("role", "group");
  element.ariaLabel = "Run time";
  element.append(fuse, canvas);
  const view = { run: null, from: 0, span: null, pinned: true, drag: null, hover: null, frame: 0 };

  const latest = () => {
    const { run } = view;
    if (!run) {
      return 0;
    }
    if (run.done) {
      return run.endMs ?? run.lines[run.lines.length - 1]?.ms ?? 0;
    }
    return Math.max(0, performance.now() - run.clock);
  };
  const whole = () => Math.max(view.run?.done ? latest() : latest() / HEAD, LEAST);
  const width = () => canvas.getBoundingClientRect().width || 1;

  // Puts `from` where the run holds it, and says whether the view reaches the latest moment.
  const settle = () => {
    if (view.span === null) {
      return;
    }
    const end = latest();
    if (view.pinned && !view.run?.done) {
      view.from = end - view.span * HEAD;
    }
    view.from = Math.max(0, Math.min(view.from, Math.max(0, whole() - view.span)));
    view.pinned = view.from + view.span * HEAD >= end - view.span * 0.02;
  };

  // What the ruler shows: where it starts and how many milliseconds across.
  const shown = () => {
    if (view.span === null) {
      return [0, whole()];
    }
    settle();
    return [view.from, view.span];
  };

  const fit = () => {
    Object.assign(view, { span: null, from: 0, pinned: true });
    wake();
  };

  // Zooms by `by` about the point `at` of the way across.
  const zoom = (by, at) => {
    const [from, span] = shown();
    const next = Math.max(LEAST, span * by);
    if (next >= whole()) {
      fit();
      return;
    }
    const under = from + at * span;
    view.span = next;
    view.from = under - at * next;
    view.pinned = false;
    settle();
    wake();
  };

  // Moves the view `by` milliseconds later.
  const move = (by) => {
    if (view.span === null) {
      return;
    }
    view.from += by;
    view.pinned = false;
    settle();
    wake();
  };

  element.addEventListener(
    "wheel",
    (event) => {
      if (!view.run) {
        return;
      }
      event.preventDefault();
      const per = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? width() : 1;
      const across = Math.abs(event.deltaX) > Math.abs(event.deltaY) ? event.deltaX : event.shiftKey ? event.deltaY : 0;
      if (across) {
        move(((across * per) / width()) * shown()[1]);
        return;
      }
      const box = canvas.getBoundingClientRect();
      zoom(Math.exp(event.deltaY * per * 0.0015), Math.max(0, Math.min(1, (event.clientX - box.left) / box.width)));
    },
    { passive: false }
  );
  element.addEventListener("keydown", (event) => {
    if (!view.run || event.ctrlKey || event.altKey || event.metaKey) {
      return;
    }
    const at = view.pinned && !view.run.done ? 1 : 0.5;
    const span = shown()[1];
    const acts = {
      "+": () => zoom(1 / ZOOM, at),
      "=": () => zoom(1 / ZOOM, at),
      "-": () => zoom(ZOOM, at),
      _: () => zoom(ZOOM, at),
      0: fit,
      ArrowLeft: () => move(-span / 10),
      ArrowRight: () => move(span / 10),
      Home: () => move(-Infinity),
      End: () => {
        view.pinned = true;
        move(Infinity);
      },
    };
    const act = acts[event.key];
    if (act) {
      event.preventDefault();
      event.stopPropagation();
      act();
    }
  });
  element.addEventListener("pointerdown", (event) => {
    if (event.button !== 0 || view.span === null) {
      return;
    }
    view.drag = { x: event.clientX, from: view.from };
    element.setPointerCapture(event.pointerId);
    element.classList.add("dragging");
  });
  element.addEventListener("pointermove", (event) => {
    const box = canvas.getBoundingClientRect();
    view.hover = event.clientX - box.left;
    if (view.drag) {
      view.from = view.drag.from - ((event.clientX - view.drag.x) / box.width) * view.span;
      view.pinned = false;
      settle();
    }
    wake();
  });
  const drop = () => {
    view.drag = null;
    element.classList.remove("dragging");
  };
  element.addEventListener("pointerup", drop);
  element.addEventListener("pointercancel", drop);
  element.addEventListener("pointerleave", () => {
    view.hover = null;
    wake();
  });
  element.addEventListener("dblclick", fit);

  const paint = () => {
    const box = canvas.getBoundingClientRect();
    if (!box.width) {
      return false;
    }
    const scale = window.devicePixelRatio || 1;
    const wide = Math.round(box.width * scale);
    const high = Math.round(box.height * scale);
    if (canvas.width !== wide || canvas.height !== high) {
      canvas.width = wide;
      canvas.height = high;
    }
    const pen = canvas.getContext("2d");
    pen.setTransform(scale, 0, 0, scale, 0, 0);
    pen.clearRect(0, 0, box.width, box.height);
    const { run } = view;
    if (!run) {
      return false;
    }
    const css = getComputedStyle(document.documentElement);
    const color = (name) => css.getPropertyValue(name).trim();
    const [from, span] = shown();
    const per = box.width / span;
    const xOf = (ms) => (ms - from) * per;

    pen.fillStyle = color("--line");
    pen.fillRect(0, BASE, box.width, 1);

    const columns = Math.ceil(box.width);
    const counts = new Uint32Array(columns);
    const failed = new Uint8Array(columns);
    for (const line of run.lines) {
      if (line.stream === "command" || line.ms === undefined) {
        continue;
      }
      const column = Math.floor(xOf(line.ms));
      if (column >= 0 && column < columns) {
        counts[column] += 1;
        failed[column] ||= line.stream === "stderr" ? 1 : 0;
      }
    }
    const quiet = color("--fg-3");
    const loudest = color("--fg-2");
    const loud = color("--fail");
    for (let column = 0; column < columns; column += 1) {
      if (counts[column]) {
        const tall = Math.min(BARS, 2 + Math.log2(counts[column]) * 2);
        pen.fillStyle = failed[column] ? loud : loudest;
        pen.fillRect(column, BASE - tall, 1, tall);
      }
    }

    pen.font = `10.5px ${color("--head")}`;
    pen.textBaseline = "alphabetic";
    const hovered = view.hover !== null && view.hover >= 0 && view.hover <= box.width;
    const said = hovered ? pointLabel(from + (view.hover / box.width) * span, span) : "";
    const saidWide = pen.measureText(said).width + 8;
    const saidLeft = view.hover + saidWide + 4 > box.width ? view.hover - saidWide - 2 : view.hover + 2;

    const step = NICE.find((one) => one * per >= MARK_GAP) ?? NICE[NICE.length - 1];
    const minor = step / (String(step)[0] === "2" ? 4 : 5);
    for (let at = Math.ceil(from / minor); at * minor <= from + span; at += 1) {
      const ms = at * minor;
      const x = Math.round(xOf(ms));
      const major = Math.abs(ms / step - Math.round(ms / step)) < 1e-6;
      pen.fillStyle = quiet;
      pen.fillRect(x, BASE + 1, 1, major ? 5 : 2);
      const label = major ? markLabel(ms, step) : "";
      if (label && !(hovered && x + 3 < saidLeft + saidWide && x + 3 + pen.measureText(label).width > saidLeft)) {
        pen.fillStyle = color("--fg-2");
        pen.fillText(label, x + 3, LABEL);
      }
    }

    pen.fillStyle = color("--signal");
    for (const ms of run.steps) {
      const x = xOf(ms);
      if (x >= -2 && x <= box.width + 2) {
        pen.fillRect(Math.round(x), 0, 2, BASE + 6);
      }
    }

    const end = xOf(latest());
    if (end >= -2 && end <= box.width + 2) {
      pen.fillStyle = !run.done ? color("--hover") : run.code === 0 && !run.stopped ? color("--signal") : loud;
      pen.fillRect(Math.min(box.width - 2, Math.round(end)), 0, 2, BASE + 6);
    }

    if (hovered) {
      pen.fillStyle = color("--fg-2");
      pen.fillRect(Math.round(view.hover), 0, 1, box.height);
      pen.fillStyle = color("--fg");
      pen.fillText(said, saidLeft + 4, LABEL);
    }
    return !run.done;
  };

  const tick = () => {
    view.frame = paint() ? requestAnimationFrame(tick) : 0;
  };

  // Draws the ruler at the next frame, and goes on drawing it while its run goes.
  function wake() {
    if (!view.frame) {
      view.frame = requestAnimationFrame(tick);
    }
  }

  return {
    element,
    // Points the ruler at a run, or at nothing. A new run starts fitted.
    follow(run) {
      if (run !== view.run) {
        Object.assign(view, { run, span: null, from: 0, pinned: true, drag: null });
      }
      wake();
    },
    // Draws again, as each line the run writes asks.
    wake,
    // How far across the run's latest moment falls, 0 at the left and 1 at the right, or null with
    // no run.
    headAt() {
      if (!view.run) {
        return null;
      }
      const [from, span] = shown();
      return (latest() - from) / span;
    },
  };
}
