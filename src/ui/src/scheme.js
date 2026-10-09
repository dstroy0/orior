// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Dark and light. Dark is the scheme the app opens in, a reader's choice is kept for the next time,
// and where the reader follows the system the scheme is the system's, changing as it does. View,
// Light or Dark, Preferences and the command palette choose it.

const KEY = "orior.scheme";
const SYSTEM = "system";
const listeners = [];
const dark = window.matchMedia("(prefers-color-scheme: dark)");

export function scheme() {
  return document.documentElement.dataset.scheme;
}

export function onScheme(listener) {
  listeners.push(listener);
}

// Tells what reads the scheme's colors that they changed, as a theme does without the scheme changing.
export function notifyScheme() {
  listeners.forEach((listener) => listener(scheme()));
}

// Whether the scheme follows the system's.
export function followsSystem() {
  return localStorage.getItem(KEY) === SYSTEM;
}

function show(name) {
  document.documentElement.dataset.scheme = name;
  listeners.forEach((listener) => listener(name));
}

// Chooses a scheme, which stops following the system's.
export function setScheme(name) {
  localStorage.setItem(KEY, name);
  show(name);
}

export function toggleScheme() {
  setScheme(scheme() === "dark" ? "light" : "dark");
}

// Follows the system's scheme, or keeps the one shown as the reader's own.
export function setFollowSystem(on = !followsSystem()) {
  if (on) {
    localStorage.setItem(KEY, SYSTEM);
    show(dark.matches ? "dark" : "light");
  } else {
    setScheme(scheme());
  }
}

export function keepScheme() {
  const kept = localStorage.getItem(KEY);
  show(kept === SYSTEM ? (dark.matches ? "dark" : "light") : kept === "light" ? "light" : "dark");
  dark.addEventListener("change", () => followsSystem() && show(dark.matches ? "dark" : "light"));
}
