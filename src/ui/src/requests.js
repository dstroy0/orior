// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Requests: a file of HTTP requests, `.http` or `.rest`, each sent from the window's own process
// (requests.rs) and its answers shown in the preview beside it. Ctrl+Enter, or Send over the
// answers, sends the request the cursor stands in, with the variables of the environment chosen
// there; the preview turns on beside the text where it is off. An answer shows its status, how long
// each part took, its headers, what was sent, and its body: JSON laid out, an image as itself, and
// any other text as it came. The last answers of the request the cursor stands in stand under it,
// the last first; a press shows one, and two of them checked are compared side by side. Copy as
// curl puts the request on the clipboard as a curl line, a curl line pasted into the file becomes a
// request, and Private Environment File opens the reader's own file of values, made where it is not
// there and named in the tree's .gitignore.

import { invoke } from "./bridge.js";
import { showDiff } from "./diffview.js";
import { editing, onPasteAs, openFile } from "./edit.js";
import { copyText } from "./menu.js";
import { say } from "./statusbar.js";

const ENV = "orior.requests.env";

// The most of a body drawn, in characters; the rest is said and not drawn.
const MOST_DRAWN = 1 << 20;

const state = { holder: null, session: null, path: null, key: null, answers: [], shown: 0, checked: new Set(), envs: [], sending: 0 };

export const isRequests = (path) => /\.(http|rest)$/i.test(path ?? "");

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The environment chosen, or null for none: of those `names` gives, where it is among them.
const chosen = (names = state.envs) => {
  const kept = localStorage.getItem(ENV);
  return kept && names.includes(kept) ? kept : null;
};

// The environment chosen for the file of requests at `path`, its environments read anew.
async function chosenFor(path) {
  const read = await invoke("requests_environments", { path }).catch(() => ({ names: [] }));
  return chosen(read.names ?? []);
}

// The line the cursor stands on in the file shown, where it is the file of requests shown here.
function cursorLine() {
  const view = editing().editor;
  return view && view.s === state.session ? (view.head()?.line ?? 0) : 0;
}

const bytes = (size) => (size < 1024 ? `${size} B` : size < 1 << 20 ? `${(size / 1024).toFixed(1)} KB` : `${(size / (1 << 20)).toFixed(1)} MB`);
const ms = (value) => `${value < 10 ? value.toFixed(1) : Math.round(value)} ms`;
const header = (answer, name) => answer.headers.find(([key]) => key.toLowerCase() === name.toLowerCase())?.[1] ?? "";
const timeOf = (at) => new Date(at).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });

// The body of an answer as it reads: JSON laid out, any other text as it came.
export function bodyText(answer) {
  if (answer.binary) {
    return "";
  }
  if (/json/i.test(header(answer, "Content-Type")) || /^\s*[[{]/.test(answer.body)) {
    try {
      return JSON.stringify(JSON.parse(answer.body), null, 2);
    } catch {
      return answer.body;
    }
  }
  return answer.body;
}

// An answer as text two answers are compared by: its status line, its headers and its body.
export function answerText(answer) {
  if (answer.failed) {
    return answer.failed;
  }
  return [`${answer.version} ${answer.status} ${answer.reason}`, ...answer.headers.map(([name, value]) => `${name}: ${value}`), "", answer.binary ? `${answer.size} bytes, not text` : bodyText(answer)].join("\n");
}

function drawBar() {
  const select = element("select", { className: "rq-env", title: "The environment whose variables fill the request" });
  select.append(element("option", { value: "", textContent: "No environment" }), ...state.envs.map((name) => element("option", { value: name, textContent: name, selected: name === chosen() })));
  select.addEventListener("change", () => localStorage.setItem(ENV, select.value));
  const send = element("button", { type: "button", className: "rq-send", textContent: state.sending ? "Sending" : "Send", disabled: state.sending > 0, title: "Send the request the cursor stands in (Ctrl+Enter)" });
  send.addEventListener("click", () => sendRequest());
  const curl = element("button", { type: "button", className: "rq-curl", textContent: "Copy as curl" });
  curl.addEventListener("click", () => copyCurl());
  return element("div", { className: "rq-bar" }, select, send, curl);
}

function drawTimes(times) {
  const parts = [["resolve", times.resolve], ["connect", times.connect], ["TLS", times.tls], ["wait", times.wait], ["read", times.read]].filter(([, value]) => value > 0);
  return element("p", { className: "rq-times" }, parts.map(([name, value]) => `${name} ${ms(value)}`).join(", "));
}

function drawAnswer(answer) {
  if (!answer) {
    return element("p", { className: "rq-empty" }, "No answer yet. Ctrl+Enter sends the request the cursor stands in.");
  }
  const sent = answer.sent;
  const sentText = [`${sent.method} ${sent.url}`, ...sent.headers.map(([name, value]) => `${name}: ${value}`), ...(sent.body ? ["", sent.body] : [])].join("\n");
  const sentBox = element("details", { className: "rq-sent" }, element("summary", {}, "Sent"), element("pre", {}, sentText));
  if (answer.failed) {
    return element("div", { className: "rq-answer" }, element("p", { className: "rq-status failed" }, answer.failed), sentBox);
  }
  const kind = answer.status >= 400 ? "failed" : answer.status >= 300 ? "moved" : "good";
  const status = element("p", { className: `rq-status ${kind}` }, element("strong", {}, `${answer.status} ${answer.reason}`), ` ${ms(answer.times.total)}, ${bytes(answer.size)}${answer.redirects.length ? `, after ${answer.redirects.length} redirect${answer.redirects.length === 1 ? "" : "s"}` : ""}`);
  const rows = answer.headers.map(([name, value]) => element("tr", {}, element("th", {}, name), element("td", {}, value)));
  const headers = element("details", { className: "rq-headers" }, element("summary", {}, `Headers (${answer.headers.length})`), element("table", {}, ...rows));
  const type = header(answer, "Content-Type");
  let body;
  if (answer.binary && /^image\//i.test(type)) {
    body = element("img", { className: "rq-image", src: `data:${type.split(";")[0]};base64,${answer.body}`, alt: `The image the answer holds, ${bytes(answer.size)}` });
  } else if (answer.binary) {
    body = element("p", { className: "rq-empty" }, `${bytes(answer.size)} of ${type || "bytes"}, not text.`);
  } else {
    const text = bodyText(answer);
    body = element("pre", { className: "rq-body" }, text.length > MOST_DRAWN ? text.slice(0, MOST_DRAWN) : text);
    if (text.length > MOST_DRAWN || answer.cut) {
      body = element("div", {}, body, element("p", { className: "rq-empty" }, `Only the first ${bytes(Math.min(text.length, MOST_DRAWN))} of the body is shown.`));
    }
  }
  return element("div", { className: "rq-answer" }, status, drawTimes(answer.times), headers, sentBox, body);
}

function drawHistory() {
  if (state.answers.length < 2) {
    return element("div", { className: "rq-history", hidden: true });
  }
  const compare = element("button", { type: "button", className: "rq-compare", textContent: "Compare", disabled: state.checked.size !== 2, title: "Compare the two answers checked side by side" });
  compare.addEventListener("click", () => compareChecked());
  const rows = state.answers.map((answer, at) => {
    const check = element("input", { type: "checkbox", checked: state.checked.has(at), title: "Check two answers to compare them" });
    check.addEventListener("change", () => {
      if (check.checked) {
        state.checked.add(at);
      } else {
        state.checked.delete(at);
      }
      draw();
    });
    const label = answer.failed ? "failed" : `${answer.status}`;
    const show = element("button", { type: "button", className: `rq-past${at === state.shown ? " shown" : ""}`, textContent: `${timeOf(answer.at)}  ${label}  ${ms(answer.times.total)}  ${bytes(answer.size)}` });
    show.addEventListener("click", () => {
      state.shown = at;
      draw();
    });
    return element("li", {}, check, show);
  });
  return element("div", { className: "rq-history" }, element("div", { className: "rq-history-bar" }, element("span", {}, "Last answers"), compare), element("ol", {}, ...rows));
}

function draw() {
  if (!state.holder) {
    return;
  }
  const scroll = state.holder.scrollTop;
  state.holder.replaceChildren(drawBar(), drawAnswer(state.answers[state.shown]), drawHistory());
  state.holder.scrollTop = scroll;
}

async function readEnvironments() {
  const read = await invoke("requests_environments", { path: state.path }).catch(() => ({ names: [] }));
  state.envs = read.names ?? [];
  draw();
}

// Reads the answers kept for the request the cursor stands on at `line`, where it is another than
// the one shown.
async function readAnswers(line, force = false) {
  const path = state.path;
  const read = await invoke("requests_answers", { path, text: state.session.doc.text(), line }).catch(() => null);
  if (!read || state.path !== path || (read.key === state.key && !force)) {
    return;
  }
  Object.assign(state, { key: read.key, answers: read.answers, shown: 0, checked: new Set() });
  draw();
}

// Shows the answers of the file of requests `path`, whose text is the session `session`, in `holder`.
export function showRequests(holder, session, path) {
  Object.assign(state, { holder, session, path, key: null, answers: [], shown: 0, checked: new Set() });
  draw();
  readEnvironments();
  readAnswers(cursorLine(), true);
}

export function hideRequests() {
  Object.assign(state, { holder: null, session: null, path: null, key: null, answers: [] });
}

// The cursor stands on line `line`: the answers shown are those of the request it stands in.
export function requestsCursor(line) {
  if (state.holder) {
    readAnswers(line);
  }
}

// What the last drawing shows, for the tests: the request's key, its answers and the one shown.
export const shownAnswers = () => ({ key: state.key, answers: state.answers, shown: state.shown });

// Sends the request the cursor stands in, and shows its answer first among the request's last.
export async function sendRequest() {
  const { editor, active } = editing();
  if (!editor || !isRequests(active)) {
    say("Send Request sends a request of a file of requests, .http or .rest", { failed: true });
    return;
  }
  const line = editor.head()?.line ?? 0;
  const text = editor.s.doc.text();
  const { previewMode, setPreview } = await import("./preview.js");
  if (previewMode() === "off") {
    setPreview("beside");
  }
  state.sending += 1;
  draw();
  try {
    const answer = await invoke("requests_send", { path: active, text, line, env: await chosenFor(active) });
    if (answer.failed) {
      say(answer.failed, { failed: true });
    }
    if (state.path === active) {
      state.key = null;
      await readAnswers(line, true);
    }
  } catch (error) {
    say(String(error), { failed: true });
  } finally {
    state.sending -= 1;
    draw();
  }
}

// Puts the request the cursor stands in on the clipboard as a curl line.
export async function copyCurl() {
  const { editor, active } = editing();
  if (!editor || !isRequests(active)) {
    say("Copy as curl copies a request of a file of requests, .http or .rest", { failed: true });
    return;
  }
  const at = editor.head()?.line ?? 0;
  const text = editor.s.doc.text();
  try {
    const line = await invoke("requests_curl", { path: active, text, line: at, env: await chosenFor(active) });
    await copyText(line);
    say("The request is on the clipboard as a curl line");
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Opens the reader's own file of private values beside the file of requests, made where it is not
// there.
export async function privateEnvironment() {
  const { active } = editing();
  if (!isRequests(active)) {
    say("Private Environment File opens the private values of a file of requests, .http or .rest", { failed: true });
    return;
  }
  try {
    await openFile(await invoke("requests_private", { path: active }));
  } catch (error) {
    say(String(error), { failed: true });
  }
}

function compareChecked() {
  const [newer, older] = [...state.checked].sort((a, b) => a - b).map((at) => state.answers[at]);
  if (!newer || !older) {
    return;
  }
  showDiff(document.querySelector("#mode-edit .desk"), state.key, answerText(older), answerText(newer), { sides: `answer at ${timeOf(older.at)}, then at ${timeOf(newer.at)}`, same: "The two answers are the same." });
}

export function startRequests() {
  onPasteAs((path, text) => (isRequests(path) && /^\s*curl\s/.test(text) ? invoke("requests_from_curl", { text }).catch(() => text) : null));
}
