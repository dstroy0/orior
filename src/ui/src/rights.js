// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Writing with care. A write the system refuses, a save of the editor's or a plugin's, is not left to
// fail: the window asks the reader whether to write the file with the rights it needs, naming who
// asks, and on Write with Rights the system asks for the rights in its own way and the file is
// written (raised.rs). On Leave It the file stays as it was and the one who asked is told the system
// refused. The question takes no keys from the editor until it is pressed.

import { invoke } from "./bridge.js";

const state = { asking: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// Whether `said`, what a write that failed said, is the system refusing it the rights.
export const refused = (said) => /access is denied|permission denied|operation not permitted|os error (5|13|1)\)|read-only file system/i.test(String(said));

// Asks the reader the question `text`, answering whether they chose to write with the rights.
export function askRights(text) {
  state.asking?.answer(false);
  return new Promise((answer) => {
    const node = document.getElementById("rights-ask");
    const done = (yes) => {
      node.hidden = true;
      node.replaceChildren();
      state.asking = null;
      answer(yes);
    };
    state.asking = { answer: done };
    const write = element("button", { type: "button", className: "rights-button primary-ish", textContent: "Write with Rights" });
    const leave = element("button", { type: "button", className: "rights-button", textContent: "Leave It" });
    write.addEventListener("click", () => done(true));
    leave.addEventListener("click", () => done(false));
    node.replaceChildren(element("p", { className: "rights-said", textContent: text }), element("div", { className: "rights-buttons" }, write, leave));
    node.hidden = false;
  });
}

// Writes `text` to the file of the tree at `path`, and where the system refuses, asks the reader and
// writes it with the rights they give; `who` names who asks for the write. A write the reader leaves
// fails with `refused` set.
export async function writeWithCare(path, text, { who = null } = {}) {
  try {
    await invoke("file_write", { path, text });
    return { raised: false };
  } catch (error) {
    if (!refused(error)) {
      throw error;
    }
    const yes = await askRights(`${who ? `${who} asks to write` : "Saving"} ${path}, and the system refuses it the rights. Write it with the rights it needs?`);
    if (!yes) {
      throw Object.assign(new Error(`the system refused to write ${path}, and the reader left it as it was`), { refused: true });
    }
    await invoke("file_write_raised", { path, text });
    return { raised: true };
  }
}
