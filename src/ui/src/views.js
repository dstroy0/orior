// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The two views, run and edit, one shown at a time, chosen from the tool strip's Run and Explorer
// icons and from View, Run and View, Edit. The page's body names the one shown, and whatever marks
// it is told each time it changes.

const listeners = [];

export function shownView() {
  return document.querySelector('.mode[data-active="true"]')?.id.replace("mode-", "") ?? "run";
}

export function onView(listener) {
  listeners.push(listener);
}

export function showView(name) {
  document.body.dataset.view = name;
  document.querySelectorAll(".mode").forEach((section) => (section.dataset.active = String(section.id === `mode-${name}`)));
  listeners.forEach((listener) => listener(name));
}
