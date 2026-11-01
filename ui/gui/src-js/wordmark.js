// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The name: orior over the eye, tabula under it. The eye is line art in the text's color and
// follows the scheme, and the name reads as orior to a screen reader. The bar along the top holds
// the eye alone, and an empty view holds the whole name large until orior and the eye go there,
// tabula fading as they go. Either opens orior's repository.

import { invoke } from "./bridge.js";
import { incised } from "./incised.js";
import { motionReduced } from "./motion.js";
import { firstStart } from "./session.js";

// The eye's strokes, each the center line of one line of the drawing, in the drawing's own units.
const STROKES = [
  "M164.8 30.5L164.2 32.8L164.5 65M136.5 42.2L140.5 68M191.5 42.8L188.5 63L188 67L188.2 68.2",
  "M113.2 55.2L113.8 58.2L119.8 74.8M215.5 55.8L208.5 75M231.5 71.2L225.7 82.2L224.5 83.5",
  "M96.8 71.5L103.1 82.8L103.2 84.2M83.8 87.2L89 93.8M244.8 87.2L239.2 93.8M256.2 101.2L252.8 105.2",
  "M72.8 102L75.8 105.5M164.8 106L164.5 107.5",
  "M164.5 107.5L162.4 109.2L141 146.5L140.6 148.2L141.3 149.3L147.8 149.5L149.8 150",
  "M164.5 107.5L166.6 109.2L187.1 144.8L188.5 148.8M164.8 127L164.5 127.8",
  "M164.2 128L162.4 129.8L153.9 144.5L152.9 146.5L152.5 148.8",
  "M164.8 128L166.4 129.5L173.7 142L176.2 146.8L176.8 149M77.8 136.5L76.2 138.1L64.8 145.8L61 147.2",
  "M250.2 136.5L251.5 137.9L262 145L268.5 148.8",
  "M60.8 147L61.9 143.9L65.5 139.5L71 133.5L83.2 121.8L97.2 110.5L104.2 105.7L113 100.6L121 96.4L128 93.4L141.5 89.1L147.5 87.7L160 86L172 86L184 87.6L196.2 90.9L203.8 93.6L210 96.5L217.8 100.6L226 105.7L240.8 116.9L253.4 128.8L267.6 144.5L268.7 147L268.8 148.8",
  "M60.5 147.2L59.2 150.8M152.2 149L150 150M152.5 149L155 149.5L176.5 149.2",
  "M188.2 149L187 149.5L181.2 149.5L179.2 150M188.8 149L189.8 149.2M268.8 149L269 150.8",
  "M75.5 149.2L64.8 150.8L59.5 151M177 149.2L178.5 149.5L179 150M252.5 149.2L263.2 150.8L268.8 151",
  "M149.8 150.2L148.9 153.2L141.2 166.5L140.6 168.5L141.1 169.6L187.9 169.6L188.4 168.5L187.8 166.5L180.2 153.5L179.2 150.2",
  "M59.2 151.2L59.2 152.5L59.7 154L61.8 156.2M269 151.2L269.2 151.8M272.2 151.8L269.5 152",
  "M269.2 152.2L268.8 154M268.5 154.2L263 155.4L251.5 159.8M268.8 154.5L267.9 157.5L265.8 160.2",
  "M62.2 156.2L63.8 155.5L65.2 155.7L75.5 159.4L76.8 159.5M62 156.5L63 159.2L65 161.2",
  "M265.2 160.2L262.5 161.1L253.2 167.7L241 175.2L230 180.8L215.8 186.6L199 191.4L197.5 192.2",
  "M265.5 160.5L264.9 162.8L263.8 164.5L251.8 177.1L240.5 187.1L227.5 196.6L214.5 204.2L202.5 209.7L189 214L182.5 215.1L181 214.8",
  "M65.5 161.2L68.8 162.3L84.8 172.1L96.2 178.1L113.2 185.1L125.2 188.8L128.8 190.2",
  "M65.2 161.5L65.7 163.2L66.9 165.2L78.8 177.2L88.5 185.8L103 196.6L115.5 204L127.8 209.6L140.5 213.7L142.8 214.1L144.5 213.8",
  "M38.8 168.5L43.8 174L52.5 182.6L53.8 185M290 169L277.1 183.2L276 185.5M53.5 185.2L50 189.5",
  "M54 185.2L56.8 186.5L69.2 197.2L70.5 199.2M275.8 185.8L273 187.2L261.8 196.9L260.4 198.2L259.8 199.8",
  "M276 185.8L279.8 189.5M129 190.5L131 190.4L141.5 192.6L144.2 192",
  "M128.8 190.8L129.5 194.5L132.8 200.5L137.2 206L143.2 211L144.8 213.5",
  "M197 192.2L195.2 192.2L184.2 193.8L181 193.2",
  "M197.2 192.5L196.5 196L191.9 203.2L187.5 207.8L181.8 211.9L180 214.5M70.2 199.5L69.2 201.2L64 207.8",
  "M70.8 199.5L72.8 200.1L74.2 201.1L87 210L88.5 211.8",
  "M259.5 200L258 200.3L256.2 201.3L243 210.5L241.2 212.2M259.8 200L265.8 207.8M146 200.8L150 204.8",
  "M180.2 201L176.5 204.8M88.8 212L91.8 213L109 222.5M88.5 212.2L87.5 214.8L80.2 227.2",
  "M241 212.5L238.8 213.2L229 218.6L222.5 221.7L220.8 223.2M241.2 212.5L249.2 227.2",
  "M145 213.8L153.5 215.9L159.2 216.8L170.5 216.5L173.5 216.2L180 214.5",
  "M181.2 214.2L181.5 214L181.5 214.5M180 214.5L181 214.8",
  "M109.2 222.8L112 223.4L126 228.7L133.8 230.8L135.2 231.8M109 223L108.3 225.8L100.2 248.2",
  "M220.5 223.5L217.5 224L205 228.6L197 230.7L194.5 232M220.8 223.8L227.7 243.5L228.9 246.5L229.8 247.5",
  "M135.2 231.8L139 231.9L142 232.6L151.5 234L162.5 234.6L164.2 235.2M135.2 231.8L130.5 263.2",
  "M194.5 232L192.8 231.8L180.8 233.8L166.8 234.8L164.5 235.2M194.5 232L199 263",
  "M164.5 235.5L164.2 276.5L164.8 278.8",
].join("");

// The iris, the drawing's one filled shape.
const IRIS = "M163 209.4L158.8 208.9L155.5 207.9L151.2 205.4L145.4 199.5L142.6 194.8L145.2 192.1L159 193.6L176.2 193.6L180.2 193.1L183.1 196L180.9 199.8L175.2 205.4L173 206.9L167.5 208.9L163 209.4Z";

const BOX = "36.8 28.5 255.2 252.2";

const SVG = "http://www.w3.org/2000/svg";

function eye() {
  const svg = document.createElementNS(SVG, "svg");
  svg.setAttribute("class", "eye-mark");
  svg.setAttribute("viewBox", BOX);
  svg.setAttribute("aria-hidden", "true");
  const lines = document.createElementNS(SVG, "path");
  lines.setAttribute("d", STROKES);
  const iris = document.createElementNS(SVG, "path");
  iris.setAttribute("d", IRIS);
  iris.setAttribute("class", "iris");
  svg.append(lines, iris);
  return svg;
}

// A word of the large name, above or below the eye.
function word(text, className) {
  const span = document.createElement("span");
  span.className = className;
  span.textContent = text;
  return span;
}

// Sets the name in `node`: orior over the eye, tabula under it; or, with `eyeOnly`, the eye alone,
// as the bar holds it.
export function setWordmark(node, { eyeOnly = false } = {}) {
  node.setAttribute("aria-label", "orior");
  node.classList.add("wordmark");
  node.replaceChildren(...(eyeOnly ? [eye()] : [word("orior", "wordmark-name"), eye(), under()]));
  return node;
}

// tabula under the eye, cut in its Roman capitals.
function under() {
  const span = document.createElement("span");
  span.className = "wordmark-under";
  span.append(incised("TABVLA"));
  return span;
}

// How long the name stands on an empty view before it goes to the bar, and how long it takes, in
// milliseconds.
const STANDS = 1400;
const FLIES = 900;

// How long tabula takes to slide up to the name's middle, how long it holds there whole, and how long it
// takes to fade, in milliseconds.
const SLIDES = 540;
const HOLDS = 500;
const FADES = 360;

// The share of the flight over which the funnel forms, before the name starts up it, and the height
// a row keeps at the funnel's tip.
const FORMS = 0.35;
const TIP_ROW = 0.25;

const flights = new Map();
const genies = new Map();

// The names standing or flying to the bar. Where none is, the eye in the bar shows: a name that never
// stood, as where the tabs the window kept come back over the empty view, and a name taken out of
// sight before it reached the bar, as a load cut short leaves it, each leave the eye to show at once.
const going = new Set();

// Whether the large name has shown on this page, gone to the bar or as tabula faded: it shows once a page.
let nameShown = false;

function landed() {
  document.getElementById("bar-mark")?.classList.add("home");
}

// Takes away a genie's canvas.
function settle(node) {
  const genie = genies.get(node);
  if (genie) {
    cancelAnimationFrame(genie.frame);
    genie.canvas.remove();
  }
  genies.delete(node);
}

const smooth = (part) => {
  const clamped = Math.min(1, Math.max(0, part));
  return clamped * clamped * (3 - 2 * clamped);
};

// The share of a funnel side that runs straight, the rest of it the S of `smooth`: a flatter S than
// the whole would make.
const STRAIGHT = 0.35;
const sided = (part) => STRAIGHT * Math.min(1, Math.max(0, part)) + (1 - STRAIGHT) * smooth(part);

// The box round both of `boxes`.
const around = (one, other) => {
  const left = Math.min(one.left, other.left);
  const top = Math.min(one.top, other.top);
  const right = Math.max(one.right, other.right);
  const bottom = Math.max(one.bottom, other.bottom);
  return { left, top, right, bottom, width: right - left, height: bottom - top };
};

// orior and the eye drawn as they stand in `node`, onto a canvas `scale` times `box`, the box round
// them: the word on the baseline the page sets it on, the eye's strokes and iris, each in the name's
// color, and nothing else of the view.
function drawRising(node, word, part, box, scale) {
  const canvas = document.createElement("canvas");
  canvas.width = Math.ceil(box.width * scale);
  canvas.height = Math.ceil(box.height * scale);
  const pen = canvas.getContext("2d");
  pen.scale(scale, scale);
  const style = getComputedStyle(node);
  pen.fillStyle = style.color;
  pen.strokeStyle = style.color;
  const wordStyle = getComputedStyle(word);
  const wordBox = word.getBoundingClientRect();
  pen.font = wordStyle.font;
  pen.letterSpacing = wordStyle.letterSpacing;
  const metrics = pen.measureText(word.textContent);
  const ascent = metrics.fontBoundingBoxAscent;
  const baseline = wordBox.top - box.top + (wordBox.height - ascent - metrics.fontBoundingBoxDescent) / 2 + ascent;
  pen.fillText(word.textContent, wordBox.left - box.left, baseline);
  const eyeBox = part.getBoundingClientRect();
  const [left, top, wide, high] = part.getAttribute("viewBox").split(" ").map(Number);
  const fit = Math.min(eyeBox.width / wide, eyeBox.height / high);
  pen.translate(eyeBox.left - box.left + (eyeBox.width - wide * fit) / 2, eyeBox.top - box.top + (eyeBox.height - high * fit) / 2);
  pen.scale(fit, fit);
  pen.translate(-left, -top);
  const lines = part.querySelector("path:not(.iris)");
  pen.lineWidth = Number.parseFloat(getComputedStyle(lines).strokeWidth) || 2.2;
  pen.lineCap = "round";
  pen.lineJoin = "round";
  pen.stroke(new Path2D(lines.getAttribute("d")));
  pen.fill(new Path2D(part.querySelector(".iris").getAttribute("d")));
  return canvas;
}

// The name large on an empty view: it stands a moment, then orior and its eye go to the bar as a
// genie does. A funnel forms between them and the point on the bar's bottom edge under the eye in the
// bar, its sides bending sideways from the one's ends to the other's as they rise, and they pour up it
// a row of pixels at a time, every row level, each as wide as the funnel where it stands and shorter
// the nearer the tip, until the bar's edge takes the last of them. The funnel is drawn over the whole
// window: no pane or strip it crosses cuts it short. Over the same flight tabula slides up to the
// name's middle, where the eye stood, and fades out. The eye in the bar then shows and stays. The name
// stands and goes once as the app loads; an empty view shown after it has reached the bar holds the
// lattice alone.
function fly(node) {
  const home = document.getElementById("bar-mark");
  window.clearTimeout(flights.get(node));
  waits.get(node)?.disconnect();
  waits.delete(node);
  settle(node);
  node.getAnimations({ subtree: true }).forEach((one) => one.cancel());
  // a flight cut short gives the name back as it stood: on the loading pane where it was lifted off it,
  // orior and the eye shown again
  lifts.get(node)?.();
  node.querySelectorAll(".wordmark-name, .eye-mark").forEach((part) => (part.style.visibility = ""));
  const pane = document.getElementById("loading");
  const own = Boolean(pane?.contains(node));
  // With the eye already in the bar, as on every start after the first in the system's session, the loading
  // pane's name shows tabula alone, faded, once a page; every other name stays hidden.
  if (home.classList.contains("home")) {
    if (own && !nameShown) {
      fadeTabula(node);
      return;
    }
    node.style.visibility = "hidden";
    return;
  }
  // Another name standing or on its way to the bar is the one that goes, and this one stays hidden.
  if (!own && [...going].some((other) => other !== node)) {
    going.delete(node);
    node.style.visibility = "hidden";
    return;
  }
  // The loading pane lies over the view while a tree is read: a view's name stands once the pane has
  // gone, and not under it.
  if (!own && pane && !pane.hidden) {
    going.add(node);
    node.style.visibility = "hidden";
    new MutationObserver((_, watcher) => {
      if (pane.hidden) {
        watcher.disconnect();
        fly(node);
      }
    }).observe(pane, { attributes: true, attributeFilter: ["hidden"] });
    return;
  }
  node.style.visibility = "";
  node.dataset.standing = "";
  going.add(node);
  // The loading pane's own name stands the whole while the tree is read and goes as the pane begins to
  // leave; a view's stands STANDS first.
  if (own) {
    waitOn(node, pane, () => leaving(pane), () => go(node, home));
  } else {
    flights.set(node, window.setTimeout(() => go(node, home), STANDS));
  }
}

const leaving = (pane) => pane.hidden || pane.dataset.leaving === "true";

// The one watch the loading pane's name keeps on the pane: for it to leave while the name stands, and
// for it to be shown again while the name goes.
const waits = new Map();

// Calls `then` once `when` holds of `pane`, or at once where it already does, in place of the watch
// `node` kept before.
function waitOn(node, pane, when, then) {
  waits.get(node)?.disconnect();
  waits.delete(node);
  if (when()) {
    then();
    return;
  }
  const watcher = new MutationObserver(() => {
    if (when()) {
      watcher.disconnect();
      waits.delete(node);
      then();
    }
  });
  watcher.observe(pane, { attributes: true, attributeFilter: ["hidden", "data-leaving"] });
  waits.set(node, watcher);
}

// A name standing on the loading pane lifted off it, into a holder of the same kind over the window at
// the same place, so the pane fading and going under it takes nothing of the name with it: what gives
// the name back to the pane.
function lifted(node) {
  const holder = node.parentElement;
  if (!document.getElementById("loading")?.contains(node)) {
    return () => {};
  }
  const box = holder.getBoundingClientRect();
  const over = holder.cloneNode(false);
  over.style.cssText = `position: fixed; left: ${box.left}px; top: ${box.top}px; width: ${box.width}px; margin: 0; z-index: 49;`;
  over.append(node);
  document.body.append(over);
  const give = () => {
    holder.append(node);
    over.remove();
    lifts.delete(node);
  };
  lifts.set(node, give);
  return give;
}

// What gives each name lifted off the loading pane back to it.
const lifts = new Map();

// tabula alone where the eye stands in `node`, orior and the eye hidden, kept there whole for HOLDS and faded; the
// name cleared and put back as it stood once it has.
function fadeTabula(node) {
  nameShown = true;
  const word = node.querySelector(".wordmark-name");
  const symbol = node.querySelector(".eye-mark");
  const under = node.querySelector(".wordmark-under");
  if (motionReduced()) {
    node.style.visibility = "hidden";
    return;
  }
  const given = lifted(node);
  word.style.visibility = "hidden";
  symbol.style.visibility = "hidden";
  node.style.visibility = "";
  node.dataset.standing = "";
  const middle = (box) => box.top + box.height / 2;
  const there = `translateY(${middle(symbol.getBoundingClientRect()) - middle(under.getBoundingClientRect())}px)`;
  const length = HOLDS + FADES;
  under
    .animate(
      [
        { offset: 0, transform: there, opacity: 1 },
        { offset: HOLDS / length, transform: there, opacity: 1, easing: "ease-in" },
        { offset: 1, transform: there, opacity: 0 },
      ],
      { duration: length, fill: "forwards" },
    )
    .finished.then(() => {
      node.style.visibility = "hidden";
      delete node.dataset.standing;
      word.style.visibility = "";
      symbol.style.visibility = "";
      under.getAnimations().forEach((one) => one.cancel());
      given();
    }, () => {});
}

// The name `node` goes to `home` in the bar as a genie does.
function go(node, home) {
  nameShown = true;
  const pane = document.getElementById("loading");
  // the loading pane shown again before the name has gone stands the name on it again
  if (pane?.contains(node)) {
    waitOn(node, pane, () => !leaving(pane), () => fly(node));
  }
  const given = lifted(node);
  // orior and the eye go up the funnel together; tabula stays below and fades where they stood.
  const word = node.querySelector(".wordmark-name");
  const symbol = node.querySelector(".eye-mark");
  const under = node.querySelector(".wordmark-under");
  const from = around(word.getBoundingClientRect(), symbol.getBoundingClientRect());
  // The mark's place whole, however far it has shrunk: it shrinks toward its own top left, which stays
  // where its place is.
  const shown = home.getBoundingClientRect();
  const to = { left: shown.left, right: shown.left + home.offsetWidth, width: home.offsetWidth };
  const row = home.closest(".bar");
  // orior and the eye landed: the funnel taken away and the eye in the bar shown
  const landing = () => {
    settle(node);
    landed();
  };
  // tabula gone too: the large name cleared and put back as it stood
  const cleared = () => {
    node.style.visibility = "hidden";
    delete node.dataset.standing;
    word.style.visibility = "";
    symbol.style.visibility = "";
    under.getAnimations().forEach((one) => one.cancel());
    given();
    waits.get(node)?.disconnect();
    waits.delete(node);
    going.delete(node);
  };
  if (!from.width || !to.width || !row || motionReduced()) {
    landing();
    cleared();
    return;
  }
  const scale = window.devicePixelRatio || 1;
  const name = drawRising(node, word, symbol, from, scale);
  const middle = (box) => box.top + box.height / 2;
  // tabula slides up to the name's middle as orior and the eye pour up, holds there whole for HOLDS, the
  // window's own name shown on its own, and fades; the large name is cleared once it has
  const rise = `translateY(${middle(symbol.getBoundingClientRect()) - middle(under.getBoundingClientRect())}px)`;
  const length = SLIDES + HOLDS + FADES;
  under
    .animate(
      [
        { offset: 0, transform: "none", opacity: 1, easing: "ease-out" },
        { offset: SLIDES / length, transform: rise, opacity: 1 },
        { offset: (SLIDES + HOLDS) / length, transform: rise, opacity: 1, easing: "ease-in" },
        { offset: 1, transform: rise, opacity: 0 },
      ],
      { duration: length, fill: "forwards" },
    )
    .finished.then(cleared, () => {});
  const width = window.innerWidth;
  const height = window.innerHeight;
  const canvas = Object.assign(document.createElement("canvas"), { className: "genie" });
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  canvas.setAttribute("aria-hidden", "true");
  document.body.append(canvas);
  const pen = canvas.getContext("2d");
  // The window's own pixels, moved down so the funnel's tip, at the foot of the bar's row where the
  // eye stands, is at 0; no row is drawn above it.
  const tipY = shown.top - home.offsetTop + row.offsetHeight;
  pen.setTransform(scale, 0, 0, scale, 0, tipY * scale);
  const left = from.left;
  const right = from.right;
  const tipLeft = to.left;
  const tipRight = to.right;
  const foot = from.bottom - tipY;
  const high = from.height;
  // How far above the name the funnel narrows, and where rows start to shorten toward the tip.
  const reach = Math.max(1, foot);
  const shortens = Math.max(1, foot - high);
  // Where a row that would stand `depth` below the tip stands once rows shorten near the tip.
  const placed = (depth) => (depth >= shortens ? depth - shortens * (1 - TIP_ROW) / 2 : depth * (TIP_ROW + ((1 - TIP_ROW) * depth) / (2 * shortens)));
  const offset = shortens * (1 - TIP_ROW) / 2;
  const began = performance.now();
  const genie = { canvas, frame: 0 };
  genies.set(node, genie);
  word.style.visibility = "hidden";
  symbol.style.visibility = "hidden";
  const draw = (now) => {
    const part = Math.min(1, (now - began) / FLIES);
    const forming = smooth(part / FORMS);
    const rising = Math.max(0, (part - FORMS * 0.8) / (1 - FORMS * 0.8));
    const lift = rising * rising * (foot + high);
    pen.clearRect(0, -tipY, width, height);
    const rows = name.height;
    for (let row = 0; row < rows; row += 1) {
      // The row's depth below the tip before rows shorten, and after.
      const depth = foot - high + (row / scale) - lift + offset;
      if (depth < 0) {
        continue;
      }
      const y = placed(depth);
      const tall = (placed(depth + 1 / scale) - y) || 1 / scale;
      const bend = forming * (1 - sided(y / reach));
      const start = left + (tipLeft - left) * bend;
      const end = right + (tipRight - right) * bend;
      pen.drawImage(name, 0, row, name.width, 1, start, y, end - start, tall + 0.35 / scale);
    }
    if (part < 1) {
      genie.frame = requestAnimationFrame(draw);
    } else {
      landing();
    }
  };
  genie.frame = requestAnimationFrame(draw);
}

// The large name each watched holder holds, found by its holder even while the name is lifted off it.
const holders = new WeakMap();

// What shows a large name is watched, and not the name: gone to the bar, the name stands outside the
// view that clips it, and never reads as shown again.
const seen = new IntersectionObserver((entries) => {
  for (const entry of entries) {
    const node = holders.get(entry.target);
    if (!node) {
      continue;
    }
    if (entry.isIntersecting) {
      fly(node);
    } else if (!document.getElementById("loading")?.contains(entry.target)) {
      // the loading pane's name goes as the pane leaves and lands after it has gone, so its going is
      // the pane's to start and not the view's to cut short
      window.clearTimeout(flights.get(node));
      settle(node);
      if (going.delete(node) && !going.size) {
        landed();
      }
    }
  }
});

// Watches what holds `node`, once it is kept.
function watch(node) {
  requestAnimationFrame(() => {
    const holder = node.parentElement;
    if (holder) {
      holders.set(holder, node);
      seen.observe(holder);
    }
  });
}

// The name large, for an empty view, and set to go to the bar once it shows.
export function wordmark(tag) {
  const node = setWordmark(document.createElement(tag));
  linkHome(node);
  watch(node);
  return node;
}

// The name opens orior's repository in the browser, from a click or from Enter or Space.
const REPOSITORY = "https://github.com/dstroy0/orior";

function linkHome(node) {
  node.setAttribute("role", "link");
  node.tabIndex = 0;
  node.title = REPOSITORY;
  node.classList.add("wordmark-link");
  const go = () => invoke("report_open", { url: REPOSITORY }).catch(() => {});
  node.addEventListener("click", go);
  node.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      go();
    }
  });
}

// Sets the name in the bar and in every heading the page marks for it, each of those set to go to
// the bar once it shows. The name flies once in the system's session, on the first start in it, from the
// loading pane that start alone shows; every start after shows the eye in the bar at once.
export async function startWordmark() {
  linkHome(setWordmark(document.getElementById("bar-mark"), { eyeOnly: true }));
  const names = [...document.querySelectorAll("[data-wordmark]")];
  names.forEach((node) => linkHome(setWordmark(node)));
  if (!(await firstStart())) {
    landed();
  }
  window.setTimeout(() => going.size || landed(), STANDS);
  names.forEach(watch);
}
