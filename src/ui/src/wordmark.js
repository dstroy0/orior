// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The name: "or", the eye in place of the i, "or". The eye is line art in the text's color and
// follows the scheme, and the name still reads as orior to a screen reader. The bar along the top
// holds it small, and an empty view holds it large until it goes there.

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

// Sets the name in `node`: or, the eye, or.
export function setWordmark(node) {
  node.setAttribute("aria-label", "orior");
  node.classList.add("wordmark");
  node.replaceChildren("or", eye(), "or");
  return node;
}

// How long the name stands on an empty view before it goes to the bar, and how long it takes, in
// milliseconds.
const STANDS = 1400;
const FLIES = 700;

const flights = new Map();

// The name large on an empty view: it stands a moment, then slides left and up into its place in the
// bar along the top, where it stays. Each time the view shows again it stands and goes again.
function fly(node) {
  const home = document.getElementById("bar-mark");
  window.clearTimeout(flights.get(node));
  node.getAnimations().forEach((one) => one.cancel());
  node.style.visibility = "";
  flights.set(
    node,
    window.setTimeout(() => {
      const from = node.getBoundingClientRect();
      const to = home.getBoundingClientRect();
      const done = () => {
        node.style.visibility = "hidden";
        home.classList.add("home");
      };
      if (!from.width || !to.width || matchMedia("(prefers-reduced-motion: reduce)").matches) {
        done();
        return;
      }
      const scale = to.width / from.width;
      const dx = to.left + to.width / 2 - (from.left + from.width / 2);
      const dy = to.top + to.height / 2 - (from.top + from.height / 2);
      const flight = node.animate([{ transform: "none" }, { transform: `translate(${dx}px, ${dy}px) scale(${scale})` }], {
        duration: FLIES,
        easing: "cubic-bezier(0.5, 0, 0.25, 1)",
        fill: "forwards",
      });
      flight.onfinish = done;
    }, STANDS)
  );
}

// What shows a large name is watched, and not the name: gone to the bar, the name stands outside the
// view that clips it, and never reads as shown again.
const seen = new IntersectionObserver((entries) => {
  for (const entry of entries) {
    const node = entry.target.querySelector(":scope > .wordmark");
    if (!node) {
      continue;
    }
    if (entry.isIntersecting) {
      fly(node);
    } else {
      window.clearTimeout(flights.get(node));
    }
  }
});

// Watches what holds , once it is held.
function watch(node) {
  requestAnimationFrame(() => node.parentElement && seen.observe(node.parentElement));
}

// The name large, for an empty view, and set to go to the bar once it shows.
export function wordmark(tag) {
  const node = setWordmark(document.createElement(tag));
  watch(node);
  return node;
}

// Sets the name in the bar and in every heading the page marks for it, each of those set to go to
// the bar once it shows.
export function startWordmark() {
  setWordmark(document.getElementById("bar-mark"));
  document.querySelectorAll("[data-wordmark]").forEach((node) => {
    setWordmark(node);
    watch(node);
  });
}
