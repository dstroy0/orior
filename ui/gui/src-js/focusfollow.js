// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Focus that follows the pointer, set on or off in View and kept. Where it is on, the editor, a shell
// of the terminal, or a list of the explorer, the jobs or the debugger takes the keys once the
// pointer has rested on it for REST: the editor its own, the terminal the shell under the pointer,
// and a list the row under the pointer. The keys stay where they are while a menu, a sheet or Go to
// File is open, while a field is being typed in, and while a button of the pointer is held.

import { menuOpen } from "./menu.js";

const KEY = "orior.focus-follows";

// How long the pointer rests on a part before it takes the keys, in milliseconds.
const REST = 120;

// The lists that take the keys, each by its part of the page.
const LISTS = "#files, #jobs, .pane-body, #debug-frames, #debug-vars, #tests";

export const focusFollows = () => localStorage.getItem(KEY) === "true";

export function setFocusFollows(on = !focusFollows()) {
  localStorage.setItem(KEY, String(on));
}

// The part under the pointer that takes the keys, and what gives them to it.
function partOf(target, hooks) {
  const editor = target.closest(".editor");
  if (editor) {
    return { node: editor, take: () => editor.querySelector(".ed-input")?.focus({ preventScroll: true }) };
  }
  const shell = target.closest("#term .term-view");
  if (shell) {
    return { node: shell, take: () => hooks.terminal(shell) };
  }
  const list = target.closest(LISTS);
  if (list) {
    const row = target.closest("button, [tabindex]:not([tabindex='-1'])");
    return { node: row && list.contains(row) ? row : list, take: () => (row && list.contains(row) ? row : list.querySelector("[aria-current='true'], button"))?.focus({ preventScroll: true }) };
  }
  return null;
}

// Whether the keys stay where they are: a menu, a sheet or Go to File open, or a field being typed in.
function held() {
  if (menuOpen() || document.querySelector("dialog[open]") || document.querySelector(".quick:not([hidden])")) {
    return true;
  }
  const active = document.activeElement;
  return Boolean(active?.matches?.("input:not([type='checkbox']):not([type='radio']), textarea, select") && !active.matches(".ed-input, #term-keys"));
}

// `hooks` gives the keys to the shell whose view is given.
export function startFocusFollows(hooks) {
  let over = null;
  let resting = 0;
  document.addEventListener(
    "pointerover",
    (event) => {
      if (!focusFollows() || event.buttons) {
        return;
      }
      const part = event.target instanceof Element ? partOf(event.target, hooks) : null;
      if (part?.node === over) {
        return;
      }
      over = part?.node ?? null;
      window.clearTimeout(resting);
      if (part) {
        resting = window.setTimeout(() => {
          if (!held() && over === part.node && !part.node.contains(document.activeElement)) {
            part.take();
          }
        }, REST);
      }
    },
    true,
  );
}
