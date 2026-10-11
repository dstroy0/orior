// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The page's one door to the app: the IPC bridge the Rust side puts in every window. A command is
// called by name and answers with a promise, and an event calls its handler with { event, payload }.
// Nothing else in the page reaches the bridge.
//
// The window answers its own commands, those of WINDOW; every other is the tree's side's, which the
// window answers through call, in its own process for a tree on this machine and over its link for a
// tree on another.

const bridge = () => window.__TAURI_INTERNALS__;

const WINDOW = new Set([
  "app_exit", "app_version", "call", "checkers_known", "clip_files", "clip_read", "clip_write", "clone_start",
  "code_docstring", "code_sort", "commands_read", "devcontainer_line", "file_read_any", "file_slice", "file_write_any",
  "files_copy", "files_paste", "fill_paragraph", "format_languages", "highlight_grammar", "highlight_lines",
  "home_reveal", "inspect_languages", "kept_write", "launch_take", "link_open", "memory_calls", "memory_hold", "memory_use", "print_page", "printers_list",
  "parse_classes", "pick", "plugin_create", "plugin_draft", "plugins_read", "project_create", "reader_menus_read",
  "reads_file", "remote_get", "remote_rejoin", "repo_clone", "repo_opened", "report_asked", "report_auto", "report_auto_set", "report_bug", "report_error", "report_open",
  "repos_folder", "scrollback_close", "scrollback_keep", "scrollback_open", "scrollback_read", "scrollback_reset",
  "structure_compare", "templates_list", "templates_reveal", "term_close", "term_open", "term_resize", "term_write",
  "toolchain_install", "user_css_read", "view_open", "window_act", "window_open", "window_show", "workspace_read",
  "zoom_set",
]);

// A watch over every call, set by src/ui/test's walker of the menus: it is given each call and what
// makes it, and answers in the call's place.
let watcher = null;

export function watchCalls(given) {
  watcher = given;
  return Boolean(watcher);
}

function send(command, args) {
  if (WINDOW.has(command) || command.startsWith("plugin:")) return bridge().invoke(command, args);
  return bridge().invoke("call", { name: command, args });
}

export function invoke(command, args = {}) {
  return watcher ? watcher(command, args, send) : send(command, args);
}

export async function listen(event, handler) {
  const id = bridge().transformCallback(handler);
  await invoke("plugin:event|listen", { event, target: { kind: "Any" }, handler: id });
}

// The system's picker: "dir" and "save-dir" a folder, "save" a file to write, anything else a file
// to read. Answers the path chosen, or null.
export function pick(kind) {
  return invoke("pick", { kind });
}
