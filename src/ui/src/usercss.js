// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The reader's stylesheet, user.css in orior's own folder, laid over the app's last so that each of
// its rules wins over the app's of the same weight. It is read as the app starts, each time the window
// comes back to the front, and after File, User Stylesheet opens it. A change to it reaches the
// canvases as a change of theme does.

import { invoke } from "./bridge.js";
import { notifyScheme } from "./scheme.js";

let kept = null;

export async function loadUserCss() {
  const text = await invoke("user_css_read").catch(() => "");
  if (text === kept) {
    return;
  }
  kept = text;
  let sheet = document.getElementById("user-css");
  if (!sheet) {
    sheet = Object.assign(document.createElement("style"), { id: "user-css" });
  }
  sheet.textContent = text;
  document.head.append(sheet);
  notifyScheme();
}

// Opens user.css, made first where it is not there, in the program the system opens stylesheets in.
export async function openUserCss() {
  await invoke("home_reveal", { what: "user-css" });
  await loadUserCss();
}

export async function keepUserCss() {
  await loadUserCss();
  window.addEventListener("focus", () => loadUserCss());
}
