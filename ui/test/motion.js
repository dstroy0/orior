// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The motion recorder inside the window, put into the page by `harness.py window motion` (utils/test/harness_motion.py)
// as a script of its own, and read by utils/test/motion_read.py.
//
// From start() to stop(), on every animation frame: its time, the edge revealed, and a value for each channel of each
// part that moves, keyed `part.channel`. The claim's parts are read on every frame, at rest too: each one's box, the
// window its clip shows where a script cuts it and the work's the area its clip encloses, its opacity, its translate
// and scale, and each of the claim's numbers it holds. Any other element a finite animation runs on is read from the frame it
// is first seen on, by the name a person can find it by, its box, opacity and the custom property its transition runs.
// Beside the frames: the marks the driver names each step with, and each long animation frame with its scripts.

(() => {
  if (window.__motion) {
    return "kept";
  }

  const PARTS = {
    bar: "header.bar",
    toolbar: "#toolbar",
    tabs: '#mode-edit .desk[data-tabs="row"] > .tabs',
    strip: "#strip",
    statusbar: "#statusbar",
    controls: "#window-controls",
    minimap: "#mode-edit .editor > .ed-mini",
    work: ".work",
    frame: ".frame",
    mark: "#bar-mark",
    editor: "#mode-edit .editor",
  };
  const NUMBERS = ["--claim", "--reveal-top", "--reveal-bottom", "--reveal-left", "--reveal-down", "--room-top", "--room-left", "--room-bottom", "--tabs-away", "--map-away", "--claim-gap"];
  // The most other elements followed, so a page of animations cannot fill the recording.
  const OTHERS = 60;

  const motion = { frames: [], marks: [], late: [], running: false, others: new Map(), lateRead: true };
  window.__motion = motion;

  const describe = (element) => {
    const classes = typeof element.className === "string" ? element.className.trim().split(/\s+/).filter(Boolean).slice(0, 3) : [];
    return `${element.tagName.toLowerCase()}${element.id ? `#${element.id}` : ""}${classes.map((one) => `.${one}`).join("")}`;
  };
  const pixels = (value) => Math.round(value * 1000) / 1000;
  const fraction = (value) => Math.round(value * 1e6) / 1e6;

  // A clip's path as figures, each the points its commands reach: M, L, H, V and A, as slat.js and bend.js write
  // them, an arc taken by its end.
  const figuresOf = (clip) => {
    const path = /^path\("(.*)"\)$/.exec(clip ?? "")?.[1];
    if (!path) {
      return [];
    }
    const figures = [];
    let points = [];
    let x = 0;
    let y = 0;
    for (const [, command, list] of path.matchAll(/([MLHVAZ])([^MLHVAZ]*)/gi)) {
      const numbers = (list.match(/-?\d*\.?\d+(?:e-?\d+)?/gi) ?? []).map(Number);
      const upper = command.toUpperCase();
      if (upper === "M") {
        if (points.length) {
          figures.push(points);
        }
        [x, y] = numbers;
        points = [[x, y]];
      } else if (upper === "L") {
        [x, y] = numbers;
        points.push([x, y]);
      } else if (upper === "H") {
        x = numbers[0];
        points.push([x, y]);
      } else if (upper === "V") {
        y = numbers[0];
        points.push([x, y]);
      } else if (upper === "A") {
        x = numbers[5];
        y = numbers[6];
        points.push([x, y]);
      } else if (upper === "Z" && points.length) {
        figures.push(points);
        points = [];
      }
    }
    if (points.length) {
      figures.push(points);
    }
    return figures;
  };
  const areaOf = (points) => Math.abs(points.reduce((sum, [x, y], at) => sum + x * points[(at + 1) % points.length][1] - points[(at + 1) % points.length][0] * y, 0)) / 2;

  // Each channel of one element into `values`, under `name`.
  const read = (element, name, values, numbers) => {
    const box = element.getBoundingClientRect();
    values[`${name}.left`] = pixels(box.left);
    values[`${name}.top`] = pixels(box.top);
    values[`${name}.right`] = pixels(box.right);
    values[`${name}.bottom`] = pixels(box.bottom);
    const style = getComputedStyle(element);
    values[`${name}.opacity`] = fraction(Number.parseFloat(style.opacity));
    const matrix = style.transform && style.transform !== "none" ? new DOMMatrixReadOnly(style.transform) : null;
    if (matrix) {
      values[`${name}.tx`] = pixels(matrix.m41);
      values[`${name}.ty`] = pixels(matrix.m42);
      values[`${name}.scale`] = fraction(Math.hypot(matrix.m11, matrix.m12));
    }
    // The work's outline gives way round each hole as one figure, whose box jumps as a hole reaches an edge: the
    // work is read by the area it encloses. Every other part's clip is a slat, its first figure the window it shows.
    const figures = figuresOf(element.style.clipPath);
    if (figures.length && name === "work") {
      values[`${name}.area`] = pixels(figures.reduce((sum, points) => sum + areaOf(points), 0));
    } else if (figures.length) {
      const xs = figures[0].map(([x]) => x);
      const ys = figures[0].map(([, y]) => y);
      values[`${name}.clip.left`] = pixels(box.left + Math.min(...xs));
      values[`${name}.clip.top`] = pixels(box.top + Math.min(...ys));
      values[`${name}.clip.right`] = pixels(box.left + Math.max(...xs));
      values[`${name}.clip.bottom`] = pixels(box.top + Math.max(...ys));
    }
    for (const number of numbers) {
      const said = style.getPropertyValue(number).trim();
      if (said) {
        values[`${name}.${number}`] = fraction(Number.parseFloat(said));
      }
    }
  };

  const sample = (now) => {
    if (!motion.running) {
      return;
    }
    const values = {};
    for (const [name, selector] of Object.entries(PARTS)) {
      const element = document.querySelector(selector);
      if (element?.isConnected && !element.hidden) {
        read(element, name, values, NUMBERS);
      }
    }
    for (const animation of document.getAnimations()) {
      const target = animation.effect?.target;
      if (animation.playState !== "running" || animation.effect?.getComputedTiming?.().iterations === Infinity || !(target instanceof Element)) {
        continue;
      }
      if (Object.values(PARTS).some((selector) => target.matches(selector))) {
        continue;
      }
      const followed = motion.others.get(target);
      const property = animation.transitionProperty?.startsWith("--") ? animation.transitionProperty : "";
      if (followed) {
        if (property && !followed.numbers.includes(property)) {
          followed.numbers.push(property);
        }
      } else if (motion.others.size < OTHERS) {
        motion.others.set(target, { name: describe(target), numbers: property ? [property] : [] });
      }
    }
    for (const [element, { name, numbers }] of motion.others) {
      if (element.isConnected) {
        read(element, name, values, numbers);
      }
    }
    motion.frames.push({ t: now, reveal: document.body.dataset.reveal ?? "", values });
    requestAnimationFrame(sample);
  };

  try {
    new PerformanceObserver((list) => {
      if (!motion.running) {
        return;
      }
      for (const entry of list.getEntries()) {
        motion.late.push({
          t: entry.startTime,
          ms: entry.duration,
          blocking: entry.blockingDuration,
          scripts: (entry.scripts ?? []).map((one) => ({ at: `${(one.sourceURL || "").split("/").pop()}:${one.sourceCharPosition}`, fn: one.sourceFunctionName, invoker: one.invoker, ms: Math.round(one.duration) })),
        });
      }
    }).observe({ type: "long-animation-frame" });
  } catch {
    motion.lateRead = false;
  }

  motion.start = () => {
    motion.frames = [];
    motion.marks = [];
    motion.late = [];
    motion.others = new Map();
    motion.running = true;
    requestAnimationFrame(sample);
    return true;
  };
  motion.mark = (label) => {
    motion.marks.push({ t: performance.now(), label });
    return true;
  };
  motion.stop = () => {
    motion.running = false;
    return { frames: motion.frames, marks: motion.marks, late: motion.late, lateRead: motion.lateRead, size: { width: innerWidth, height: innerHeight, scale: devicePixelRatio } };
  };
  return "made";
})();
