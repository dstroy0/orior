// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// SonarQube: the analysis of SonarQube for IDE run on the tree. Its language server and analyzers,
// found through the toolchain sonarqube-ide, start as the first file opens; this plugin answers what
// the server asks the editor, keeps its settings and tokens, and draws what it says in panels. The
// files of this plugin share one scope: this one holds what they all keep and the helpers they share.

// What the plugin holds while it runs.
const sonar = {
  // the settings as read, the defaults filled in
  settings: null,
  // the tree's paths of the files the editor holds open
  open: new Set(),
  // the server: started, or the start under way
  started: false,
  starting: null,
  // the folder URI the server names the tree by, as it first names it
  folder: null,
  // what the server says it is doing, by task, and the lines of its log
  progress: new Map(),
  log: [],
  // findings the server publishes beside a file's problems, by file URI
  hotspots: new Map(),
  taints: new Map(),
  risks: new Map(),
  // the analyzers the server holds and how each stands
  statuses: [],
  // the last check of each connection, by its id
  checks: new Map(),
  // the new code each folder is held to, and the branch each is compared against, by folder URI
  newCode: new Map(),
  branches: new Map(),
  // the rules, by language, as last listed, and the filter they are shown by
  rules: null,
  ruleFilter: null,
  // the issue whose locations are shown, and the panel each command pressed in a panel runs
  located: null,
  pressed: {},
  // the groups of duplicated blocks last read from the server, each its blocks
  duplications: null,
};

// The panels this plugin shows, each by the id it is shown with.
const PANELS = {
  findings: "SonarQube Findings",
  duplications: "SonarQube Duplications",
  rules: "SonarQube Rules",
  rule: "SonarQube Rule",
  locations: "SonarQube Locations",
  connections: "SonarQube Connections",
  connection: "SonarQube Connection",
  binding: "SonarQube Binding",
  status: "SonarQube Status Change",
  compile: "SonarQube Compilation Database",
  languages: "SonarQube Languages",
  output: "SonarQube Output",
  settings: "SonarQube Settings",
  ask: "SonarQube",
};

// The most lines of the server's log kept.
const LOG_LINES = 5000;

// `text` written so HTML shows it as it is.
function escaped(text) {
  return String(text ?? "").replace(/[&<>"']/g, (one) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[one]);
}

// The whole path a file URI names, its letters unescaped, a drive's path without the slash before it.
function pathOfUri(uri) {
  const rest = decodeURIComponent(String(uri ?? "").replace(/^file:\/\//, ""));
  return /^\/[A-Za-z]:/.test(rest) ? rest.slice(1) : rest;
}

// The file URI of a whole path.
function uriOfPath(path) {
  const forward = String(path).replace(/\\/g, "/");
  const rooted = /^[A-Za-z]:/.test(forward) ? `/${forward}` : forward;
  return `file://${rooted.split("/").map((part, at) => (at === 1 && /^[A-Za-z]:$/.test(part) ? part : encodeURIComponent(part))).join("/")}`;
}

// The tree's path of the file a URI names, null outside the tree.
async function treePathOf(uri) {
  return orior.tree.relative(pathOfUri(uri));
}

// The URI of a file of the tree, by its path in the tree.
function uriInTree(path) {
  const top = sonar.folder ? pathOfUri(sonar.folder).replace(/\/$/, "") : "";
  return uriOfPath(top ? `${top}/${path}` : path);
}

// A line kept in the server's log.
function logged(line) {
  sonar.log.push(`[${new Date().toISOString().slice(11, 19)}] ${line}`);
  if (sonar.log.length > LOG_LINES) {
    sonar.log.splice(0, sonar.log.length - LOG_LINES);
  }
}

// The style every panel is drawn with, from the window's own colors.
async function panelStyle() {
  const color = await orior.theme.colors();
  return `<style>
body{background:${color.bg};color:${color.fg};font:13px/1.5 system-ui,sans-serif;margin:0;padding:10px 14px}
a{color:${color.link}}h1,h2,h3{font-weight:600;margin:12px 0 6px}h1{font-size:16px}h2{font-size:14px}h3{font-size:13px}
table{border-collapse:collapse;width:100%}td,th{border-bottom:1px solid ${color.line};padding:4px 6px;text-align:left;vertical-align:top}
th{color:${color["fg-2"]};font-weight:500}.quiet{color:${color["fg-3"]}}.fail{color:${color.fail}}
button{background:${color.surface};color:${color.fg};border:1px solid ${color.line};border-radius:4px;padding:2px 8px;margin:2px;cursor:pointer}
button:focus-visible,input:focus-visible,select:focus-visible,textarea:focus-visible{outline:2px solid ${color.signal}}
input,select,textarea{background:${color.well};color:${color.fg};border:1px solid ${color.line};border-radius:4px;padding:3px 6px}
textarea{width:100%;min-height:60px}label{display:block;margin:6px 0}pre,code{background:${color.well};border-radius:3px}
pre{padding:8px;overflow:auto}code{padding:0 3px}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
</style>`;
}

// A panel of this plugin shown, by its id, with `body` its HTML.
async function shown(id, body) {
  await orior.panels.show(id, { title: PANELS[id] ?? id, html: `${await panelStyle()}${body}` });
}

// A button that runs the command `command` with `value` as it is pressed.
function button(label, command, value = "") {
  return `<button type="button" data-command="${escaped(command)}" data-value="${escaped(value)}">${escaped(label)}</button>`;
}

// A link that runs the command `command` with `value` as it is pressed.
function linked(label, command, value = "") {
  return `<a href="#" data-command="${escaped(command)}" data-value="${escaped(value)}">${escaped(label)}</a>`;
}

// A question shown in a panel of its own: its text, and a button for each answer, each answer's
// command run with the answer as it is pressed.
async function asked(text, answers, command) {
  await shown("ask", `<p>${escaped(text)}</p><div class="row">${answers.map((answer) => button(answer, command, answer)).join("")}</div>`);
}

// What a command pressed in a panel runs, kept by the command's name.
function onPressed(command, run) {
  sonar.pressed[command] = run;
}

orior.on("panel", async ({ panel, command, value, fields }) => {
  const run = sonar.pressed[command];
  if (run) {
    await run(value, fields ?? {}, panel);
  } else if (command === "link" && /^https?:/.test(String(value))) {
    await orior.browse(value);
  }
});
