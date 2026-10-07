// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Popup menus: the one a right click opens, or the menu key, or Shift+F10, over whatever part of the
// app is under it, and the ones the menu bar opens. A part says what its menu holds with menuOn, and
// the innermost part under the pointer answers. A text field no part answers for gets the field's own
// menu, and anywhere else the web view's menu, which offers nothing the app does, stays shut.
//
// A menu is a list of items, each a label, the keys that do the same where there are some, and what
// it does, with "-" for a line between items. An item with items of its own opens them beside it, as
// the pointer rests on it or as Right, Enter or Space is pressed on it, and Left or Escape closes them
// again. An item that cannot act now is drawn but cannot be chosen. Up and Down step through the
// items, Home and End go to the ends, a letter goes to the next item it begins, Enter or Space
// chooses, and Escape or Tab closes. A choice closes every open menu and hands the keys back to what
// held them before the first opened, then acts. A command for the editor or the terminal finds it as
// it was. A menu too long for the window scrolls.

import { invoke } from "./bridge.js";

const parts = [];
// `stack` holds the open menus, the first one and then each opened from an item of the one before.
// `side` is told of Left and Right in the first menu, which the menu bar uses to step between its
// menus, and a pointer pressed in `keep` leaves the menus to it.
const state = { stack: [], back: null, anchor: null, side: null, keep: null, onClose: null, rest: 0 };

// How long the pointer rests on an item before the items under it open.
const REST = 180;

const TEXT_FIELD = 'input:not([type="button"]):not([type="checkbox"]):not([type="radio"]), textarea:not(.term-keys)';

// Gives a part of the page a menu. `itemsFor` is asked with the contextmenu event and answers with
// the items, or with null where the pointer is on nothing in the part that has a menu.
export function menuOn(part, itemsFor) {
  parts.push({ part, itemsFor });
}

export function menuOpen() {
  return state.stack.length > 0;
}

// Closes the menus from `level` down, the first being level 0.
function closeFrom(level) {
  window.clearTimeout(state.rest);
  for (const menu of state.stack.splice(level)) {
    menu.parentItem?.removeAttribute("aria-expanded");
    menu.remove();
  }
}

export function closeMenu(refocus = true) {
  if (!state.stack.length) {
    return;
  }
  closeFrom(0);
  delete state.anchor?.dataset.menu;
  state.anchor = null;
  state.side = null;
  state.keep = null;
  const onClose = state.onClose;
  state.onClose = null;
  onClose?.();
  if (refocus && state.back?.isConnected) {
    state.back.focus();
  }
  state.back = null;
}

const enabledIn = (menu) => [...menu.querySelectorAll(":scope > .menu-item:not([disabled])")];

function step(menu, by) {
  const items = enabledIn(menu);
  const at = items.indexOf(document.activeElement);
  items[(at + by + items.length) % items.length]?.focus();
}

function choose(item) {
  closeMenu();
  item.run();
}

// Places a menu at x, y, turned in from the window's edges. `beside` is the box of the item it opens
// from, which it moves to the other side of where there is no room to the right.
function place(menu, x, y, beside = null) {
  const box = menu.getBoundingClientRect();
  let left = x;
  if (beside && left + box.width > window.innerWidth - 4) {
    left = beside.left - box.width + 2;
  }
  left = Math.max(4, Math.min(left, window.innerWidth - box.width - 4));
  let top = y;
  if (top + box.height > window.innerHeight - 4) {
    top = beside ? window.innerHeight - 4 - box.height : y - box.height;
  }
  menu.style.left = `${left}px`;
  menu.style.top = `${Math.max(4, top)}px`;
}

function openUnder(button, item, level, focusFirst) {
  if (state.stack[level + 1]?.parentItem === button) {
    if (focusFirst) {
      enabledIn(state.stack[level + 1])[0]?.focus();
    }
    return;
  }
  closeFrom(level + 1);
  const box = button.getBoundingClientRect();
  const menu = build(item.items, level + 1);
  menu.parentItem = button;
  button.setAttribute("aria-expanded", "true");
  place(menu, box.right - 2, box.top - 5, box);
  if (focusFirst) {
    enabledIn(menu)[0]?.focus();
  }
}

function build(items, level) {
  const menu = document.createElement("div");
  menu.className = "menu";
  menu.tabIndex = -1;
  menu.setAttribute("role", "menu");
  for (const item of items) {
    if (item === "-") {
      menu.append(Object.assign(document.createElement("div"), { className: "menu-line", role: "separator" }));
      continue;
    }
    const button = Object.assign(document.createElement("button"), { type: "button", className: "menu-item", disabled: Boolean(item.disabled) });
    button.setAttribute("role", "menuitem");
    button.append(Object.assign(document.createElement("span"), { textContent: item.label }));
    if (item.items) {
      button.setAttribute("aria-haspopup", "menu");
      button.append(Object.assign(document.createElement("i"), { className: "menu-more", ariaHidden: "true" }));
    } else if (item.keys) {
      button.append(Object.assign(document.createElement("kbd"), { textContent: item.keys }));
    }
    button.addEventListener("click", () => (item.items ? openUnder(button, item, level, true) : choose(item)));
    button.addEventListener("pointermove", () => {
      if (button.disabled || document.activeElement === button) {
        return;
      }
      button.focus();
      window.clearTimeout(state.rest);
      if (item.items) {
        state.rest = window.setTimeout(() => openUnder(button, item, level, false), REST);
      } else if (state.stack.length > level + 1) {
        state.rest = window.setTimeout(() => closeFrom(level + 1), REST);
      }
    });
    button.item = item;
    menu.append(button);
  }
  menu.addEventListener("keydown", (event) => {
    const focused = document.activeElement.closest?.(".menu-item");
    const item = focused?.item;
    const moves = { ArrowDown: () => step(menu, 1), ArrowUp: () => step(menu, -1), Home: () => enabledIn(menu)[0]?.focus(), End: () => enabledIn(menu).at(-1)?.focus() };
    if (moves[event.key]) {
      moves[event.key]();
    } else if (event.key === "ArrowRight") {
      if (item?.items) {
        openUnder(focused, item, level, true);
      } else if (state.side) {
        state.side(1);
      }
    } else if (event.key === "ArrowLeft" || (event.key === "Escape" && level > 0)) {
      if (level > 0) {
        const parent = menu.parentItem;
        closeFrom(level);
        parent.focus();
      } else if (event.key === "ArrowLeft" && state.side) {
        state.side(-1);
      }
    } else if (event.key === "Enter" || event.key === " ") {
      if (item?.items) {
        openUnder(focused, item, level, true);
      } else if (item && !focused.disabled) {
        choose(item);
      }
    } else if (event.key === "Escape" || event.key === "Tab") {
      closeMenu();
    } else if (event.key.length === 1 && /\S/.test(event.key) && !event.ctrlKey && !event.altKey) {
      const items = enabledIn(menu);
      const at = items.indexOf(focused);
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
  state.stack[level] = menu;
  return menu;
}

// Shows a menu with its top left corner at x, y, turned in from the window's edges. `anchor` is the
// row it was opened on, which stays marked while it is open; `side`, `keep` and `onClose`, told when
// the menus close, are the menu bar's.
export function showMenu(x, y, items, { anchor = null, side = null, keep = null, onClose = null, focusFirst = true } = {}) {
  const back = state.stack.length ? state.back : document.activeElement;
  closeMenu(false);
  state.back = back;
  state.anchor = anchor;
  state.side = side;
  state.keep = keep;
  state.onClose = onClose;
  if (anchor) {
    anchor.dataset.menu = "open";
  }
  const menu = build(items, 0);
  place(menu, x, y);
  // A menu with nothing to choose still takes the keys, which step to the menus beside it or close it.
  if (focusFirst) {
    (enabledIn(menu)[0] ?? menu).focus();
  }
  return menu;
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

const inMenus = (target) => state.stack.some((menu) => menu.contains(target));

export function startMenus() {
  document.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    const target = event.target instanceof Element ? event.target : event.target.parentElement;
    if (inMenus(target)) {
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
        showMenu(x, y, items, { anchor: target.closest("[data-key], .tab") });
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
  const outside = (event) => {
    if (state.stack.length && !inMenus(event.target) && !state.keep?.contains(event.target)) {
      closeMenu(false);
    }
  };
  document.addEventListener("pointerdown", outside, true);
  document.addEventListener("wheel", outside, true);
  window.addEventListener("blur", () => closeMenu(false));
  window.addEventListener("resize", () => closeMenu(false));
}
