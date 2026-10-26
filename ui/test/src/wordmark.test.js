// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The name: a large one hidden until it stands, so it shows no frame before it does; and as it goes to
// the bar, orior and the eye pouring up the funnel together while tabula slides to the name's middle
// and fades, the bar's eye showing once they land.

import { SHORTEST, hideLoading, killLoading, showLoading } from "../../gui/src-js/loading.js";
import { wordmark } from "../../gui/src-js/wordmark.js";

const { test, assert, ui } = window.__harness;

const STANDS = 1400;
const FLIES = 900;

// A large name made where an empty view holds one, `home` the bar's eye as the name finds it.
function madeName() {
  const holder = Object.assign(document.createElement("div"), { className: "empty-body" });
  holder.style.cssText = "position: fixed; top: 160px; left: 380px; z-index: 70;";
  const node = wordmark("h1");
  holder.append(node);
  document.body.append(holder);
  return { holder, node };
}

test("the loading pane holds the name standing the whole while a tree is read, and as the pane leaves it goes to the bar", async () => {
  const home = document.getElementById("bar-mark");
  const pane = document.getElementById("loading");
  home.classList.remove("home");
  try {
    showLoading();
    await ui.waitFor(() => pane.querySelector(".wordmark")?.hasAttribute("data-standing"), 3000, "the name standing on the loading pane");
    const name = pane.querySelector(".wordmark");
    assert.equal(getComputedStyle(name).visibility, "visible", "the name shown over the loading pane");
    await ui.rest(STANDS + 300);
    assert.ok(name.hasAttribute("data-standing") && !document.querySelector("canvas.genie"), "the name stands as long as the pane shows");
    const under = name.querySelector(".wordmark-under");
    const middle = () => {
      const box = under.getBoundingClientRect();
      return box.top + box.height / 2;
    };
    const standing = middle();
    hideLoading();
    await ui.waitFor(() => document.querySelector("canvas.genie"), SHORTEST + 1000, "the funnel drawn as the pane leaves");
    await ui.rest(FLIES / 2);
    assert.ok(pane.hidden, "the pane gone under the flight");
    assert.ok(under.checkVisibility({ opacityProperty: true, visibilityProperty: true }), "tabula still seen with the pane gone");
    assert.ok(middle() < standing - 4, `tabula sliding up toward the middle: ${standing} to ${middle()}`);
    await ui.waitFor(() => home.classList.contains("home"), FLIES + 1000, "the bar's eye shown once the name lands");
    await ui.waitFor(() => pane.contains(name), 2000, "the name back on the pane once tabula has faded");
  } finally {
    killLoading();
    home.classList.add("home");
  }
});

test("tabula is TABVLA incised in the ground the name stands on, its cuts below it and light on their lower lip, while orior stands on it", () => {
  const { holder, node } = madeName();
  try {
    // A color in any form the page computes, painted and read back as the pixel it makes.
    const lightness = (color) => {
      const pen = Object.assign(document.createElement("canvas"), { width: 1, height: 1 }).getContext("2d");
      pen.fillStyle = color;
      pen.fillRect(0, 0, 1, 1);
      const [red, green, blue] = pen.getImageData(0, 0, 1, 1).data;
      return (Math.max(red, green, blue) + Math.min(red, green, blue)) / 2;
    };
    const word = node.querySelector(".wordmark-under svg.incised");
    assert.equal(word?.dataset.word, "TABVLA", "tabula in Roman capitals, cut");
    const cut = getComputedStyle(word.querySelector("path.cut"));
    const name = getComputedStyle(node.querySelector(".wordmark-name"));
    assert.ok(lightness(cut.fill) < lightness(name.color) / 3, `tabula's cuts below the ground: ${cut.fill} against ${name.color}`);
    const lip = getComputedStyle(word.querySelector("g.cuts")).filter;
    assert.ok(/drop-shadow\(.* 0px 1px 0px\)/.test(lip) && (lip.match(/drop-shadow/g) ?? []).length >= 2, `light on the lower lip and shade on the upper wall: ${lip}`);
    assert.equal(name.textShadow, "none", "orior stands on the ground, not in it");
  } finally {
    holder.remove();
  }
});

test("the loading pane shown again while its name is still going to the bar stands the name on it again", async () => {
  const home = document.getElementById("bar-mark");
  const pane = document.getElementById("loading");
  home.classList.remove("home");
  try {
    showLoading();
    await ui.waitFor(() => pane.querySelector(".wordmark")?.hasAttribute("data-standing"), 3000, "the name standing on the loading pane");
    await hideLoading();
    assert.ok(document.querySelector("canvas.genie") && !pane.querySelector(".wordmark"), "the name lifted off the pane and going to the bar as the pane goes");
    showLoading();
    await ui.waitFor(() => pane.querySelector(".wordmark")?.hasAttribute("data-standing"), 1000, "the name standing on the pane again");
    assert.equal(getComputedStyle(pane.querySelector(".wordmark")).visibility, "visible", "the name shown on the pane again");
    assert.equal(document.querySelectorAll("canvas.genie").length, 0, "the flight cut short");
  } finally {
    killLoading();
    home.classList.add("home");
  }
});

test("a large name made stands hidden until it stands, so no frame shows it before it does", () => {
  const { holder, node } = madeName();
  try {
    assert.equal(getComputedStyle(node).visibility, "hidden", "hidden the moment it is made");
  } finally {
    holder.remove();
  }
});

test("as the name goes to the bar orior and the eye pour up together, tabula slides to the name's middle and fades, and the bar's eye shows once they land", async () => {
  const home = document.getElementById("bar-mark");
  home.classList.remove("home");
  const { holder, node } = madeName();
  try {
    await ui.waitFor(() => node.hasAttribute("data-standing"), 3000, "the name stands");
    const name = node.querySelector(".wordmark-name");
    const eye = node.querySelector(".eye-mark");
    const under = node.querySelector(".wordmark-under");
    const middle = (box) => box.top + box.height / 2;
    const eyeMiddle = middle(eye.getBoundingClientRect());
    const underBefore = middle(under.getBoundingClientRect());
    await ui.waitFor(() => document.querySelector("canvas.genie"), STANDS + 1000, "the funnel drawn");
    await ui.rest(FLIES / 2);
    assert.equal(getComputedStyle(name).visibility, "hidden", "orior gone up the funnel");
    assert.equal(getComputedStyle(eye).visibility, "hidden", "the eye gone up the funnel");
    const underNow = middle(under.getBoundingClientRect());
    assert.ok(underNow < underBefore - 4 && underNow > eyeMiddle - 1, `tabula sliding up toward the middle: ${underBefore} to ${underNow}, the middle at ${eyeMiddle}`);
    await ui.rest(200);
    assert.ok(Math.abs(middle(under.getBoundingClientRect()) - eyeMiddle) < 1.5, `tabula at the middle: ${middle(under.getBoundingClientRect())}, the middle at ${eyeMiddle}`);
    assert.equal(Number(getComputedStyle(under).opacity), 1, "tabula held there whole");
    await ui.waitFor(() => home.classList.contains("home"), FLIES, "the bar's eye shown as orior and the eye land");
    assert.equal(Number(getComputedStyle(under).opacity), 1, "tabula still held whole at the middle as they land, the window's name shown");
    await ui.waitFor(() => Number(getComputedStyle(under).opacity) < 1, 1000, "tabula fading after its hold");
    await ui.waitFor(() => getComputedStyle(node).visibility === "hidden", 1000, "the large name cleared once tabula has faded");
    assert.equal(document.querySelectorAll("canvas.genie").length, 0, "the funnel taken away");
    assert.equal(getComputedStyle(node).visibility, "hidden", "the large name gone");
  } finally {
    holder.remove();
    home.classList.add("home");
  }
});
