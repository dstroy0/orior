// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Requests: a file of HTTP requests sent from the window's own process to the harness's own server,
// each answer shown beside it and kept, two answers compared, a request copied as curl, a curl line
// pasted as a request, and the reader's private values kept out of git.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { previewMode, setPreview } from "../../gui/src-js/preview.js";
import { answerText, bodyText, isRequests, shownAnswers } from "../../gui/src-js/requests.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("preview");
const server = () => window.__harness.servers.ORIOR_TEST_HTTP;

const FILE = '### Echo\nPOST http://{{host}}/echo?x=1\nContent-Type: application/json\nX-Who: {{who}}\n\n{"a": 1}\n\n### Count\nGET http://{{host}}/count\n\n### Moved\nGET http://{{host}}/moved\n\n### Image\nGET http://{{host}}/image\n';

// Sends the request named `key` at line `line` with Ctrl+Enter, and waits for its answer to show first
// among the request's `count` last.
async function sendAt(line, key, count) {
  const { editor } = editing();
  editor.goTo(line, 0);
  editor.focus();
  const before = Date.now();
  await ui.key("Ctrl+Enter");
  const shown = () => shownAnswers();
  await ui.waitFor(() => shown().key === key && shown().answers[0]?.at >= before && shown().answers.length === count && pane().querySelector(".rq-send:not([disabled])"), 15000).catch((error) => {
    throw new Error(`${error.message}: ${shown().key} shows ${shown().answers.length}, ${shown().answers.map((one) => `${one.at - before} ${one.status} ${one.failed ?? ""}`).join("; ")}`);
  });
  return shown().answers[0];
}

test("a file of requests is a .http or a .rest file", () => {
  assert.deepEqual(["a.http", "b.REST", "c.txt", null].map(isRequests), [true, true, false, false]);
});

test("Ctrl+Enter sends the request the cursor stands in with the environment chosen, its answer shown beside it", async () => {
  assert.ok(server(), "the harness's server is told");
  await invoke("file_write", { path: "http-client.env.json", text: JSON.stringify({ local: { host: server(), who: "tester" } }) });
  await invoke("file_write", { path: "calls.http", text: FILE });
  setPreview("off");
  showView("edit");
  await openFile("calls.http");
  await ui.waitFor(() => editing().active === "calls.http", 5000);
  localStorage.setItem("orior.requests.env", "local");
  const answer = await sendAt(1, "Echo", 1);
  assert.equal(previewMode(), "beside", "the preview turned on");
  assert.equal(pane().dataset.kind, "requests");
  assert.equal(answer.status, 200, JSON.stringify(answer).slice(0, 300));
  const echoed = JSON.parse(answer.body);
  assert.deepEqual([echoed.method, echoed.path, echoed.headers["x-who"], echoed.body], ["POST", "/echo?x=1", "tester", '{"a": 1}']);
  await ui.waitFor(() => pane().querySelector(".rq-status strong")?.textContent === "200 OK", 2000);
  assert.match(pane().querySelector(".rq-body").textContent, /\n {2}"method": "POST"/, "JSON laid out");
  assert.match(pane().querySelector(".rq-sent pre").textContent, new RegExp(`^POST http://${server()}/echo\\?x=1\\n`), "what was sent, its variables filled in");
});

test("each request keeps its last answers, the last first, and two of them are compared side by side", async () => {
  await sendAt(8, "Count", 1);
  const second = await sendAt(8, "Count", 2);
  assert.equal(second.body, "2", "the last first");
  assert.equal(shownAnswers().answers[1].body, "1");
  const checks = pane().querySelectorAll(".rq-history input[type=checkbox]");
  assert.equal(checks.length, 2);
  await ui.click(checks[0]);
  await ui.click(pane().querySelectorAll(".rq-history input[type=checkbox]")[1]);
  const compare = pane().querySelector(".rq-compare");
  assert.equal(compare.disabled, false);
  await ui.click(compare);
  await ui.waitFor(() => /^answer at .+, then at /.test(document.querySelector(".diff-sides")?.textContent ?? ""), 3000).catch(() => {
    throw new Error("no comparison shows");
  });
  await ui.key("Escape");
  editing().editor.goTo(1, 0);
  await ui.waitFor(() => shownAnswers().key === "Echo", 3000);
  assert.equal(shownAnswers().answers.length, 1, "the cursor's request shows its own answers");
});

test("a redirect is followed, and an image shows as itself", async () => {
  const moved = await sendAt(11, "Moved", 1);
  assert.equal(moved.status, 200);
  assert.equal(moved.redirects.length, 1);
  assert.match(pane().querySelector(".rq-status").textContent, /after 1 redirect/);
  const image = await sendAt(14, "Image", 1);
  assert.equal(image.binary, true);
  const shown = pane().querySelector(".rq-image");
  assert.ok(shown?.src.startsWith("data:image/png;base64,"));
  await ui.waitFor(() => shown.complete && shown.naturalWidth === 1, 2000);
});

test("Copy as curl gives the request back as a curl line, and a curl line pasted becomes a request", async () => {
  editing().editor.goTo(1, 0);
  runCommand("copy-as-curl");
  await ui.waitFor(async () => (await invoke("clip_read")).includes(server()), 3000);
  const line = await invoke("clip_read");
  assert.match(line, new RegExp(`'http://${server()}/echo\\?x=1'`));
  assert.match(line, /-H 'X-Who: tester'/);
  const { editor } = editing();
  editor.goTo(editor.s.doc.count - 1, 0);
  const data = new DataTransfer();
  data.setData("text/plain", "curl -X PUT 'http://h/items' -H 'A: b' --data-raw 'x'");
  editor.paste({ preventDefault() {}, clipboardData: data });
  await ui.waitFor(() => editor.s.doc.text().includes("PUT http://h/items\nA: b\n"), 3000).catch(() => {
    throw new Error(`the text ends: ${JSON.stringify(editor.s.doc.text().slice(-120))}`);
  });
});

test("Private Environment File makes the reader's own values beside the file and keeps them out of git", async () => {
  runCommand("private-environment");
  await ui.waitFor(() => editing().active === "http-client.private.env.json", 5000);
  assert.deepEqual(JSON.parse(editing().editor.s.doc.text()), { local: {} });
  const ignored = (await invoke("file_read", { path: ".gitignore" })).text;
  assert.match(ignored, /^http-client\.private\.env\.json$/m);
  setPreview("off");
});

test("an answer reads as its status line, its headers and its body, JSON laid out", () => {
  const answer = { version: "HTTP/1.1", status: 200, reason: "OK", headers: [["Content-Type", "application/json"]], body: '{"a":[1]}', binary: false, size: 9 };
  assert.equal(bodyText(answer), '{\n  "a": [\n    1\n  ]\n}');
  assert.equal(answerText(answer), 'HTTP/1.1 200 OK\nContent-Type: application/json\n\n{\n  "a": [\n    1\n  ]\n}');
  assert.equal(answerText({ failed: "refused" }), "refused");
});
