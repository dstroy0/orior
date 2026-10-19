// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the window, a layer on orior's C side (orior.rs). Every job, every reading of the tree and
//! every command of the menus is that side's; this layer adds the window, the commands its page
//! calls, the `view` scheme its page windows load from, the terminal's pseudo-terminals and the
//! clipboard. The same program is the command line, handing it any words it is started with.

mod allocations;
mod clip;
mod dragging;
mod memory;
mod orior;
mod place;
mod printing;

#[global_allocator]
static ALLOCATOR: allocations::Counting = allocations::Counting;
mod scrollback;
mod terminal;

use std::borrow::Cow;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};

use tauri::http::{Request, Response, StatusCode};
use tauri::{AppHandle, Emitter, Manager, State, UriSchemeContext, WebviewUrl, WebviewWindowBuilder};

#[derive(Default)]
struct App {
    terms: terminal::Terms,
    scrollbacks: scrollback::Scrollbacks,
    windows: AtomicU64,
}

/// The tree open here, or why there is none.
fn root_of(_app: &App) -> Result<PathBuf, String> {
    orior::own_as::<String>("root", serde_json::json!({})).map(PathBuf::from)
}

/// The machine the tree is on, where it is on another, as the link names it.
fn linked() -> Option<String> {
    orior::own("link_address", serde_json::json!({})).ok().and_then(|address| address["machine"].as_str().map(String::from))
}

/// Refuses what only a tree on this machine does, the system's clipboard of files being this
/// machine's.
fn local_only(_app: &App) -> Result<(), String> {
    match linked() {
        Some(machine) => Err(format!("files are cut, copied and pasted on this machine, and the tree is on {machine}")),
        None => Ok(()),
    }
}

/// Answers a command of the tree's side by its name, as serve.c answers it.
#[tauri::command(async)]
fn call(handle: AppHandle, app: State<App>, name: String, args: serde_json::Value) -> Result<serde_json::Value, String> {
    if name == "files_bring" && linked().is_some() {
        bring_over(handle, args)
    } else {
        tree_call(&app, &name, args)
    }
}

/// A call of the tree's side: over the link where the tree is on another machine, and here where not.
fn tree_call(_app: &App, name: &str, args: serde_json::Value) -> Result<serde_json::Value, String> {
    allocations::measured(name, || orior::tree(name, args))
}

/// Files dropped on a window whose tree is on another machine: they are on this machine, and are read
/// here and written there a part at a time on a thread of their own, each telling how far along it is
/// as a copy on this machine does.
fn bring_over(handle: AppHandle, args: serde_json::Value) -> Result<serde_json::Value, String> {
    let into = args["into"].as_str().unwrap_or_default().to_string();
    let paths: Vec<PathBuf> = args["paths"].as_array().map(|all| all.iter().filter_map(|one| one.as_str().map(PathBuf::from)).collect()).unwrap_or_default();
    std::thread::spawn(move || {
        let paths: Vec<String> = paths.iter().map(|path| path.display().to_string()).collect();
        let done = match orior::own("files_bring_over", serde_json::json!({"into": into, "paths": paths})) {
            Ok(brought) => serde_json::json!({"into": into, "brought": brought}),
            Err(said) => serde_json::json!({"into": into, "error": said}),
        };
        let _ = handle.emit("bring-done", done);
    });
    Ok(serde_json::Value::Null)
}

/// The system's printers, and whether a page prints on them with no dialog of the system's.
#[tauri::command(async)]
fn printers_list() -> serde_json::Value {
    serde_json::json!({ "printers": printing::printers(), "silent": printing::SILENT })
}

/// Prints a page from orior's print sheet, or writes it as a PDF, as `settings` say.
#[tauri::command(async)]
fn print_page(app: tauri::AppHandle, html: String, settings: printing::Settings) -> Result<String, String> {
    printing::print(&app, html, settings)
}

/// What the calls of the tree's side took and kept, by name, while the count is on: `on` turns it on
/// or off, and `reset` empties it once read.
#[tauri::command]
fn memory_calls(on: Option<bool>, reset: Option<bool>) -> std::collections::BTreeMap<String, allocations::Calls> {
    allocations::read(on, reset.unwrap_or(false))
}

/// The tree's machine, its address and how its link stands, where the tree is on another machine.
#[tauri::command]
fn remote_get(app: State<App>) -> Option<serde_json::Value> {
    let _ = app;
    orior::own("link_standing", serde_json::json!({})).ok().filter(|standing| !standing.is_null())
}

/// Lets go of the link to the tree's machine, for it to be made again.
#[tauri::command]
fn remote_rejoin(app: State<App>) {
    let _ = app;
    let _ = orior::own("link_drop", serde_json::json!({}));
}

/// The shell line that brings the tree's dev container up and opens a window on the tree in it, for
/// the terminal to run.
#[tauri::command]
fn devcontainer_line(app: State<App>) -> Result<String, String> {
    let _ = app;
    orior::own_as::<String>("devcontainer_line", serde_json::json!({}))
}

/// Opens a terminal at the tree's top folder, or at the home folder where no tree is open.
#[tauri::command]
fn term_open(handle: AppHandle, app: State<App>, cols: u16, rows: u16, at: Option<String>) -> Result<u64, String> {
    if linked().is_some() {
        let folder = at.map(|at| terminal::folder_of(&at).to_string_lossy().replace('\\', "/"));
        let mut words: Vec<String> = orior::own_as("link_shell_words", serde_json::json!({"folder": folder}))?;
        let program = if words.is_empty() { String::new() } else { words.remove(0) };
        // The program as a file on the PATH: on Windows the pseudo-terminal runs the first file of the
        // name it finds, and docker's folder holds a script named docker beside docker.exe.
        let found: Option<String> = orior::own_as("path_program", serde_json::json!({"program": program}))?;
        let mut command = portable_pty::CommandBuilder::new(found.unwrap_or(program));
        command.args(words);
        command.env("TERM", "xterm-256color");
        return app.terms.open(handle, command, cols, rows, None);
    }
    let home = std::env::var_os(if cfg!(windows) { "USERPROFILE" } else { "HOME" }).map(PathBuf::from);
    let kept = at.map(|at| terminal::folder_of(&at)).filter(|folder| folder.is_dir());
    let at = kept.or_else(|| root_of(&app).ok()).or(home).or_else(|| std::env::current_dir().ok()).ok_or("no folder to open a terminal in")?;
    let mut command = terminal::shell(&at)?;
    let part = keyed(&app, &mut command);
    app.terms.open(handle, command, cols, rows, part)
}

/// A terminal's part of the comms layer: its shell, and every program run in it, joins the door with
/// the key the shell is started with, and tells the window of the runs it starts.
fn keyed(_app: &App, command: &mut portable_pty::CommandBuilder) -> Option<terminal::Part> {
    let (part, address, key) = orior::Part::made()?;
    command.env(orior::ADDRESS_VARIABLE, address);
    command.env(orior::KEY_VARIABLE, key);
    Some(part)
}

#[tauri::command]
fn term_write(app: State<App>, id: u64, text: String) -> Result<(), String> {
    app.terms.write(id, &text)
}

#[tauri::command]
fn term_resize(app: State<App>, id: u64, cols: u16, rows: u16) -> Result<(), String> {
    app.terms.resize(id, cols, rows)
}

#[tauri::command]
fn term_close(app: State<App>, id: u64) -> Result<(), String> {
    app.terms.close(id)
}

/// A scrollback on disk for a terminal screen, and its number.
#[tauri::command]
fn scrollback_open(app: State<App>) -> Result<u64, String> {
    app.scrollbacks.open()
}

/// Adds the lines that scrolled off a terminal screen to its scrollback, and answers how many it holds.
#[tauri::command(async)]
fn scrollback_keep(app: State<App>, id: u64, lines: Vec<String>) -> Result<u64, String> {
    app.scrollbacks.keep(id, &lines)
}

#[tauri::command(async)]
fn scrollback_read(app: State<App>, id: u64, from: u64, count: u64) -> Result<Vec<String>, String> {
    app.scrollbacks.read(id, from, count)
}

#[tauri::command]
fn scrollback_close(app: State<App>, id: u64) {
    app.scrollbacks.close(id);
}

#[tauri::command]
fn scrollback_reset(app: State<App>) {
    app.scrollbacks.reset();
}

/// The menus, as the command line reads them.
#[tauri::command]
fn commands_read() -> Result<String, String> {
    orior::own_as("commands_read", serde_json::json!({}))
}

/// The command the window was opened to run, once.
#[tauri::command]
fn launch_take(app: State<App>) -> Option<serde_json::Value> {
    let _ = app;
    orior::own("launch_take", serde_json::json!({})).ok().filter(|launch| !launch.is_null())
}

/// The app's version, as its package gives it.
#[tauri::command]
fn app_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

/// What each release of the window changed, as CHANGES.md gives it, built into the app.
#[tauri::command]
fn app_changes() -> &'static str {
    include_str!("../../../CHANGES.md")
}

/// Ends the app, every window of it, from the File menu.
/// What the app holds in memory, its own process and every one it started, or nothing where the
/// system does not say.
#[tauri::command]
fn memory_use(app: State<App>) -> Option<memory::Memory> {
    let mut read = memory::read()?;
    let _ = app;
    named(&mut read, &running());
    Some(read)
}

/// A language server running: its toolchain, its program and its process.
#[derive(serde::Deserialize)]
struct Running {
    tool: String,
    program: String,
    pid: u32,
}

/// Every language server running.
fn running() -> Vec<Running> {
    orior::own_as("servers_running", serde_json::json!({})).unwrap_or_default()
}

// Names each part of a reading that is a language server by its program, with its toolchain's id,
// and the web view's as the web view.
fn named(read: &mut memory::Memory, running: &[Running]) {
    for part in &mut read.parts {
        if let Some(server) = running.iter().find(|one| one.pid == part.pid) {
            part.name = server.program.clone();
            part.server = Some(server.tool.clone());
        } else if part.name == "msedgewebview2" {
            part.name = "web view".into();
        }
    }
}

/// What holding memory to the budget let go: the servers stopped and the files they had open.
#[derive(serde::Serialize)]
struct Held {
    stopped: Vec<String>,
    files: Vec<String>,
}

/// Holds the app's memory to `budget` bytes: stops the language servers no language in `open`, the
/// languages of the open tabs, needs; and where the app still holds more than the budget, the server
/// that holds the most and does not serve `shown`, the language of the tab shown. Gives what it
/// stopped. While the tree's check goes on, it needs every server, and none is stopped.
#[tauri::command(async)]
fn memory_hold(app: State<App>, open: Vec<String>, shown: Option<String>, budget: u64) -> Held {
    let _ = app;
    if orior::own_as::<bool>("servers_checking", serde_json::json!({})).unwrap_or(false) {
        return Held { stopped: Vec::new(), files: Vec::new() };
    }
    let running = running();
    let tools_for = |languages: &[String]| orior::own_as::<Vec<String>>("servers_tools_for", serde_json::json!({"languages": languages})).unwrap_or_default();
    let needed = tools_for(&open);
    let mut stopped: Vec<String> = running.iter().filter(|one| !needed.contains(&one.tool)).map(|one| one.tool.clone()).collect();
    if stopped.is_empty() {
        let keep = tools_for(&shown.into_iter().collect::<Vec<_>>());
        if let Some(mut read) = memory::read().filter(|read| read.working > budget) {
            named(&mut read, &running);
            stopped.extend(read.parts.iter().filter_map(|part| part.server.clone().filter(|tool| !keep.contains(tool))).take(1));
        }
    }
    let names = stopped.iter().map(|tool| running.iter().find(|one| &one.tool == tool).map_or_else(|| tool.clone(), |one| one.program.clone())).collect();
    let files: Vec<String> = orior::own_as("servers_stop", serde_json::json!({"tools": stopped})).unwrap_or_default();
    Held { stopped: names, files }
}

/// Shows the window once its page has its scheme and its colors, so that no frame before them shows.
#[tauri::command]
fn window_show(window: tauri::WebviewWindow) -> Result<(), String> {
    window.show().map_err(|e| e.to_string())
}

#[tauri::command]
fn app_exit(handle: AppHandle) {
    handle.exit(0);
}

/// The text on the system clipboard, read here so the page never asks the reader for leave to read
/// it. Empty where the clipboard holds no text.
#[tauri::command]
fn clip_read() -> String {
    clip::with(|clip| clip.get_text()).unwrap_or_default()
}

/// Puts text on the system clipboard, written here so the page never asks the reader for leave to
/// write it.
#[tauri::command]
fn clip_write(text: String) -> Result<(), String> {
    clip::with(|clip| clip.set_text(text))
}

/// Keeps the page's changed entries in orior's own folder, each key starting `orior.` with its new
/// text, or null where it is gone.
#[tauri::command(async)]
fn kept_write(changes: std::collections::BTreeMap<String, Option<String>>) -> Result<(), String> {
    orior::own("kept_write", serde_json::json!({"changes": changes})).map(|_| ())
}

/// The settings in force for the tree at `root`, from the reader's settings and the tree's, `tree`,
/// as the tree's side read them (settings.rs).
#[tauri::command(async)]
fn settings_layers(tree: serde_json::Value, root: String) -> Result<serde_json::Value, String> {
    orior::own("settings_layers", serde_json::json!({"tree": tree, "root": root}))
}

/// Writes the reader's settings that travel to `to`, a file or a folder of the reader's (carry.c).
#[tauri::command(async)]
fn carry_send(to: String) -> Result<serde_json::Value, String> {
    orior::own("carry_send", serde_json::json!({"to": to}))
}

/// What the file or the folder `from` carries, and how it differs from the reader's own here.
#[tauri::command(async)]
fn carry_read(from: String) -> Result<serde_json::Value, String> {
    orior::own("carry_read", serde_json::json!({"from": from}))
}

/// Takes in the files of `places` that `there`, read from `from`, carries, and of settings.json the
/// settings of `keys` alone.
#[tauri::command(async)]
fn carry_take(from: String, there: serde_json::Value, places: Vec<String>, keys: Vec<String>) -> Result<serde_json::Value, String> {
    orior::own("carry_take", serde_json::json!({"from": from, "there": there, "places": places, "keys": keys}))
}

/// Every taking kept, the last first.
#[tauri::command(async)]
fn carry_takings() -> serde_json::Value {
    orior::own("carry_takings", serde_json::json!({})).unwrap_or_default()
}

#[tauri::command(async)]
fn carry_undo(id: String) -> Result<serde_json::Value, String> {
    orior::own("carry_undo", serde_json::json!({"id": id}))
}

/// The settings and the themes orior's folder keeps now, by key without `orior.`, and each of the
/// page's keys `keys` that is a setting the folder no longer keeps.
#[tauri::command(async)]
fn kept_now(keys: Vec<String>) -> serde_json::Value {
    orior::own("kept_now", serde_json::json!({"keys": keys})).unwrap_or_default()
}

/// The settings of the plugin `id`, as it last wrote them, or an empty object.
#[tauri::command(async)]
fn plugin_settings_read(id: String) -> Result<serde_json::Value, String> {
    orior::own("plugin_settings_read", serde_json::json!({"id": id}))
}

/// Keeps `values` as the settings of the plugin `id`, in place of those it kept before.
#[tauri::command(async)]
fn plugin_settings_write(id: String, values: serde_json::Value) -> Result<(), String> {
    orior::own("plugin_settings_write", serde_json::json!({"id": id, "values": values})).map(|_| ())
}

/// Holds orior off the network, or lets it on, as the reader's setting `network` says (net.c).
#[tauri::command]
fn network_hold(off: bool) {
    let _ = orior::own("network_hold", serde_json::json!({"off": off}));
}

/// The addresses orior has reached, the last first, with when and what for.
#[tauri::command(async)]
fn network_reached() -> serde_json::Value {
    orior::own("network_reached", serde_json::json!({})).unwrap_or_default()
}

/// The text of the reader's language file for the language `tag`, or nothing where there is none.
#[tauri::command(async)]
fn language_read(tag: String) -> String {
    orior::own_as("language_read", serde_json::json!({"tag": tag})).unwrap_or_default()
}

/// The languages the reader has files for, by tag, in order.
#[tauri::command(async)]
fn languages_list() -> Vec<String> {
    orior::own_as("languages_list", serde_json::json!({})).unwrap_or_default()
}

/// The plugins the catalog at `address` lists.
#[tauri::command(async)]
fn plugins_catalog_read(address: String) -> Result<serde_json::Value, String> {
    orior::own("plugins_catalog_read", serde_json::json!({"address": address}))
}

/// Installs the plugin `entry` of the catalog at `address`, in place of the one of its id there.
#[tauri::command(async)]
fn plugins_catalog_install(address: String, entry: serde_json::Value) -> Result<String, String> {
    orior::own_as("plugins_catalog_install", serde_json::json!({"address": address, "entry": entry}))
}

/// A request the plugin `id` sends to a host it asked to reach, as the window lets it: its answer's
/// status, headers and body, the body in base64 where it is not text.
#[tauri::command(async)]
fn plugin_request(id: String, method: String, url: String, headers: Vec<(String, String)>, body: String) -> Result<serde_json::Value, String> {
    orior::own("plugin_request", serde_json::json!({"id": id, "method": method, "url": url, "headers": headers, "body": body}))
}

/// The picture at `path` as the data address a theme keeps for the image behind the editor: PNG,
/// JPEG, GIF, WebP, SVG or BMP, of 8 MB at most.
#[tauri::command(async)]
fn image_data(path: String) -> Result<String, String> {
    orior::own_as("image_data", serde_json::json!({"path": path}))
}

/// The script that puts what orior's folder keeps into the page's storage before the page's own
/// scripts run, on the first load of the main window's page in a run, in place of every entry the
/// page kept before. Where the folder keeps nothing yet, it tells the page to write all it holds
/// there.
fn kept_script() -> String {
    let kept = orior::own("kept_read", serde_json::json!({})).unwrap_or_default();
    let kept = serde_json::to_string(&kept).unwrap_or_else(|_| "null".into());
    format!(
        "(() => {{ if (location.protocol === \"view:\" || location.hostname === \"view.localhost\") return; \
         if (sessionStorage.getItem(\"orior.kept.read\")) return; sessionStorage.setItem(\"orior.kept.read\", \"1\"); \
         const kept = {kept}; if (kept === null) {{ window.oriorKeepAll = true; return; }} \
         for (const key of Object.keys(localStorage)) {{ if (key.startsWith(\"orior.\") && !(key in kept)) localStorage.removeItem(key); }} \
         for (const [key, text] of Object.entries(kept)) localStorage.setItem(key, text); }})();"
    )
}

/// Puts files and folders of the tree on the system clipboard, cut or copied, for another window of
/// orior or the system's file manager to paste.
#[tauri::command]
fn files_copy(app: State<App>, paths: Vec<String>, cut: bool) -> Result<(), String> {
    local_only(&app)?;
    let full: Vec<PathBuf> = orior::own_as::<Vec<String>>("tree_inside", serde_json::json!({"paths": paths}))?.into_iter().map(PathBuf::from).collect();
    clip::put(&full, cut)
}

/// How many files and folders the system clipboard holds.
#[tauri::command]
fn clip_files() -> usize {
    clip::held().map_or(0, |(paths, _)| paths.len())
}

/// Pastes the files and folders on the system clipboard into a folder of the tree, moving those that
/// were cut, and says where each went.
#[tauri::command(async)]
fn files_paste(app: State<App>, into: String) -> Result<serde_json::Value, String> {
    local_only(&app)?;
    let Some((paths, cut)) = clip::held() else {
        return Ok(serde_json::json!([]));
    };
    let paths: Vec<String> = paths.iter().map(|path| path.display().to_string()).collect();
    let pasted = orior::own("files_paste", serde_json::json!({"into": into, "paths": paths, "cut": cut}))?;
    if cut {
        clip::let_go();
    }
    Ok(pasted)
}

/// Sets the window's zoom, 1 being none.
#[tauri::command]
fn zoom_set(webview: tauri::Webview, factor: f64) -> Result<(), String> {
    webview.set_zoom(factor.clamp(0.5, 3.0)).map_err(|e| e.to_string())
}

/// Files an error the window met, on its own where the reporter lets errors file. Answers where the
/// report went, or nothing.
#[tauri::command]
async fn report_error(category: String, message: String, detail: String) -> Result<serde_json::Value, String> {
    tauri::async_runtime::spawn_blocking(move || orior::own("report_error", serde_json::json!({"category": category, "message": message, "detail": detail})))
        .await
        .map_err(|e| e.to_string())?
}

/// Files a bug report the reader wrote, with the run's recent errors where they asked for them.
#[tauri::command]
async fn report_bug(report: serde_json::Value, with_errors: bool) -> Result<serde_json::Value, String> {
    tauri::async_runtime::spawn_blocking(move || orior::own("report_bug", serde_json::json!({"report": report, "with_errors": with_errors})))
        .await
        .map_err(|e| e.to_string())?
}

#[tauri::command]
fn report_auto() -> bool {
    orior::own_as("report_auto", serde_json::json!({})).unwrap_or(false)
}

/// Whether the question of whether errors file on their own needs no asking, as the reporter has
/// answered it or ORIOR_NO_REPORTS turned reports off for the run, and the question. A build for
/// testing, `cargo tauri dev`'s, answers yes on its own and keeps no answer: nothing a test runs
/// waits on the question, every error it meets files as an issue, and an install still asks its
/// reporter.
#[tauri::command]
fn report_asked() -> serde_json::Value {
    let mut asked = orior::own("report_asked", serde_json::json!({})).unwrap_or_default();
    if cfg!(debug_assertions) {
        asked[0] = serde_json::Value::Bool(true);
    }
    asked
}

#[tauri::command]
fn report_auto_set(on: bool) -> Result<(), String> {
    orior::own("report_auto_set", serde_json::json!({"on": on})).map(|_| ())
}

#[tauri::command]
fn report_open(url: String) -> Result<(), String> {
    orior::own("report_open", serde_json::json!({"url": url})).map(|_| ())
}

/// The window's own controls, for the frame the page draws in place of the system's: minimize,
/// maximize, which restores a maximized window, close, and drag, which moves the window with the
/// pointer from a press on the top bar until it is let go. Says whether the window is maximized after.
#[tauri::command]
fn window_act(window: tauri::WebviewWindow, act: String) -> Result<bool, String> {
    let done = match act.as_str() {
        "minimize" => window.minimize(),
        "maximize" => {
            if window.is_maximized().unwrap_or(false) {
                window.unmaximize()
            } else {
                window.maximize()
            }
        }
        "close" => window.close(),
        "next-display" => to_next_display(&window),
        "system-title" => window.set_decorations(true),
        "own-title" => window.set_decorations(false),
        "focus" => window.unminimize().and_then(|()| window.set_focus()),
        "drag" => return dragging::start_drag(&window).map(|()| window.is_maximized().unwrap_or(false)),
        "state" => Ok(()),
        other => return Err(format!("{other} is not something the window does")),
    };
    done.map_err(|error| error.to_string())?;
    Ok(window.is_maximized().unwrap_or(false))
}

/// Whether the reader asked for the system's own title bar, as settings.json keeps it.
fn system_title_bar() -> bool {
    orior::own("kept_read", serde_json::json!({})).ok().is_some_and(|kept| kept["orior.titlebar"] == "system")
}

/// Moves the window to the display after the one it stands on, maximized there where it was here.
fn to_next_display(window: &tauri::WebviewWindow) -> tauri::Result<()> {
    let displays = window.available_monitors()?;
    if displays.len() < 2 {
        return Ok(());
    }
    let here = window.current_monitor()?;
    let at = here.and_then(|here| displays.iter().position(|one| one.position() == here.position())).unwrap_or(0);
    let next = &displays[(at + 1) % displays.len()];
    let maximized = window.is_maximized().unwrap_or(false);
    if maximized {
        window.unmaximize()?;
    }
    window.set_position(tauri::PhysicalPosition::new(next.position().x + 40, next.position().y + 40))?;
    if maximized {
        window.maximize()?;
    }
    Ok(())
}

/// Opens another window, a program of its own with its own tabs, terminals and servers, on the tree
/// at `root`, or on this window's tree where none is named, to run the menus' command `words` names
/// once its page is up.
#[tauri::command]
fn window_open(app: State<App>, root: Option<String>, remote: Option<String>, words: Vec<String>) -> Result<(), String> {
    let program = std::env::current_exe().map_err(|error| error.to_string())?;
    let mut given = vec![program.display().to_string()];
    let linked = orior::own("link_address", serde_json::json!({})).ok().and_then(|address| address["address"].as_str().map(String::from));
    match (root, remote.or(linked)) {
        (None, Some(remote)) => {
            orior::own("address_parse", serde_json::json!({"text": remote}))?;
            given.extend(["--remote".to_string(), remote]);
        }
        (Some(root), _) => {
            given.extend(["--root".to_string(), orior::own_as::<String>("tree_of", serde_json::json!({"root": root}))?]);
        }
        (None, None) => {
            given.extend(["--root".to_string(), root_of(&app)?.display().to_string()]);
        }
    }
    given.extend(words);
    // Another window is the reader's to keep, and stays as this one closes.
    orior::own("started_outside", serde_json::json!({"words": given})).map(|_| ())
}

/// Reads a grammar of the page's and keeps it under `key` for the lines colored with it, or says what
/// in it is not read here.
#[tauri::command]
fn highlight_grammar(key: String, def: serde_json::Value) -> Result<(), String> {
    orior::own("highlight_grammar", serde_json::json!({"key": key, "def": def})).map(|_| ())
}

/// Colors lines with the grammar kept under `key`, the first starting in `state`.
#[tauri::command(async)]
fn highlight_lines(key: String, state: String, lines: Vec<String>) -> Result<serde_json::Value, String> {
    orior::own("highlight_lines", serde_json::json!({"key": key, "state": state, "lines": lines}))
}

/// Holds the file at `path`, read from the tree as the page read it, whole or, for a file the page
/// reads a window at a time, by its slices, under `key` for the page's slow work on it to run here, and
/// answers its shape for the page to compare with its own; null where the file is not text.
#[tauri::command(async)]
fn document_read(app: State<App>, key: String, path: String) -> Result<serde_json::Value, String> {
    let _ = app;
    orior::own("document_read", serde_json::json!({"key": key, "path": path}))
}

/// Holds a text the page gives a part at a time under `key`, answering its shape once the last comes.
#[tauri::command(async)]
fn document_part(key: String, part: String, first: bool, done: bool) -> Result<serde_json::Value, String> {
    orior::own("document_part", serde_json::json!({"key": key, "part": part, "first": first, "done": done}))
}

/// Writes the changes the page made to the text held under `key`, in order, answering its shape.
#[tauri::command(async)]
fn document_edit(key: String, changes: serde_json::Value) -> Result<serde_json::Value, String> {
    orior::own("document_edit", serde_json::json!({"key": key, "changes": changes}))
}

/// Lets go of the text held under `key`.
#[tauri::command(async)]
fn document_release(key: String) {
    let _ = orior::own("document_release", serde_json::json!({"key": key}));
}

/// The gutter's marks for the text held under `key` against the file at `path` as the last commit
/// holds it, read from the tree where it is not kept beside the text yet or `fresh` asks; null where
/// no text is held under `key`.
#[tauri::command(async)]
fn document_changes(app: State<App>, key: String, path: String, fresh: bool) -> Result<serde_json::Value, String> {
    let _ = app;
    orior::own("document_changes", serde_json::json!({"key": key, "path": path, "fresh": fresh}))
}

/// Find's matches in the text held under `key`, each [line, from col, to col], at most `most`; an
/// error where the search is the page's own.
#[tauri::command(async)]
fn document_find(key: String, sought: serde_json::Value, most: usize) -> Result<serde_json::Value, String> {
    orior::own("document_find", serde_json::json!({"key": key, "sought": sought, "most": most}))
}

/// Two texts compared by structure, the older first, or null where they are too far apart.
#[tauri::command(async)]
fn structure_compare(then: Option<String>, now: String) -> Result<serde_json::Value, String> {
    orior::own("structure_compare", serde_json::json!({"then": then, "now": now}))
}

/// The folders a .code-workspace file names, each made whole against the file's own folder, as JSON
/// with comments and trailing commas reads them.
#[tauri::command]
fn workspace_read(path: String) -> Result<serde_json::Value, String> {
    orior::own("workspace_read", serde_json::json!({"path": path}))
}

/// The languages orior's own inspections read, by the editor's names for them.
#[tauri::command]
fn inspect_languages() -> Result<serde_json::Value, String> {
    orior::own("inspect_languages", serde_json::json!({}))
}

/// A docstring drawn up for the function at or around `line` of a text in `language`, in `form`:
/// its edit, and the place to write its summary.
#[tauri::command]
fn code_docstring(language: String, text: String, line: u32, form: String) -> Result<serde_json::Value, String> {
    orior::own("code_docstring", serde_json::json!({"language": language, "text": text, "line": line, "form": form}))
}

/// The edit that fills the paragraph of documentation at `line` of a text to `width` columns.
#[tauri::command]
fn fill_paragraph(text: String, line: u32, width: usize) -> Result<serde_json::Value, String> {
    orior::own("fill_paragraph", serde_json::json!({"text": text, "line": line, "width": width}))
}

/// Whether orior reads the file at `path` of the tree itself: a build file, a Gradle script or
/// catalog, a POM, an SConstruct or an SConscript, or JSON, read by its schema.
#[tauri::command]
fn reads_file(path: String) -> bool {
    orior::own_as("reads_file", serde_json::json!({"path": path})).unwrap_or_default()
}

/// The edit that sorts the methods of the class at or around `line` of a text in `language` by name.
#[tauri::command]
fn code_sort(language: String, text: String, line: u32) -> Result<serde_json::Value, String> {
    orior::own("code_sort", serde_json::json!({"language": language, "text": text, "line": line}))
}

/// The reader's templates, and the folder that holds them.
#[tauri::command]
fn templates_list() -> Result<serde_json::Value, String> {
    orior::own("templates_list", serde_json::json!({}))
}

/// Begins a project named `name` in `parent` from the template `template`. Gives its folder.
#[tauri::command(async)]
fn project_create(template: String, parent: String, name: String) -> Result<String, String> {
    orior::own_as("project_create", serde_json::json!({"template": template, "parent": parent, "name": name}))
}

/// Opens the folder that holds the reader's templates, made where it is not there yet.
#[tauri::command]
fn templates_reveal() -> Result<(), String> {
    orior::own("templates_reveal", serde_json::json!({})).map(|_| ())
}

/// Every checker the manifest knows: its id, its name, and the languages it checks.
#[tauri::command]
fn checkers_known() -> Result<serde_json::Value, String> {
    orior::own("checkers_known", serde_json::json!({}))
}

/// The classes a parse names, by their index.
#[tauri::command]
fn parse_classes() -> Result<serde_json::Value, String> {
    orior::own("parse_classes", serde_json::json!({}))
}

/// The text of the file at `path`, anywhere on the machine, as the reader chose it.
#[tauri::command(async)]
fn file_read_any(path: String) -> Result<String, String> {
    orior::own_as("file_read_any", serde_json::json!({"path": path}))
}

/// Writes `text` to the file at `path`, anywhere on the machine, as the reader chose it, making the
/// folders it stands in where they are not there.
#[tauri::command(async)]
fn file_write_any(path: String, text: String) -> Result<(), String> {
    orior::own("file_write_any", serde_json::json!({"path": path, "text": text})).map(|_| ())
}

/// Every language a formatter formats.
#[tauri::command]
fn format_languages() -> Vec<String> {
    orior::own_as("format_languages", serde_json::json!({})).unwrap_or_default()
}

/// Opens an https page a hover or a diagnostic links to in the browser.
#[tauri::command]
fn link_open(url: String) -> Result<(), String> {
    orior::own("link_open", serde_json::json!({"url": url})).map(|_| ())
}

/// Opens a toolchain's install page in the browser, and names it.
#[tauri::command]
fn toolchain_install(id: String) -> Result<String, String> {
    orior::own_as("toolchain_install", serde_json::json!({"id": id}))
}

/// What File, Clone Repository starts from: orior's own repository and the reader's home.
#[tauri::command]
fn clone_start() -> serde_json::Value {
    orior::own_as("clone_start", serde_json::json!({})).unwrap_or_default()
}

/// Clones `url` into a new folder under `parent`, telling the page each line of git's progress as
/// "clone-progress", and answers the folder made. `parent` is a full path: the window has no working
/// folder a reader knows of for a relative one to start from.
#[tauri::command(async)]
fn repo_clone(url: String, parent: String) -> Result<String, String> {
    orior::own_as("repo_clone", serde_json::json!({"url": url, "parent": parent}))
}

/// The folder File, Open, Repository clones into, made where it is not there yet.
#[tauri::command]
fn repos_folder() -> Result<String, String> {
    orior::own_as("repos_folder", serde_json::json!({}))
}

/// The folder File, Open, Repository cloned `url` into before, or null where it has not.
#[tauri::command]
fn repo_opened(url: String) -> Option<String> {
    orior::own_as("repo_opened", serde_json::json!({"url": url})).unwrap_or_default()
}

/// Every plugin, as plugins.c finds them.
#[tauri::command]
fn plugins_read() -> serde_json::Value {
    orior::own_as("plugins_read", serde_json::json!({})).unwrap_or_default()
}

/// The code the script plugin `id` runs.
#[tauri::command]
fn plugin_code(id: String) -> Result<String, String> {
    orior::own_as("plugin_code", serde_json::json!({"id": id}))
}

/// orior's own folder, where it keeps what it keeps and the reader's plugins are.
#[tauri::command]
fn home_folder() -> Result<String, String> {
    orior::own_as("home_folder", serde_json::json!({}))
}

/// The plugin the generator's answers ask for, as its file would hold it.
#[tauri::command]
fn plugin_draft(spec: serde_json::Value) -> Result<String, String> {
    orior::own_as("plugin_draft", serde_json::json!({"spec": spec}))
}

/// Writes the plugin the answers ask for to the reader's plugins folder, and names the folder.
#[tauri::command]
fn plugin_create(spec: serde_json::Value, replace: bool) -> Result<String, String> {
    orior::own_as("plugin_create", serde_json::json!({"spec": spec, "replace": replace}))
}

/// The reader's stylesheet, or nothing where there is none.
#[tauri::command]
fn user_css_read() -> String {
    orior::own_as("user_css_read", serde_json::json!({})).unwrap_or_default()
}

/// The reader's menus and toolbar, as menus.json holds them, or nothing where there is none.
#[tauri::command]
fn reader_menus_read() -> String {
    orior::own_as("reader_menus_read", serde_json::json!({})).unwrap_or_default()
}

/// Opens one of orior's own places as the system opens it: "user-css", made first where it is not
/// there, "menus", the reader's menus and toolbar, made first likewise, "settings", the reader's
/// settings.json, made first likewise, "plugins", the reader's plugins folder, made first likewise,
/// or the folder of one of the reader's plugins.
#[tauri::command]
fn home_reveal(what: String) -> Result<(), String> {
    orior::own("home_reveal", serde_json::json!({"what": what})).map(|_| ())
}

/// The whole lines from about `start` to about `end`, sent as bytes and not as JSON: the slice's
/// start, end and the file's size, each 8 bytes little-endian, then the text.
#[tauri::command(async)]
fn file_slice(app: State<App>, path: String, start: u64, end: u64) -> Result<tauri::ipc::Response, String> {
    struct Slice {
        start: u64,
        end: u64,
        size: u64,
        text: String,
    }
    let _ = app;
    let read = orior::tree("file_slice", serde_json::json!({"path": path, "start": start, "end": end}))?;
    let number = |key: &str| read[key].as_u64().unwrap_or(0);
    let slice = Slice { start: number("start"), end: number("end"), size: number("size"), text: read["text"].as_str().unwrap_or("").to_string() };
    let mut sent = Vec::with_capacity(24 + slice.text.len());
    sent.extend_from_slice(&slice.start.to_le_bytes());
    sent.extend_from_slice(&slice.end.to_le_bytes());
    sent.extend_from_slice(&slice.size.to_le_bytes());
    sent.extend_from_slice(slice.text.as_bytes());
    Ok(tauri::ipc::Response::new(sent))
}

/// The system's picker, opened from Rust so the page needs no script of the picker's own. Async,
/// because a picker that blocks the main thread never draws.
#[tauri::command]
async fn pick(handle: AppHandle, kind: String) -> Option<String> {
    use tauri_plugin_dialog::DialogExt;
    let dialog = handle.dialog().file();
    let chosen = match kind.as_str() {
        "dir" | "save-dir" => dialog.blocking_pick_folder(),
        "save" => dialog.blocking_save_file(),
        _ => dialog.blocking_pick_file(),
    }?;
    chosen.into_path().ok().map(|path| path.to_string_lossy().into_owned())
}

/// The address a page under the tree loads from: the `view` scheme, which Windows reaches as an
/// http host and the other platforms by the scheme's own name.
fn view_address(path: &str) -> Result<tauri::Url, String> {
    let encoded: String = path
        .split('/')
        .map(|part| {
            part.bytes()
                .map(|b| match b {
                    b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => (b as char).to_string(),
                    _ => format!("%{b:02X}"),
                })
                .collect::<String>()
        })
        .collect::<Vec<_>>()
        .join("/");
    let base = if cfg!(windows) { "http://view.localhost/" } else { "view://localhost/" };
    tauri::Url::parse(&format!("{base}{encoded}")).map_err(|e| e.to_string())
}

/// Opens a page the tree holds in a window of its own. Async, because a window made from a command
/// that blocks the main thread never draws on Windows.
#[tauri::command]
async fn view_open(handle: AppHandle, path: String) -> Result<(), String> {
    let app = handle.state::<App>();
    let file = PathBuf::from(orior::own_as::<Vec<String>>("tree_inside", serde_json::json!({"paths": [&path]}))?.into_iter().next().unwrap_or_default());
    if !file.is_file() {
        return Err(format!("{path} is not a file"));
    }
    let label = format!("view-{}", app.windows.fetch_add(1, Ordering::SeqCst));
    WebviewWindowBuilder::new(&handle, label, WebviewUrl::CustomProtocol(view_address(&path)?))
        .title(&path)
        .inner_size(1280.0, 860.0)
        .build()
        .map(|_| ())
        .map_err(|e| e.to_string())
}

/// The bytes base64 `text` gives, the standard letters with `=` to pad, whitespace passed over; None
/// where it is not base64.
fn unbase64(text: &str) -> Option<Vec<u8>> {
    let mut bytes = Vec::with_capacity(text.len() / 4 * 3);
    let mut held: u32 = 0;
    let mut count = 0;
    for letter in text.bytes() {
        let value = match letter {
            b'A'..=b'Z' => letter - b'A',
            b'a'..=b'z' => letter - b'a' + 26,
            b'0'..=b'9' => letter - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            b'=' | b' ' | b'\t' | b'\r' | b'\n' => continue,
            _ => return None,
        };
        held = (held << 6) | u32::from(value);
        count += 1;
        if count == 4 {
            bytes.extend_from_slice(&[(held >> 16) as u8, (held >> 8) as u8, held as u8]);
            held = 0;
            count = 0;
        }
    }
    match count {
        0 => {}
        2 => bytes.push((held >> 4) as u8),
        3 => bytes.extend_from_slice(&[(held >> 10) as u8, (held >> 2) as u8]),
        _ => return None,
    }
    Some(bytes)
}

fn decoded(path: &str) -> String {
    let bytes = path.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut at = 0;
    while at < bytes.len() {
        if bytes[at] == b'%' && at + 2 < bytes.len() {
            if let Some(value) = std::str::from_utf8(&bytes[at + 1..at + 3]).ok().and_then(|h| u8::from_str_radix(h, 16).ok()) {
                out.push(value);
                at += 3;
                continue;
            }
        }
        out.push(bytes[at]);
        at += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn media_type(path: &str) -> &'static str {
    match path.rsplit('.').next().unwrap_or("") {
        "html" | "htm" => "text/html; charset=utf-8",
        "js" | "mjs" => "text/javascript; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "json" => "application/json",
        "svg" => "image/svg+xml",
        "png" => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "csv" | "tsv" | "txt" => "text/plain; charset=utf-8",
        "wav" => "audio/wav",
        _ => "application/octet-stream",
    }
}

/// Serves a file under the tree to a page window, and nothing outside it.
fn view_scheme(context: UriSchemeContext<'_, tauri::Wry>, request: Request<Vec<u8>>) -> Response<Cow<'static, [u8]>> {
    let refuse = |status: StatusCode, said: String| {
        Response::builder().status(status).body(Cow::Owned(said.into_bytes())).expect("a plain response")
    };
    let app = context.app_handle().state::<App>();
    let path = decoded(request.uri().path().trim_start_matches('/'));
    if linked().is_some() {
        return match orior::tree("file_bytes", serde_json::json!({"path": path})).ok().and_then(|read| unbase64(read.as_str().unwrap_or(""))) {
            Some(bytes) => Response::builder().header("Content-Type", media_type(&path)).body(Cow::Owned(bytes)).expect("a file response"),
            None => refuse(StatusCode::NOT_FOUND, format!("{path} is not a file")),
        };
    }
    let Ok(_root) = root_of(&app) else { return refuse(StatusCode::NOT_FOUND, "no tree".into()) };
    let file = match orior::own_as::<Vec<String>>("tree_inside", serde_json::json!({"paths": [&path]})).map(|full| PathBuf::from(full.into_iter().next().unwrap_or_default())) {
        Ok(file) if file.is_file() => file,
        Ok(_) => return refuse(StatusCode::NOT_FOUND, format!("{path} is not a file")),
        Err(said) => return refuse(StatusCode::FORBIDDEN, said),
    };
    match std::fs::read(&file) {
        Ok(bytes) => Response::builder()
            .header("Content-Type", media_type(&path))
            .body(Cow::Owned(bytes))
            .expect("a file response"),
        Err(error) => refuse(StatusCode::NOT_FOUND, error.to_string()),
    }
}

/// The program: the command line where it is given words, else the window. Returns the code to
/// exit with.
pub fn start() -> i32 {
    // A panic of the window's files as an error does, its category by where its backtrace enters orior's code.
    let shown = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        shown(info);
        let detail = std::backtrace::Backtrace::force_capture().to_string();
        let _ = orior::own("report_panic", serde_json::json!({"message": info.to_string(), "detail": detail}));
    }));
    let words: Vec<String> = std::env::args().skip(1).collect();
    let (code, window) = orior::command_line(&words);
    if window {
        open();
        return 0;
    }
    code
}

/// How long the window waits for its page before it shows regardless.
const SHOW_ANYWAY: std::time::Duration = std::time::Duration::from_secs(4);

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    open();
}

/// Puts the window where `kept` says, fitted to the monitors there are now, the main one first.
fn put_back(window: &tauri::WebviewWindow, kept: place::Place) {
    let area = |monitor: &tauri::Monitor| place::Area { x: monitor.position().x, y: monitor.position().y, width: monitor.size().width, height: monitor.size().height };
    let main = window.primary_monitor().ok().flatten();
    let mut areas: Vec<place::Area> = main.iter().map(area).collect();
    for one in window.available_monitors().unwrap_or_default().iter().map(area) {
        if !areas.contains(&one) {
            areas.push(one);
        }
    }
    let Some(fitted) = place::fit(kept, &areas) else { return };
    let _ = window.set_size(tauri::PhysicalSize::new(fitted.width, fitted.height));
    let _ = window.set_position(tauri::PhysicalPosition::new(fitted.x, fitted.y));
    if fitted.maximized {
        let _ = window.maximize();
    }
}

/// Opens the window on the tree the launch names, else the one `cli_tree_found` finds, to run the
/// launch's command once the page is up.
fn open() {
    let app = App::default();
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri::plugin::Builder::<tauri::Wry, ()>::new("kept").js_init_script(kept_script()).build())
        .manage(app)
        .register_uri_scheme_protocol("view", view_scheme)
        .setup(|app| {
            // orior's side: the tree open, its jobs, its servers and its debugger, what it tells sent to the page.
            orior::begun(app.handle().clone());
            // The web view's own ground, which shows between one page and the next, is the page's.
            // A page that never asks for the window still has it shown after SHOW_ANYWAY.
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_background_color(Some(tauri::window::Color(0x13, 0x13, 0x31, 0xff)));
                // The system's own title bar where the reader asked for it, set before the window shows.
                if system_title_bar() {
                    let _ = window.set_decorations(true);
                }
                // The tree's window where it was left, fitted to the monitors there are now.
                if let Ok(tree) = root_of(&app.state::<App>()) {
                    if let Some(kept) = place::kept(&tree) {
                        put_back(&window, kept);
                    }
                }
                std::thread::spawn(move || {
                    std::thread::sleep(SHOW_ANYWAY);
                    let _ = window.show();
                });
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            call,
            document_read,
            document_part,
            document_edit,
            document_release,
            document_changes,
            document_find,
            remote_get,
            remote_rejoin,
            devcontainer_line,
            plugins_read,
            plugin_code,
            home_folder,
            plugin_draft,
            plugin_create,
            user_css_read,
            reader_menus_read,
            workspace_read,
            home_reveal,
            format_languages,
            parse_classes,
            code_docstring,
            checkers_known,
            code_sort,
            fill_paragraph,
            reads_file,
            templates_list,
            project_create,
            templates_reveal,
            inspect_languages,
            file_read_any,
            file_write_any,
            toolchain_install,
            link_open,
            clone_start,
            repo_clone,
            term_open,
            term_write,
            term_resize,
            term_close,
            clip_read,
            clip_write,
            commands_read,
            launch_take,
            window_open,
            app_version,
            app_changes,
            app_exit,
            window_show,
            memory_use,
            memory_hold,
            memory_calls,
            printers_list,
            print_page,
            kept_write,
            settings_layers,
            image_data,
            network_hold,
            network_reached,
            plugins_catalog_read,
            language_read,
            languages_list,
            plugins_catalog_install,
            plugin_request,
            carry_send,
            carry_read,
            carry_take,
            carry_takings,
            carry_undo,
            kept_now,
            plugin_settings_read,
            plugin_settings_write,
            files_copy,
            clip_files,
            files_paste,
            zoom_set,
            report_error,
            report_bug,
            report_auto,
            report_auto_set,
            report_asked,
            report_open,
            structure_compare,
            highlight_grammar,
            highlight_lines,
            scrollback_open,
            scrollback_keep,
            scrollback_read,
            scrollback_close,
            scrollback_reset,
            repos_folder,
            repo_opened,
            window_act,
            file_slice,
            view_open,
            pick,
        ])
        .on_window_event(|window, event| {
            // The tree's window kept where it stands as it is asked to close.
            if let (tauri::WindowEvent::CloseRequested { .. }, "main") = (event, window.label()) {
                if let (Ok(tree), Ok(at), Ok(size)) = (root_of(&window.state::<App>()), window.outer_position(), window.inner_size()) {
                    let maximized = window.is_maximized().unwrap_or(false);
                    if let Err(said) = place::keep(&tree, place::Place { x: at.x, y: at.y, width: size.width, height: size.height, maximized }) {
                        eprintln!("orior: the window's place: {said}");
                    }
                }
            }
        })
        .build(tauri::generate_context!())
        .expect("the app failed to start")
        .run(|_handle, event| {
            // A language server orior started ends with it: on Windows a child outlives its parent.
            if let tauri::RunEvent::Exit = event {
                orior::ended();
            }
        });
}

#[cfg(test)]
mod addresses {
    #[test]
    fn a_page_path_round_trips() {
        let path = "build/ui/views/3/a view#1.html";
        let address = super::view_address(path).unwrap();
        assert_eq!(super::decoded(address.path().trim_start_matches('/')), path);
    }
}
