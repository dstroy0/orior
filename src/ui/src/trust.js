// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Trust: whether the tree's own code runs as orior's tools read the tree (trust.rs). A tree that
// holds code of its own, with no answer kept for it or for a folder above it, is asked about once as
// it opens; the answer holds for the tree, or for every tree in the folder above where the reader
// says so. A tree with no code of its own is never asked about. Until a tree is trusted its code is
// held back, and the status bar says so; a press there, or File, Trust This Tree, asks again. The
// question takes no keys from the editor until it is pressed.

import { invoke } from "./bridge.js";
import { say } from "./statusbar.js";

const state = { told: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const nameOf = (path) => path.replace(/[\\/]+$/, "").split(/[\\/]/).pop();

// The files named, as a list in words.
const listOf = (names) => (names.length > 1 ? `${names.slice(0, -1).join(", ")} and ${names.at(-1)}` : names[0]);

function close() {
  const node = document.getElementById("trust-ask");
  node.hidden = true;
  node.replaceChildren();
}

// Keeps `trusted` for `folder` and every tree under it, and reads the tree's answer again.
async function answer(folder, trusted) {
  close();
  try {
    await invoke("trust_set", { folder, trusted });
  } catch (error) {
    say(String(error), { failed: true });
  }
  await readTrust();
}

// Asks the reader whether the tree, as `told` has it, may run its own code.
export function askTrust(told = state.told) {
  if (!told?.code.length) {
    say("This tree holds no code its tools run as they read it: there is nothing to trust");
    return;
  }
  const name = nameOf(told.tree);
  const trust = element("button", { type: "button", className: "rights-button primary-ish", textContent: "Trust It" });
  trust.addEventListener("click", () => answer(told.tree, true));
  const buttons = [trust];
  if (told.above) {
    const every = element("button", { type: "button", className: "rights-button", textContent: `Trust Every Tree in ${nameOf(told.above)}` });
    every.title = told.above;
    every.addEventListener("click", () => answer(told.above, true));
    buttons.push(every);
  }
  const hold = element("button", { type: "button", className: "rights-button", textContent: "Hold It Back" });
  hold.addEventListener("click", () => answer(told.tree, false));
  buttons.push(hold);
  const node = document.getElementById("trust-ask");
  node.replaceChildren(
    element("p", { className: "rights-said", textContent: `${name} holds code its tools run as they read it: ${listOf(told.code)}. Until it is trusted, orior holds that code back.` }),
    element("div", { className: "rights-buttons" }, ...buttons),
  );
  node.hidden = false;
}

// Reads what is kept for the tree open, shows on the status bar whether its code is held back, and
// asks where it holds code of its own and no answer is kept.
export async function readTrust() {
  const told = await invoke("trust_read").catch(() => null);
  state.told = told;
  const held = Boolean(told?.code.length) && told.answer !== true;
  const status = document.getElementById("status-trust");
  status.hidden = !held;
  status.title = held ? `${listOf(told.code)}: press to trust the tree` : "";
  if (told?.answer === null && told.code.length) {
    askTrust(told);
  } else {
    close();
  }
  return told;
}

export function startTrust() {
  document.getElementById("status-trust").addEventListener("click", () => askTrust());
}
