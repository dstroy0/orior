// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the window, a layer on the orior-cli crate. Every job, every reading of the tree and
//! every command of the menus is that crate's; this layer adds the window, the commands its page
//! calls, the `view` scheme its page windows load from, the terminal's pseudo-terminals and the
//! clipboard. The same program is the command line, handing it any words it is started with.

mod allocations;
mod clip;
mod dragging;
mod memory;
mod printing;

#[global_allocator]
static ALLOCATOR: allocations::Counting = allocations::Counting;
mod scrollback;
mod terminal;

use orior_cli::cli::{self, Launch, Outcome};
use orior_cli::link::{Address, Link};
use orior_cli::serve::Server;
use orior_cli::{commands, files, format, git, home, kept, plugins, report, root, servers, toolchains};

use std::borrow::Cow;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex, OnceLock};

use tauri::http::{Request, Response, StatusCode};
use tauri::{AppHandle, Emitter, Manager, State, UriSchemeContext, WebviewUrl, WebviewWindowBuilder};

#[derive(Default)]
struct App {
    /// The tree's side: the tree open, its jobs, its servers and its debugger.
    server: Arc<Server>,
    /// The command the command line opened the window to run, until the page takes it.
    launch: Mutex<Option<Launch>>,
    terms: terminal::Terms,
    scrollbacks: scrollback::Scrollbacks,
    windows: AtomicU64,
    /// The link to the tree's side on another machine, where the tree is there.
    link: OnceLock<Arc<Link>>,
}

fn root_of(app: &App) -> Result<PathBuf, String> {
    app.server.root()
}

/// Refuses what only a tree on this machine does, the system's clipboard of files being this
/// machine's.
fn local_only(app: &App) -> Result<(), String> {
    match app.link.get() {
        Some(link) => Err(format!("files are cut, copied and pasted on this machine, and the tree is on {}", link.address.machine())),
        None => Ok(()),
    }
}

/// Answers a command of the tree's side by its name, as serve.rs answers it.
#[tauri::command(async)]
fn call(app: State<App>, name: String, args: serde_json::Value) -> Result<serde_json::Value, String> {
    allocations::measured(&name, || match app.link.get() {
        Some(link) => link.call(&name, args),
        None => app.server.call(&name, args),
    })
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
    app.link.get().map(|link| link.standing())
}

/// Lets go of the link to the tree's machine, for it to be made again.
#[tauri::command]
fn remote_rejoin(app: State<App>) {
    if let Some(link) = app.link.get() {
        link.drop_now();
    }
}

/// The shell line that brings the tree's dev container up and opens a window on the tree in it, for
/// the terminal to run.
#[tauri::command]
fn devcontainer_line(app: State<App>) -> Result<String, String> {
    let root = root_of(&app)?;
    orior_cli::devcontainer::plan(&root)?;
    let program = std::env::current_exe().map_err(|error| error.to_string())?;
    let shell = |path: &Path| orior_cli::link::quote(&path.display().to_string().replace('\\', "/"));
    Ok(format!("{} --root {} file dev-container", shell(&program), shell(&root)))
}

/// Opens a terminal at the tree's top folder, or at the home folder where no tree is open.
#[tauri::command]
fn term_open(handle: AppHandle, app: State<App>, cols: u16, rows: u16, at: Option<String>) -> Result<u64, String> {
    if let Some(link) = app.link.get() {
        let folder = at.map(|at| terminal::folder_of(&at).to_string_lossy().replace('\\', "/"));
        let (program, words) = link.address.runs(&link.address.shell_line(folder.as_deref()), true);
        // The program as a file on the PATH: on Windows the pseudo-terminal runs the first file of the
        // name it finds, and docker's folder holds a script named docker beside docker.exe.
        let found = orior_cli::toolchains::path_folders().iter().find_map(|dir| orior_cli::toolchains::program_in(dir, std::slice::from_ref(&program)));
        let mut command = portable_pty::CommandBuilder::new(found.map_or(program, |path| path.display().to_string()));
        command.args(words);
        command.env("TERM", "xterm-256color");
        return app.terms.open(handle, command, cols, rows);
    }
    let home = std::env::var_os(if cfg!(windows) { "USERPROFILE" } else { "HOME" }).map(PathBuf::from);
    let kept = at.map(|at| terminal::folder_of(&at)).filter(|folder| folder.is_dir());
    let at = kept.or_else(|| root_of(&app).ok()).or(home).or_else(|| std::env::current_dir().ok()).ok_or("no folder to open a terminal in")?;
    app.terms.open(handle, terminal::shell(&at)?, cols, rows)
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
fn commands_read() -> &'static str {
    commands::TEXT
}

/// The command the window was opened to run, once.
#[tauri::command]
fn launch_take(app: State<App>) -> Option<Launch> {
    app.launch.lock().ok()?.take().filter(|launch| !launch.command.is_empty())
}

/// The app's version, as its package gives it.
#[tauri::command]
fn app_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

/// Ends the app, every window of it, from the File menu.
/// What the app holds in memory, its own process and every one it started, or nothing where the
/// system does not say.
#[tauri::command]
fn memory_use(app: State<App>) -> Option<memory::Memory> {
    let mut read = memory::read()?;
    named(&mut read, &app.server.servers().running());
    Some(read)
}

// Names each part of a reading that is a language server by its program, with its toolchain's id,
// and the web view's as the web view.
fn named(read: &mut memory::Memory, running: &[servers::Running]) {
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
    let servers = app.server.servers();
    if servers.checking() {
        return Held { stopped: Vec::new(), files: Vec::new() };
    }
    let running = servers.running();
    let needed = servers::Servers::tools_for(&open);
    let mut stopped: Vec<String> = running.iter().filter(|one| !needed.contains(&one.tool)).map(|one| one.tool.clone()).collect();
    if stopped.is_empty() {
        let keep = servers::Servers::tools_for(&shown.into_iter().collect::<Vec<_>>());
        if let Some(mut read) = memory::read().filter(|read| read.working > budget) {
            named(&mut read, &running);
            stopped.extend(read.parts.iter().filter_map(|part| part.server.clone().filter(|tool| !keep.contains(tool))).take(1));
        }
    }
    let names = stopped.iter().map(|tool| running.iter().find(|one| &one.tool == tool).map_or_else(|| tool.clone(), |one| one.program.clone())).collect();
    let files = servers.stop(&stopped);
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
    arboard::Clipboard::new().and_then(|mut clip| clip.get_text()).unwrap_or_default()
}

/// Puts text on the system clipboard, written here so the page never asks the reader for leave to
/// write it.
#[tauri::command]
fn clip_write(text: String) -> Result<(), String> {
    arboard::Clipboard::new().and_then(|mut clip| clip.set_text(text)).map_err(|error| error.to_string())
}

/// Keeps the page's changed entries in orior's own folder, each key starting `orior.` with its new
/// text, or null where it is gone.
#[tauri::command(async)]
fn kept_write(changes: std::collections::BTreeMap<String, Option<String>>) -> Result<(), String> {
    let folder = home::folder().ok_or("orior has no folder of its own to keep its entries in")?;
    let changes = changes.into_iter().filter_map(|(key, text)| Some((key.strip_prefix("orior.")?.to_string(), text))).collect();
    kept::keep(&folder, &changes)
}

/// The script that puts what orior's folder keeps into the page's storage before the page's own
/// scripts run, on the first load of the main window's page in a run, in place of every entry the
/// page kept before. Where the folder keeps nothing yet, it tells the page to write all it holds
/// there.
fn kept_script() -> String {
    let kept = home::folder().and_then(|folder| kept::read(&folder)).map(|entries| entries.into_iter().map(|(key, text)| (format!("orior.{key}"), text)).collect::<std::collections::BTreeMap<_, _>>());
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
    let root = root_of(&app)?;
    let full = paths.iter().map(|path| root::inside(&root, path)).collect::<Result<Vec<_>, _>>()?;
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
fn files_paste(app: State<App>, into: String) -> Result<Vec<files::Pasted>, String> {
    local_only(&app)?;
    let root = root_of(&app)?;
    let Some((paths, cut)) = clip::held() else {
        return Ok(Vec::new());
    };
    let pasted = files::paste(&root, &into, &paths, cut)?;
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
async fn report_error(app: State<'_, App>, category: String, message: String, detail: String) -> Result<Option<report::Filed>, String> {
    let root = app.server.root_now();
    tauri::async_runtime::spawn_blocking(move || report::error(&category, &message, &detail, root.as_deref())).await.map_err(|e| e.to_string())
}

/// Files a bug report the reader wrote, with the run's recent errors where they asked for them.
#[tauri::command]
async fn report_bug(app: State<'_, App>, report: report::Report, with_errors: bool) -> Result<report::Filed, String> {
    let root = app.server.root_now();
    let mut report = report;
    if with_errors {
        let errors = report::recent();
        report.logs = [report.logs.trim(), errors.trim()].iter().filter(|part| !part.is_empty()).copied().collect::<Vec<_>>().join("

");
    }
    tauri::async_runtime::spawn_blocking(move || report::file(&report, root.as_deref())).await.map_err(|e| e.to_string())
}

#[tauri::command]
fn report_auto() -> bool {
    report::auto()
}

/// Whether the question of whether errors file on their own needs no asking, as the reporter has
/// answered it or ORIOR_NO_REPORTS turned reports off for the run, and the question.
#[tauri::command]
fn report_asked() -> (bool, &'static str) {
    (report::asked() || std::env::var_os("ORIOR_NO_REPORTS").is_some(), report::QUESTION)
}

#[tauri::command]
fn report_auto_set(on: bool) -> Result<(), String> {
    report::set_auto(on)
}

#[tauri::command]
fn report_open(url: String) -> Result<(), String> {
    report::open_page(&url)
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
    home::folder().and_then(|folder| kept::read(&folder)).and_then(|entries| entries.get("titlebar").cloned()).as_deref() == Some("system")
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
    let mut command = std::process::Command::new(program);
    match (root, remote.or_else(|| app.link.get().map(|link| link.address.text()))) {
        (None, Some(remote)) => {
            Address::parse(&remote)?;
            command.arg("--remote").arg(remote);
        }
        (Some(root), _) => {
            command.arg("--root").arg(orior_cli::cli::tree(Some(&root))?);
        }
        (None, None) => {
            command.arg("--root").arg(root_of(&app)?);
        }
    }
    command.args(&words).stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());
    orior_cli::runner::quiet(&mut command);
    command.spawn().map(|_| ()).map_err(|error| error.to_string())
}

/// Reads a grammar of the page's and keeps it under `key` for the lines colored with it, or says what
/// in it is not read here.
#[tauri::command]
fn highlight_grammar(key: String, def: serde_json::Value) -> Result<(), String> {
    orior_cli::highlight::keep(&key, &def)
}

/// Colors lines with the grammar kept under `key`, the first starting in `state`.
#[tauri::command(async)]
fn highlight_lines(key: String, state: String, lines: Vec<String>) -> Result<orior_cli::highlight::Colored, String> {
    orior_cli::highlight::color(&key, &state, &lines)
}

/// Two texts compared by structure, the older first, or null where they are too far apart.
#[tauri::command(async)]
fn structure_compare(then: Option<String>, now: String) -> Option<orior_cli::structure::Compared> {
    orior_cli::structure::compare(then.as_deref().unwrap_or(""), &now)
}

/// The folders a .code-workspace file names, each made whole against the file's own folder, as JSON
/// with comments and trailing commas reads them.
#[tauri::command]
fn workspace_read(path: String) -> Result<Vec<String>, String> {
    let file = PathBuf::from(&path);
    let text = std::fs::read_to_string(&file).map_err(|error| format!("{path}: {error}"))?;
    let value: serde_json::Value = serde_json::from_str(&orior_cli::devcontainer::plain_json(&text)).map_err(|error| format!("{path}: {error}"))?;
    let base = file.parent().map(Path::to_path_buf).unwrap_or_default();
    Ok(value["folders"]
        .as_array()
        .into_iter()
        .flatten()
        .filter_map(|folder| folder["path"].as_str())
        .map(|folder| {
            let named = PathBuf::from(folder);
            let whole = if named.is_absolute() { named } else { base.join(named) };
            dunce::canonicalize(&whole).unwrap_or(whole).display().to_string()
        })
        .collect())
}

/// The languages orior's own inspections read, by the editor's names for them.
#[tauri::command]
fn inspect_languages() -> Vec<&'static str> {
    orior_cli::inspect::LANGUAGES.to_vec()
}

/// A docstring drawn up for the function at or around `line` of a text in `language`, in `form`:
/// its edit, and the place to write its summary.
#[tauri::command]
fn code_docstring(language: String, text: String, line: u32, form: String) -> Result<(servers::TextEdit, servers::Place), String> {
    orior_cli::inspect::docstring(&language, &text, line, &form)
}

/// The edit that fills the paragraph of documentation at `line` of a text to `width` columns.
#[tauri::command]
fn fill_paragraph(text: String, line: u32, width: usize) -> Result<servers::TextEdit, String> {
    orior_cli::fill::fill(&text, line, width.max(20))
}

/// Whether orior reads the file at `path` of the tree itself: a build file, a Gradle script or
/// catalog, a POM, an SConstruct or an SConscript, or JSON, read by its schema.
#[tauri::command]
fn reads_file(path: String) -> bool {
    let path = std::path::Path::new(&path);
    orior_cli::builds::kind_of(path).is_some() || orior_cli::schema::reads(path)
}

/// The edit that sorts the methods of the class at or around `line` of a text in `language` by name.
#[tauri::command]
fn code_sort(language: String, text: String, line: u32) -> Result<servers::TextEdit, String> {
    orior_cli::inspect::sort_methods(&language, &text, line)
}

/// The reader's templates, and the folder that holds them.
#[tauri::command]
fn templates_list() -> (Vec<String>, String) {
    (orior_cli::templates::list(), orior_cli::templates::folder().map(|folder| folder.display().to_string()).unwrap_or_default())
}

/// Begins a project named `name` in `parent` from the template `template`. Gives its folder.
#[tauri::command(async)]
fn project_create(template: String, parent: String, name: String) -> Result<String, String> {
    orior_cli::templates::create(&template, Path::new(&parent), &name).map(|made| made.display().to_string())
}

/// Opens the folder that holds the reader's templates, made where it is not there yet.
#[tauri::command]
fn templates_reveal() -> Result<(), String> {
    let folder = orior_cli::templates::folder().ok_or("orior has no folder of its own to keep templates in")?;
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    home::reveal(&folder)
}

/// Every checker the manifest knows: its id, its name, and the languages it checks.
#[tauri::command]
fn checkers_known() -> Vec<(String, String, Vec<String>)> {
    orior_cli::checkers::known().into_iter().map(|one| (one.id, one.name, one.spec.languages)).collect()
}

/// The classes a parse names, by their index.
#[tauri::command]
fn parse_classes() -> Vec<&'static str> {
    orior_cli::inspect::CLASSES.to_vec()
}

/// The text of the file at `path`, anywhere on the machine, as the reader chose it.
#[tauri::command(async)]
fn file_read_any(path: String) -> Result<String, String> {
    std::fs::read_to_string(&path).map_err(|error| format!("{path}: {error}"))
}

/// Writes `text` to the file at `path`, anywhere on the machine, as the reader chose it.
#[tauri::command(async)]
fn file_write_any(path: String, text: String) -> Result<(), String> {
    std::fs::write(&path, text).map_err(|error| format!("{path}: {error}"))
}

/// Every language a formatter formats.
#[tauri::command]
fn format_languages() -> Vec<String> {
    format::languages()
}

/// Opens an https page a hover or a diagnostic links to in the browser.
#[tauri::command]
fn link_open(url: String) -> Result<(), String> {
    if !url.starts_with("https://") {
        return Err(format!("{url} is not an https page"));
    }
    orior_cli::report::open_url(&url)
}

/// Opens a toolchain's install page in the browser, and names it.
#[tauri::command]
fn toolchain_install(id: String) -> Result<String, String> {
    toolchains::open_install(&id)
}

/// What File, Clone Repository starts from: orior's own repository and the reader's home.
#[tauri::command]
fn clone_start() -> serde_json::Value {
    serde_json::json!({ "url": git::ORIOR, "parent": git::clone_parent().display().to_string() })
}

/// Clones `url` into a new folder under `parent`, telling the page each line of git's progress as
/// "clone-progress", and answers the folder made. `parent` is a full path: the window has no working
/// folder a reader knows of for a relative one to start from.
#[tauri::command(async)]
fn repo_clone(handle: AppHandle, url: String, parent: String) -> Result<String, String> {
    if !Path::new(&parent).is_absolute() {
        return Err(format!("{parent} is not a full path, such as one Choose Folder gives"));
    }
    let target = git::clone_folder(&url, Path::new(&parent))?;
    git::clone(&url, &target, |line, _| {
        let _ = handle.emit("clone-progress", line);
    })
    .map(|dir| dir.display().to_string())
}

/// The folder File, Open, Repository clones into, made where it is not there yet.
#[tauri::command]
fn repos_folder() -> Result<String, String> {
    let folder = git::opened_parent().ok_or("orior has no folder of its own here")?;
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    Ok(folder.display().to_string())
}

/// The folder File, Open, Repository cloned `url` into before, or null where it has not.
#[tauri::command]
fn repo_opened(url: String) -> Option<String> {
    let folder = git::clone_folder(&url, &git::opened_parent()?).ok()?;
    folder.join(".git").exists().then(|| folder.display().to_string())
}

/// Every plugin, as plugins.rs finds them.
#[tauri::command]
fn plugins_read() -> Vec<plugins::Plugin> {
    plugins::all()
}

/// The plugin the generator's answers ask for, as its file would hold it.
#[tauri::command]
fn plugin_draft(spec: plugins::Spec) -> Result<String, String> {
    plugins::draft(&spec).map(|plugin| plugins::text_of(&plugin))
}

/// Writes the plugin the answers ask for to the reader's plugins folder, and names the folder.
#[tauri::command]
fn plugin_create(spec: plugins::Spec, replace: bool) -> Result<String, String> {
    plugins::create(&spec, replace).map(|folder| folder.display().to_string())
}

/// The reader's stylesheet, or nothing where there is none.
#[tauri::command]
fn user_css_read() -> String {
    home::user_css().and_then(|path| std::fs::read_to_string(path).ok()).unwrap_or_default()
}

/// The reader's menus and toolbar, as menus.json holds them, or nothing where there is none.
#[tauri::command]
fn reader_menus_read() -> String {
    home::menus().and_then(|path| std::fs::read_to_string(path).ok()).unwrap_or_default()
}

/// Opens one of orior's own places as the system opens it: "user-css", made first where it is not
/// there, "menus", the reader's menus and toolbar, made first likewise, "plugins", the reader's
/// plugins folder, made first likewise, or the folder of one of the reader's plugins.
#[tauri::command]
fn home_reveal(what: String) -> Result<(), String> {
    let plugins_dir = home::plugins().ok_or("orior has no folder of its own")?;
    let path = match what.as_str() {
        "user-css" => home::ensure_user_css()?,
        "menus" => home::ensure_menus()?,
        "plugins" => {
            std::fs::create_dir_all(&plugins_dir).map_err(|error| format!("{}: {error}", plugins_dir.display()))?;
            plugins_dir
        }
        folder => {
            let path = PathBuf::from(folder);
            if !path.starts_with(&plugins_dir) || !path.is_dir() {
                return Err(format!("{folder} is not a plugin of the reader's"));
            }
            path
        }
    };
    home::reveal(&path)
}

/// The whole lines from about `start` to about `end`, sent as bytes and not as JSON: the slice's
/// start, end and the file's size, each 8 bytes little-endian, then the text.
#[tauri::command(async)]
fn file_slice(app: State<App>, path: String, start: u64, end: u64) -> Result<tauri::ipc::Response, String> {
    let slice = match app.link.get() {
        Some(link) => {
            let read = link.call("file_slice", serde_json::json!({"path": path, "start": start, "end": end}))?;
            {
                let number = |key: &str| read[key].as_u64().unwrap_or(0);
                files::Slice { start: number("start"), end: number("end"), size: number("size"), line: number("line"), lines: number("lines"), text: read["text"].as_str().unwrap_or("").to_string() }
            }
        }
        None => files::slice(&root_of(&app)?, &path, start, end)?,
    };
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
    let root = root_of(&app)?;
    let file = root::inside(&root, &path)?;
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
    if let Some(link) = app.link.get() {
        return match link.call("file_bytes", serde_json::json!({"path": path})).ok().and_then(|read| orior_cli::serve::unbase64(read.as_str().unwrap_or(""))) {
            Some(bytes) => Response::builder().header("Content-Type", media_type(&path)).body(Cow::Owned(bytes)).expect("a file response"),
            None => refuse(StatusCode::NOT_FOUND, format!("{path} is not a file")),
        };
    }
    let Ok(root) = root_of(&app) else { return refuse(StatusCode::NOT_FOUND, "no tree".into()) };
    let file = match root::inside(&root, &path) {
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
    report::catch_panics();
    let words: Vec<String> = std::env::args().skip(1).collect();
    if words.is_empty() {
        cli::console_let_go();
        open(Launch::default());
        return 0;
    }
    match cli::run(words) {
        Outcome::Exit(code) => code,
        Outcome::Window(launch) => {
            open(launch);
            0
        }
    }
}

/// How long the window waits for its page before it shows regardless.
const SHOW_ANYWAY: std::time::Duration = std::time::Duration::from_secs(4);

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    open(Launch::default());
}

/// Opens the window on the tree the launch names, else the one `root::find` finds, to run the
/// launch's command once the page is up.
fn open(launch: Launch) {
    let remote = launch.remote.as_deref().map(Address::parse).transpose().unwrap_or_else(|said| {
        eprintln!("orior: {said}");
        None
    });
    let root = if remote.is_some() { None } else { launch.root.clone().or_else(root::find) };
    let app = App { server: Arc::new(Server::new(root)), launch: Mutex::new(Some(launch)), ..App::default() };
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri::plugin::Builder::<tauri::Wry, ()>::new("kept").js_init_script(kept_script()).build())
        .manage(app)
        .register_uri_scheme_protocol("view", view_scheme)
        .setup(|app| {
            // The web view's own ground, which shows between one page and the next, is the page's.
            // A page that never asks for the window still has it shown after SHOW_ANYWAY.
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_background_color(Some(tauri::window::Color(0x13, 0x13, 0x31, 0xff)));
                // The system's own title bar where the reader asked for it, set before the window shows.
                if system_title_bar() {
                    let _ = window.set_decorations(true);
                }
                std::thread::spawn(move || {
                    std::thread::sleep(SHOW_ANYWAY);
                    let _ = window.show();
                });
            }
            let handle = app.handle().clone();
            let server = app.state::<App>().server.clone();
            server.tell_to(Arc::new(move |event, body| {
                let _ = handle.emit(event, body);
            }));
            server.watch();
            if let Some(address) = remote {
                let handle = app.handle().clone();
                let link = Link::open(address, Arc::new(move |event, body| {
                    let _ = handle.emit(event, body);
                }));
                let _ = app.state::<App>().link.set(link);
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            call,
            remote_get,
            remote_rejoin,
            devcontainer_line,
            plugins_read,
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
            app_exit,
            window_show,
            memory_use,
            memory_hold,
            memory_calls,
            printers_list,
            print_page,
            kept_write,
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
        .build(tauri::generate_context!())
        .expect("the app failed to start")
        .run(|handle, event| {
            // A language server orior started ends with it: on Windows a child outlives its parent.
            if let tauri::RunEvent::Exit = event {
                handle.state::<App>().server.stop_all();
                if let Some(link) = handle.state::<App>().link.get() {
                    link.end();
                }
            }
        });
}

#[cfg(test)]
mod workspaces {
    #[test]
    fn a_workspace_with_comments_and_trailing_commas_reads_as_json() {
        let text = "{\n  // the folders\n  \"folders\": [\n    { \"path\": \"a // not a comment\" }, /* one more */\n    { \"path\": \"../b\", },\n  ],\n}\n";
        let value: serde_json::Value = serde_json::from_str(&orior_cli::devcontainer::plain_json(text)).unwrap();
        assert_eq!(value["folders"][0]["path"], "a // not a comment");
        assert_eq!(value["folders"][1]["path"], "../b");
    }
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
