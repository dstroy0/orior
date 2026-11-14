// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Parameter Info: the signature of the call the cursor is in, the parameter it is at in bold.

import { asked, invoke } from "../bridge.js";
import { along, sendChanges } from "../edit/mirror.js";
import { escapeHtml } from "../editor/view.js";
import { markup } from "../editor/widgets.js";
import { flush } from "../servers.js";
import { say } from "../statusbar.js";
import { element, here, state } from "./state.js";

// How long the cursor rests before the signature it is in is asked for again, in milliseconds.
export const SIGNATURE_REST = 120;

function signatureBox(editor) {
  if (!state.signature) {
    const box = element("div", { className: "ed-signature", hidden: true });
    box.addEventListener("mousedown", (event) => event.preventDefault());
    editor.host.append(box);
    state.signature = box;
  }
  return state.signature;
}

export function closeSignature() {
  window.clearTimeout(state.signatureWait);
  state.signatureAsked += 1;
  if (state.signature) {
    state.signature.hidden = true;
  }
}

export function signatureOpen() {
  return Boolean(state.signature && !state.signature.hidden);
}

// The tree's signature for the call the cursor is in, worked out on the window's side off the lines of the text it holds
// down to the cursor's line, once the changes written before are: the call's name, its arguments with their types, and
// every declaration of the name in the tree in order of fit; null where the cursor is in no call or the text mirrors
// no branch.
function treeSignature(found) {
  const { s, head } = found;
  const mirrored = s.doc.mirrored;
  if (!mirrored?.ready) {
    return Promise.resolve(null);
  }
  sendChanges(s.doc);
  return along(mirrored, () => invoke("branch_signature", { key: mirrored.key, path: found.tab.file, line: head.line, col: head.col })).catch(() => null);
}

// The tree's answer as the box shows it: the first declaration's label with the active parameter marked and each
// parameter whose argument's type differs marked with the argument, where each declaration is below it.
function fromTree(answer) {
  const [first, ...others] = answer.declarations;
  const active = first.params.length ? Math.min(answer.active, first.params.length - 1) : -1;
  const differs = first.differs.map((at) => [at, `${answer.arguments[at]?.text ?? ""}: ${answer.arguments[at]?.type ?? ""}`]);
  const where = [`\`${first.path}:${first.line + 1}\``, ...others.map((one) => `\`${one.path}:${one.line + 1}\` ${one.label}`)];
  return { label: first.label, params: first.params, active, differs, doc: where.join("\n\n") };
}

// a signature's label as HTML, its active parameter in bold and each parameter that differs from its argument marked
function labelShown(signature) {
  const label = signature.label;
  const differs = new Map(signature.differs ?? []);
  let displayed = "";
  let from = 0;
  signature.params.forEach(([start, end], at) => {
    if (end <= start || start < from) {
      return;
    }
    let part = escapeHtml(label.slice(start, end));
    part = at === signature.active ? `<b>${part}</b>` : part;
    part = differs.has(at) ? `<span class="ed-signature-differs" title="${escapeHtml(differs.get(at))}">${part}</span>` : part;
    displayed += escapeHtml(label.slice(from, start)) + part;
    from = end;
  });
  return displayed + escapeHtml(label.slice(from));
}

// whether two signatures name the same parameters in the same order, their white space aside
function sameParameters(one, other) {
  const texts = (signature) => signature.params.map(([from, to]) => signature.label.slice(from, to).replace(/\s+/g, " ").trim());
  const left = texts(one);
  const right = texts(other);
  return left.length === right.length && left.every((text, at) => text === right[at]);
}

// the signature displayed at the cursor, its active parameter in bold, the server's `check` below it where one is given,
// the box placed above the line or below it
function signatureShown(editor, signature, check = null) {
  const box = signatureBox(editor);
  const checked = check ? `<pre class="ed-signature-check"><code>${labelShown(check)}</code></pre>` : "";
  box.innerHTML = `<pre><code>${labelShown(signature)}</code></pre>${checked}${signature.doc ? markup(signature.doc) : ""}`;
  box.hidden = false;
  const host = editor.host.getBoundingClientRect();
  const rect = editor.rectOf(editor.head());
  const above = rect.top - host.top - box.offsetHeight - 4;
  box.style.top = `${above >= 0 ? above : rect.bottom - host.top + 4}px`;
  box.style.left = `${Math.max(4, Math.min(rect.left - host.left - 12, host.width - box.offsetWidth - 12))}px`;
}

// Shows the signature at the cursor, or closes it where the cursor is in no call. The tree answers first and at
// once, from the declarations it holds; where a server holds the file its answer comes later as the check, positioned
// below the tree's where its parameters differ, and displayed alone where the tree has none, while the cursor still
// is in the same call. Nothing waits on the server.
export async function parameterInfo({ quiet = false } = {}) {
  const found = here();
  if (!found) {
    return;
  }
  const turn = ++state.signatureAsked;
  const answer = await treeSignature(found);
  if (turn !== state.signatureAsked || state.hooks.editor()?.s !== found.s) {
    return;
  }
  const own = answer?.declarations?.length ? fromTree(answer) : null;
  if (own) {
    signatureShown(found.editor, own);
  }
  if (!found.tab.served) {
    if (!own) {
      closeSignature();
      if (!quiet) {
        say(answer ? `The tree declares no ${answer.name}.` : "The cursor is not in a call.");
      }
    }
    return;
  }
  await flush(found.tab);
  const signature = await asked("lsp_signature", found.at).catch(() => null);
  if (turn !== state.signatureAsked || state.hooks.editor()?.s !== found.s) {
    return;
  }
  if (signature) {
    signatureShown(found.editor, own ?? signature, own && !sameParameters(own, signature) ? signature : null);
  } else if (!own) {
    closeSignature();
    if (!quiet) {
      say("The cursor is not in a call.");
    }
  }
}

// After the text of a tab changes: opens Parameter Info where ( or , was just typed, and asks again while it is open.
export function typed() {
  const found = here();
  if (!found) {
    return;
  }
  const before = found.s.doc.line(found.head.line)[found.head.col - 1];
  if (before === "(" || before === "," || signatureOpen()) {
    window.clearTimeout(state.signatureWait);
    state.signatureWait = window.setTimeout(() => parameterInfo({ quiet: true }), SIGNATURE_REST);
  }
}
