// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Whether the app's animation may run. It stops while the window is without focus or hidden, and
// while its frame is dragged or its edges pulled, and starts again when all three have passed. A
// loop asks `still()` before each frame; while it is still the loop ends, and `whenMoving` starts it
// again once motion comes back. The page's own animations pause with the root's `still` class.
//
// As a drag of the frame starts, each canvas that animates holds still and is then laid over by a
// picture of itself as it stands, and hidden: the page the drag moves holds no canvas at all. The pictures go and the
// canvases come back as the drag ends.

import { listen } from "./bridge.js";

const ANIMATED = "canvas.lattice, canvas.eye, canvas.plasma, canvas.fuse, canvas.ruler";

// The pictures are WebP at QUALITY, which keeps a canvas's clear parts clear and encodes in a fifth
// of the time PNG takes.
const QUALITY = 0.92;

// What a picture takes from its canvas's laid out style, to stand where the canvas stood and draw as
// it drew.
const PLACED = ["position", "top", "right", "bottom", "left", "margin", "transform", "zIndex", "opacity", "filter", "mixBlendMode", "pointerEvents"];

const state = { blurred: !document.hasFocus(), hidden: document.hidden, dragging: false, waiting: new Set(), frozen: [] };

export const still = () => state.blurred || state.hidden || state.dragging;

// Runs `start` once, as soon as motion comes back.
export function whenMoving(start) {
  state.waiting.add(start);
}

function settle() {
  document.documentElement.classList.toggle("still", still());
  if (!still()) {
    const starts = [...state.waiting];
    state.waiting.clear();
    starts.forEach((start) => start());
  }
}

// Lays a picture over each canvas that animates and shows, and hides the canvas.
function freeze() {
  for (const canvas of document.querySelectorAll(ANIMATED)) {
    const box = canvas.getBoundingClientRect();
    if (!box.width || !box.height || canvas.style.display === "none") {
      continue;
    }
    canvas.toBlob((blob) => {
      if (!state.dragging || !blob || !canvas.isConnected) {
        return;
      }
      const url = URL.createObjectURL(blob);
      const picture = Object.assign(new Image(), { src: url, className: canvas.className, alt: "" });
      picture.setAttribute("aria-hidden", "true");
      const laid = getComputedStyle(canvas);
      for (const name of PLACED) {
        picture.style[name] = laid[name];
      }
      Object.assign(picture.style, { display: "block", width: `${box.width}px`, height: `${box.height}px` });
      const shown = canvas.style.display;
      canvas.before(picture);
      canvas.style.display = "none";
      state.frozen.push({ canvas, picture, url, shown });
    }, "image/webp", QUALITY);
  }
}

function thaw() {
  for (const { canvas, picture, url, shown } of state.frozen) {
    canvas.style.display = shown;
    picture.remove();
    URL.revokeObjectURL(url);
  }
  state.frozen = [];
}

export async function startMotion() {
  window.addEventListener("blur", () => {
    state.blurred = true;
    settle();
  });
  window.addEventListener("focus", () => {
    state.blurred = false;
    settle();
  });
  document.addEventListener("visibilitychange", () => {
    state.hidden = document.hidden;
    settle();
  });
  await listen("window-drag", ({ payload }) => {
    const dragging = Boolean(payload);
    if (dragging === state.dragging) {
      return;
    }
    state.dragging = dragging;
    if (dragging) {
      freeze();
    } else {
      thaw();
    }
    settle();
  });
  settle();
}
