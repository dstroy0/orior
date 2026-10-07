// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Popup menus: the one a right click opens, or the menu key, or Shift+F10, over whatever part of the
// app is under it. A part says what its menu holds with menuOn, and the innermost part under the
// pointer answers. A text field no part answers for gets the field's own menu, and anywhere else the
// web view's menu, which offers nothing the app does, stays shut.
//
// A menu is a list of items, each a label, the keys that do the same where there are some, and what
// it does, with "-" for a line between items. An item that cannot act now is drawn but cannot be
// chosen. Up and Down step through the items, Home and End go to the ends, a letter goes to the next
// item it begins, Enter or Space chooses, and Escape or Tab closes. A choice closes the menu and hands
// the keys back to what held them before it opened, then acts. A command for the editor or the
// terminal finds it as it was.

import { invoke } from "./bridge.js";

const parts = [];
const state = { menu: null, back: null, anchor: null };

const TEXT_FIELD = 'input:not([type="button"]):not([type="checkbox"]):not([type="radio"]), textarea:not(.term-keys)';

// Gives a part of the page a menu. `itemsFor` is asked with the contextmenu event and answers with
// the items, or with null where the pointer is on nothing in the part that has a menu.
export function menuOn(part, itemsFor) {
  parts.push({ part, itemsFor });
}

export function closeMenu(refocus = true) {
  if (!state.menu) {
    return;
  }
  state.menu.remove();
  state.menu = null;
  delete state.anchor?.dataset.menu;
  state.anchor = null;
  if (refocus && state.back?.isConnected) {
    state.back.focus();
  }
  state.back = null;
}

function enabled() {
  return [...state.menu.querySelectorAll(".menu-item:not([disabled])")];
}

function step(by) {
  const items = enabled();
  const at = items.indexOf(document.activeElement);
  items[(at + by + items.length) % items.length]?.focus();
}

function choose(item) {
  closeMenu();
  item.run();
}

// Shows a menu with its top left corner at x, y, turned in from the window's edges. The row it was
// opened on, where there is one, stays marked while it is open.
export function showMenu(x, y, items, anchor = null) {
  closeMenu(false);
  state.back = document.activeElement;
  state.anchor = anchor;
  if (anchor) {
    anchor.dataset.menu = "open";
  }
  const menu = document.createElement("div");
  menu.className = "menu";
  menu.setAttribute("role", "menu");
  for (const item of items) {
    if (item === "-") {
      menu.append(Object.assign(document.createElement("div"), { className: "menu-line", role: "separator" }));
      continue;
    }
    const button = Object.assign(document.createElement("button"), { type: "button", className: "menu-item", disabled: Boolean(item.disabled) });
    button.setAttribute("role", "menuitem");
    button.append(Object.assign(document.createElement("span"), { textContent: item.label }));
    if (item.keys) {
      button.append(Object.assign(document.createElement("kbd"), { textContent: item.keys }));
    }
    button.addEventListener("click", () => choose(item));
    button.addEventListener("pointermove", () => button.disabled || button.focus());
    menu.append(button);
  }
  menu.addEventListener("keydown", (event) => {
    const keys = { ArrowDown: () => step(1), ArrowUp: () => step(-1), Home: () => enabled()[0]?.focus(), End: () => enabled().at(-1)?.focus() };
    if (keys[event.key]) {
      keys[event.key]();
    } else if (event.key === "Escape") {
      closeMenu();
    } else if (event.key === "Tab") {
      closeMenu();
    } else if (event.key.length === 1 && /\S/.test(event.key) && !event.ctrlKey && !event.altKey) {
      const items = enabled();
      const at = items.indexOf(document.activeElement);
      const letter = event.key.toLowerCase();
      const ordered = [...items.slice(at + 1), ...items.slice(0, at + 1)];
      ordered.find((one) => one.textContent.toLowerCase().startsWith(letter))?.focus();
    } else {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
  });
  document.body.append(menu);
  state.menu = menu;
  const box = menu.getBoundingClientRect();
  const left = Math.max(4, Math.min(x, window.innerWidth - box.width - 4));
  const top = y + box.height > window.innerHeight - 4 ? Math.max(4, y - box.height) : y;
  menu.style.left = `${left}px`;
  menu.style.top = `${top}px`;
  enabled()[0]?.focus();
}

// Pastes text into a text field where its selection is, as typing it would.
function pasteInto(field, text) {
  field.focus();
  field.setRangeText(text, field.selectionStart, field.selectionEnd, "end");
  field.dispatchEvent(new Event("input", { bubbles: true }));
}

// The menu of a text field: what its own menu would offer.
function fieldItems(field) {
  const chosen = field.selectionStart !== field.selectionEnd;
  const fixed = field.readOnly || field.disabled;
  return [
    { label: "Cut", keys: "Ctrl+X", disabled: !chosen || fixed, run: () => document.execCommand("cut") },
    { label: "Copy", keys: "Ctrl+C", disabled: !chosen, run: () => document.execCommand("copy") },
    { label: "Paste", keys: "Ctrl+V", disabled: fixed, run: async () => pasteInto(field, await clipText()) },
    "-",
    { label: "Select all", keys: "Ctrl+A", run: () => field.select() },
  ];
}

// The text on the clipboard, read by the app so the page never has to ask leave to read it.
export function clipText() {
  return invoke("clip_read").catch(() => "");
}

export function copyText(text) {
  return navigator.clipboard.writeText(text).catch(() => {});
}

export function startMenus() {
  document.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    const target = event.target instanceof Element ? event.target : event.target.parentElement;
    if (state.menu?.contains(target)) {
      return;
    }
    // A menu opened from the keyboard has no pointer to stand at, and stands at the part instead.
    let { clientX: x, clientY: y } = event;
    if (event.button !== 2 || (!x && !y)) {
      const box = target.getBoundingClientRect();
      x = box.left + Math.min(24, box.width / 2);
      y = box.top + Math.min(box.height, 28);
    }
    const holders = parts.filter(({ part }) => part.contains(target)).sort((a, b) => (a.part.contains(b.part) ? 1 : -1));
    for (const { itemsFor } of holders) {
      const items = itemsFor(event);
      if (items) {
        showMenu(x, y, items, target.closest("[data-key], .tab"));
        return;
      }
    }
    const field = target.closest(TEXT_FIELD);
    if (field) {
      field.focus();
      showMenu(x, y, fieldItems(field));
      return;
    }
    closeMenu();
  });
  const outside = (event) => state.menu && !state.menu.contains(event.target) && closeMenu(false);
  document.addEventListener("pointerdown", outside, true);
  document.addEventListener("wheel", outside, true);
  window.addEventListener("blur", () => closeMenu(false));
  window.addEventListener("resize", () => closeMenu(false));
}
