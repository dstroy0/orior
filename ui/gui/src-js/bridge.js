// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's one door to the app: the IPC bridge the Rust side puts in every window. A command is
// called by name and answers with a promise, and an event calls its handler with { event, payload }.
// Nothing else in the page reaches the bridge.
//
// The window answers its own commands, those of WINDOW. Every other goes to orior's side through call, the
// window's own calls of OWN answered in the window's process and the tree's side's in its own process for a tree
// on this machine and over its link for a tree on another.

const bridge = () => window.__TAURI_INTERNALS__;

const WINDOW = new Set([
  "app_changes", "app_debug", "app_exit", "app_version", "call", "memory_target", "print_page", "printers_list", "window_act", "window_show", "zoom_set",
]);

// The window's own calls orior's side answers, each sent as a call of the tree's side is with orior-own set: by
// `name` where orior's side knows it by another, its answer dropped where `dropped`, and `failed` given in its place
// where the call fails. Every other own call goes as it is named and fails as it fails.
const OWN = new Map([
  ["remote_get", { name: "link_standing", failed: () => null }],
  ["remote_rejoin", { name: "link_drop", dropped: true, failed: () => null }],
  ["launch_take", { failed: () => null }],
  ["kept_now", { name: "state_now", failed: () => null }],
  ["carry_takings", { failed: () => null }],
  ["plugin_settings_write", { dropped: true }],
  ["network_hold", { name: "network_disable", dropped: true, failed: () => null }],
  ["network_reached", { failed: () => null }],
  ["language_read", { failed: () => "" }],
  ["languages_list", { failed: () => [] }],
  ["report_auto", { failed: () => false }],
  ["report_auto_set", { dropped: true }],
  ["report_open", { dropped: true }],
  ["document_release", { dropped: true, failed: () => null }],
  ["reads_file", { failed: () => false }],
  ["templates_reveal", { dropped: true }],
  ["file_write_any", { dropped: true }],
  ["format_languages", { failed: () => [] }],
  ["link_open", { dropped: true }],
  ["clone_start", { failed: () => null }],
  ["repo_opened", { failed: () => null }],
  ["plugins_read", { failed: () => null }],
  ["user_css_read", { failed: () => "" }],
  ["reader_menus_read", { failed: () => "" }],
  ["home_reveal", { dropped: true }],
  ...[
    "devcontainer_line", "commands_read", "settings_layers", "carry_send", "carry_read", "carry_take", "carry_undo", "plugin_settings_read", "plugins_catalog_read",
    "plugins_catalog_install", "plugin_request", "image_data", "report_error", "report_bug", "report_prompted", "branch_open", "branch_view",
    "branch_language", "branch_signature", "branch_pair", "branch_runs", "branch_at", "branch_cells", "branch_new", "document_edit", "document_changes", "document_find", "structure_compare", "workspace_read", "inspect_languages", "code_docstring", "fill_paragraph", "code_sort",
    "templates_list", "project_create", "checkers_known", "parse_classes", "file_read_any", "toolchain_install", "repo_clone", "repos_folder", "plugin_code",
    "home_folder", "plugin_draft", "plugin_create", "window_open", "memory_usage", "servers_unneeded_stop", "session_key", "scrollback_open", "scrollback_keep",
    "scrollback_read", "scrollback_close", "scrollback_reset", "term_open", "term_write", "term_resize", "term_close",
    "clip_read", "clip_write", "clip_files", "files_copy", "files_paste", "pick", "view_window",
  ].map((name) => [name, {}]),
]);

// A watch over every call, set by ui/test's walker of the menus: it is given each call and what
// makes it, and answers in the call's place.
let watcher = null;

export function watchCalls(given) {
  watcher = given;
  return Boolean(watcher);
}

const encoder = new TextEncoder();

// A call of orior's side goes as its arguments' bytes, its name in a header, and orior's side reads them as they
// come and answers in place: the window between takes nothing apart.
function send(command, args) {
  if (WINDOW.has(command) || command.startsWith("plugin:")) return bridge().invoke(command, args);
  const own = OWN.get(command);
  const headers = own ? { "orior-call": own.name ?? command, "orior-own": "1" } : { "orior-call": command };
  const sent = bridge().invoke("call", encoder.encode(JSON.stringify(args ?? {})), { headers });
  const answered = own?.dropped ? sent.then(() => null) : sent;
  return own?.failed ? answered.catch(own.failed) : answered;
}

// Those told of each call as it is made, each given a way to stop the wait on it, as waits.js is. A wait
// stopped fails its call with an error of the name STOPPED, which the reader asked for and no report files.
const told = new Set();
export const STOPPED = "Stopped";

export function onCalls(listener) {
  told.add(listener);
}

// A call answered later: sent with an ask number of its own, the call returns at once and its answer, or
// why it failed, comes as the event call-answered with that number; a call the command line answers
// in place, as it does where no server holds the file, is answered as it is. Past `wait` ms with no
// answer the ask answers null, so nothing waits on a server that is slow or gone. The answer's event
// can come before the call's own return does, so the ask is waited for before the call is sent.
const asks = new Map();
let askNext = 0;
let asksHeard = null;
export async function asked(command, args = {}, wait = 6000) {
  asksHeard ??= listen("call-answered", (event) => {
    const one = asks.get(event.payload?.request);
    if (one) {
      asks.delete(event.payload.request);
      one(event.payload);
    }
  });
  await asksHeard;
  const ask = ++askNext;
  let heard = null;
  const answered = new Promise((resolve, reject) => {
    heard = (said) => ("error" in said ? reject(new Error(said.error)) : resolve(said.answer));
  });
  // an answer that comes before anything waits on it is no unhandled rejection
  answered.catch(() => {});
  asks.set(ask, heard);
  let sent;
  try {
    sent = await invoke(command, { ...args, request: ask });
  } catch (error) {
    asks.delete(ask);
    throw error;
  }
  if (!(sent && typeof sent === "object" && sent.deferred === true)) {
    asks.delete(ask);
    return sent;
  }
  return new Promise((resolve, reject) => {
    const timer = window.setTimeout(() => {
      asks.delete(ask);
      resolve(null);
    }, wait);
    answered.then(
      (answer) => {
        window.clearTimeout(timer);
        resolve(answer);
      },
      (error) => {
        window.clearTimeout(timer);
        reject(error);
      },
    );
  });
}

export function invoke(command, args = {}) {
  const sent = watcher ? watcher(command, args, send) : send(command, args);
  if (!told.size) {
    return sent;
  }
  let stop;
  const stopped = new Promise((_, fail) => (stop = fail));
  told.forEach((listener) => listener(command, args, sent, (why) => stop(Object.assign(new Error(why), { name: STOPPED }))));
  return Promise.race([sent, stopped]);
}

export async function listen(event, handler) {
  const id = bridge().transformCallback(handler);
  await invoke("plugin:event|listen", { event, target: { kind: "Any" }, handler: id });
}

// The system's picker: "dir" and "save-dir" a folder, "save" a file to write, anything else a file
// to read. Answers the path chosen, or null.
export function pick(type) {
  return invoke("pick", { kind: type });
}

// A page the tree holds opened in a window of its own: orior's side names the window, its title and the address it
// loads from, and the window is made as the web view makes any.
export async function viewOpen(path) {
  const view = await invoke("view_window", { path });
  return invoke("plugin:webview|create_webview_window", { options: { label: view.label, url: view.url, title: view.title, width: 1280, height: 860 } });
}
