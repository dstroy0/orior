// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Dark and light, as the docs site offers them. Dark is the scheme the app opens in, and a reader's
// choice is kept for the next time.

const KEY = "orior.scheme";
const listeners = [];

export function scheme() {
  return document.documentElement.dataset.scheme;
}

export function onScheme(listener) {
  listeners.push(listener);
}

function set(name) {
  document.documentElement.dataset.scheme = name;
  const button = document.getElementById("scheme");
  button.textContent = name === "dark" ? "☀" : "☾";
  button.setAttribute("aria-label", name === "dark" ? "Light" : "Dark");
  localStorage.setItem(KEY, name);
  listeners.forEach((listener) => listener(name));
}

export function setScheme(name) {
  set(name);
}

export function toggleScheme() {
  set(scheme() === "dark" ? "light" : "dark");
}

export function keepScheme() {
  set(localStorage.getItem(KEY) === "light" ? "light" : "dark");
  document.getElementById("scheme").addEventListener("click", toggleScheme);
}
