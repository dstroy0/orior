// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys in sight. Holding Ctrl or Alt alone for HOLD shows, over each button in sight that has
// keys, the keys that press it: those its aria-keyshortcuts names, or those its title ends with in
// brackets, as "Explorer (Ctrl+Shift+E)". Letting go, any other key, a press, a wheel or the window
// going to the back takes them away. Many people never learn a button's keys unless the keys are shown
// where they already look.

// How long Ctrl or Alt is held alone before the keys show, in milliseconds: a chord pressed at the
// pace of typing shows none.
export const HOLD = 500;

const MODIFIERS = new Set(["Control", "Alt"]);
const KEYS = /\(((?:Ctrl|Alt|Shift|Meta|F\d{1,2})(?:\+[^()]+)?)\)$/;

const state = { timer: 0, shown: [] };

// The keys of `target`, or null where it names none.
export function keysOf(target) {
  return target.getAttribute("aria-keyshortcuts") || target.title?.match(KEYS)?.[1] || null;
}

function inSight(target) {
  const box = target.getBoundingClientRect();
  if (!box.width || !box.height || box.bottom < 0 || box.right < 0 || box.top > innerHeight || box.left > innerWidth) {
    return null;
  }
  const at = document.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2);
  return at && (at === target || target.contains(at)) ? box : null;
}

function show() {
  hide();
  for (const target of document.querySelectorAll("button[aria-keyshortcuts], button[title], [role=button][aria-keyshortcuts]")) {
    const keys = keysOf(target);
    const box = keys && inSight(target);
    if (!box) {
      continue;
    }
    const hint = document.createElement("span");
    hint.className = "key-hint";
    hint.textContent = keys;
    hint.setAttribute("aria-hidden", "true");
    hint.style.left = `${box.left + box.width / 2}px`;
    hint.style.top = `${box.top + box.height / 2}px`;
    document.body.append(hint);
    state.shown.push(hint);
  }
}

export function hide() {
  window.clearTimeout(state.timer);
  state.timer = 0;
  state.shown.forEach((hint) => hint.remove());
  state.shown = [];
}

// The hints shown now, each its keys and the point it stands at, for the tests.
export const hintsShown = () => state.shown.map((hint) => ({ keys: hint.textContent, x: parseFloat(hint.style.left), y: parseFloat(hint.style.top) }));

export function startKeyHints() {
  window.addEventListener(
    "keydown",
    (event) => {
      if (MODIFIERS.has(event.key) && !event.repeat && !event.shiftKey && !event.metaKey) {
        hide();
        state.timer = window.setTimeout(show, HOLD);
      } else if (!MODIFIERS.has(event.key)) {
        hide();
      }
    },
    true,
  );
  window.addEventListener("keyup", (event) => MODIFIERS.has(event.key) && hide(), true);
  for (const kind of ["pointerdown", "wheel", "blur"]) {
    window.addEventListener(kind, hide, true);
  }
}
