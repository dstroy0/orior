// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The two views, run and edit, one shown at a time. The page's body names the one shown, and
// whatever marks it is told each time it changes.
//
// The row under the menu bar holds a tab for each view, apart from the menus. A press on a tab
// shows its view; with a tab holding the keys, Left and Right step to the other and show it, and
// Home and End go to the first and the last. Tab reaches the tab of the view shown, and no other.

const listeners = [];

export function shownView() {
  return document.querySelector('.mode[data-active="true"]')?.id.replace("mode-", "") ?? "run";
}

export function onView(listener) {
  listeners.push(listener);
}

function markTabs(name) {
  document.querySelectorAll(".mode-tab").forEach((tab) => {
    const shown = tab.dataset.view === name;
    tab.setAttribute("aria-selected", String(shown));
    tab.tabIndex = shown ? 0 : -1;
  });
}

export function showView(name) {
  document.body.dataset.view = name;
  document.querySelectorAll(".mode").forEach((section) => (section.dataset.active = String(section.id === `mode-${name}`)));
  markTabs(name);
  listeners.forEach((listener) => listener(name));
}

// Wires the tabs. `keysOf(view)` is the key that shows a view, shown on its tab's hover.
export function startModes(keysOf) {
  const tabs = [...document.querySelectorAll(".mode-tab")];
  for (const tab of tabs) {
    const keys = keysOf(tab.dataset.view);
    tab.title = keys ? `${tab.textContent} (${keys})` : tab.textContent;
    tab.addEventListener("click", () => showView(tab.dataset.view));
    tab.addEventListener("keydown", (event) => {
      const at = tabs.indexOf(tab);
      const to = { ArrowRight: at + 1, ArrowLeft: at - 1, Home: 0, End: tabs.length - 1 }[event.key];
      if (to === undefined) {
        return;
      }
      event.preventDefault();
      const next = tabs[(to + tabs.length) % tabs.length];
      showView(next.dataset.view);
      next.focus();
    });
  }
  markTabs(shownView());
}
