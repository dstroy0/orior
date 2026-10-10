// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the window, a layer on the orior-cli crate. Every job, every reading of the tree and
//! every command of the menus is that crate's; this layer adds the window, the commands its page
//! calls, the `view` scheme its page windows load from, the terminal's pseudo-terminals and the
//! clipboard. The same program is the command line, handing it any words it is started with.

mod clip;
mod dragging;
mod memory;
mod scrollback;
mod terminal;

use orior_cli::cli::{self, Launch, Outcome};
use orior_cli::{bridge, catalog, commands, debug, defs, files, format, git, history, home, kept, patterns, plugins, report, root, run_file, runner, servers, symbols, toolchains, validate, watch};

use std::borrow::Cow;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use tauri::http::{Request, Response, StatusCode};
use tauri::{AppHandle, Emitter, Manager, State, UriSchemeContext, WebviewUrl, WebviewWindowBuilder};

#[derive(Default)]
struct App {
    root: Mutex<Option<PathBuf>>,
    /// The command the command line opened the window to run, until the page takes it.
    launch: Mutex<Option<Launch>>,
    runs: runner::Runs,
    terms: terminal::Terms,
    scrollbacks: scrollback::Scrollbacks,
    servers: Arc<servers::Servers>,
    symbols: symbols::Index,
    debugger: Arc<debug::Debugger>,
    /// The tree's runs of its tests, the one going and the number of the last.
    tests: orior_cli::test_runs::Runs,
    /// The profile running, where one is.
    profiles: orior_cli::profile::Profiles,
    windows: AtomicU64,
    /// The watch over the tree open, which tells the page of changes made outside the window.
    watcher: Mutex<Option<watch::Watcher>>,
}

fn root_of(app: &App) -> Result<PathBuf, String> {
    app.root.lock().map_err(|e| e.to_string())?.clone().ok_or_else(|| "no orior tree is open".to_string())
}

/// The folder of a repository of the tree, by its path in the tree, or the tree's own folder where none
/// is named.
fn repo_of(app: &App, repo: Option<&str>) -> Result<PathBuf, String> {
    let root = root_of(app)?;
    match repo.filter(|repo| !repo.is_empty()) {
        Some(repo) => orior_cli::root::inside(&root, repo),
        None => Ok(root),
    }
}

#[tauri::command]
fn root_get(app: State<App>) -> Option<String> {
    app.root.lock().ok()?.as_ref().map(|p| p.to_string_lossy().into_owned())
}

/// Watches the tree open for changes made outside the window, each batch sent to the page as
/// tree-changed, in place of the watch over the tree open before.
fn watch_tree(handle: &AppHandle, app: &App) {
    let Ok(root) = root_of(app) else { return };
    let handle = handle.clone();
    let tree = root.clone();
    let watcher = watch::start(root, move |changed| {
        handle.state::<App>().servers.changed(&tree, &changed.files);
        let _ = handle.emit("tree-changed", changed);
    });
    if let Ok(mut held) = app.watcher.lock() {
        *held = Some(watcher);
    }
}

#[tauri::command]
fn root_set(handle: AppHandle, app: State<App>, path: String) -> Result<String, String> {
    let path = dunce::canonicalize(&path).map_err(|e| format!("{path}: {e}"))?;
    if !root::holds_tree(&path) {
        return Err(format!("{} holds no orior tree", path.display()));
    }
    let mut root = app.root.lock().map_err(|e| e.to_string())?;
    let moved = root.as_ref() != Some(&path);
    *root = Some(path.clone());
    drop(root);
    // The folders mounted beside one tree are its own; another tree mounts its own.
    if moved {
        root::unmount_all();
    }
    // A server answers for the tree it started in. Another tree starts its own as its files open.
    if moved {
        app.servers.let_go();
        app.symbols.forget();
        watch_tree(&handle, &app);
    }
    Ok(path.to_string_lossy().into_owned())
}

/// A job as the window lists it: the job, and how many steps it runs, which the run's fuse burns
/// through one at a time.
#[derive(serde::Serialize)]
struct Listed {
    #[serde(flatten)]
    job: catalog::Job,
    steps: usize,
}

#[tauri::command]
fn catalog_read(app: State<App>) -> Result<Vec<Listed>, String> {
    Ok(catalog::read(&root_of(&app)?).into_iter().map(|job| Listed { steps: job.steps.len(), job }).collect())
}

#[tauri::command]
fn definitions_read(app: State<App>) -> Result<defs::Definitions, String> {
    Ok(defs::read(&root_of(&app)?))
}

#[tauri::command]
fn job_start(handle: AppHandle, app: State<App>, job: String, values: HashMap<String, Vec<String>>) -> Result<u64, String> {
    let root = root_of(&app)?;
    let found = catalog::read(&root).into_iter().find(|j| j.id == job).ok_or_else(|| format!("no job {job}"))?;
    let sink: runner::Sink = Arc::new(move |said| {
        let _ = match said {
            runner::Said::Line(line) => handle.emit("run-line", line),
            runner::Said::End(end) => handle.emit("run-end", end),
        };
    });
    app.runs.start(sink, root, found, values).map(|(run, _)| run)
}

#[tauri::command]
fn job_stop(app: State<App>, run: u64) -> Result<(), String> {
    app.runs.stop(run)
}

/// Opens a terminal at the tree's top folder, or at the home folder where no tree is open.
#[tauri::command]
fn term_open(handle: AppHandle, app: State<App>, cols: u16, rows: u16, at: Option<String>) -> Result<u64, String> {
    let home = std::env::var_os(if cfg!(windows) { "USERPROFILE" } else { "HOME" }).map(PathBuf::from);
    let kept = at.map(|at| terminal::folder_of(&at)).filter(|folder| folder.is_dir());
    let at = kept.or_else(|| root_of(&app).ok()).or(home).or_else(|| std::env::current_dir().ok()).ok_or("no folder to open a terminal in")?;
    app.terms.open(handle, &at, cols, rows)
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
    named(&mut read, &app.servers.running());
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
    if app.servers.checking() {
        return Held { stopped: Vec::new(), files: Vec::new() };
    }
    let running = app.servers.running();
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
    let files = app.servers.stop(&stopped);
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

#[tauri::command]
fn tree_list(app: State<App>, dir: String) -> Result<Vec<files::Entry>, String> {
    files::list(&root_of(&app)?, &dir)
}

#[tauri::command]
fn tree_find(app: State<App>, query: String) -> Result<Vec<String>, String> {
    Ok(files::find(&root_of(&app)?, &query))
}

/// Every file of the tree.
#[tauri::command(async)]
fn tree_files(app: State<App>) -> Result<Vec<String>, String> {
    Ok(files::all(&root_of(&app)?))
}

/// The tree's files that answer what Go to File was given, at most `most`, the best first, those
/// in `recent` scored higher.
#[tauri::command(async)]
fn files_find(app: State<App>, query: String, recent: Vec<String>, most: usize) -> Result<Vec<files::Found>, String> {
    Ok(files::ranked(&root_of(&app)?, &query, &recent, most))
}

/// Every local and remote branch of the tree's repository.
#[tauri::command(async)]
fn git_branches(app: State<App>, repo: Option<String>) -> Result<Vec<git::Branch>, String> {
    Ok(git::branches(&repo_of(&app, repo.as_deref())?))
}

/// Creates, switches to, renames, deletes, merges or rebases onto a branch, as `act` says.
#[tauri::command(async)]
fn git_branch(app: State<App>, act: String, name: String, to: Option<String>, repo: Option<String>) -> Result<String, String> {
    git::branch_act(&repo_of(&app, repo.as_deref())?, &act, &name, to.as_deref().unwrap_or(""))
}

/// Commits whole files and files taken in part, each with the text the commit gives it.
#[tauri::command(async)]
fn git_commit_parts(app: State<App>, message: String, whole: Vec<String>, parts: Vec<git::Part>) -> Result<String, String> {
    git::commit_parts(&root_of(&app)?, &message, &whole, &parts)
}

/// Marks a file a merge left in conflict resolved.
#[tauri::command]
fn git_resolve(app: State<App>, path: String) -> Result<(), String> {
    git::resolve(&root_of(&app)?, &path)
}

/// The indentation the `.editorconfig` files over a file of the tree set for it.
#[tauri::command]
fn indent_for(app: State<App>, path: String) -> Result<orior_cli::editorconfig::Indent, String> {
    Ok(orior_cli::editorconfig::indent(&root::inside(&root_of(&app)?, &path)?))
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

/// Sets the patterns that keep a part of the window from files, and says whether they changed.
#[tauri::command]
fn patterns_set(part: patterns::Part, lines: Vec<String>) -> bool {
    let changed = patterns::set(part, &lines);
    if changed && matches!(part, patterns::Part::Search) {
        files::forget_held();
    }
    changed
}

/// Puts files and folders of the tree on the system clipboard, cut or copied, for another window of
/// orior or the system's file manager to paste.
#[tauri::command]
fn files_copy(app: State<App>, paths: Vec<String>, cut: bool) -> Result<(), String> {
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

/// The most declarations one search of the tree's symbols gives.
const SYMBOLS_MOST: usize = 200;

/// The declarations in the tree's files whose names answer `query`, the best first, and whether
/// the index was still being read. Each search brings the index up to date behind it.
#[tauri::command(async)]
fn symbols_find(app: State<App>, query: String) -> Result<symbols::Found, String> {
    let root = root_of(&app)?;
    let listed = root.clone();
    app.symbols.refresh(&root, move || files::all(&listed));
    Ok(app.symbols.find(&query, SYMBOLS_MOST))
}

/// Every line in the tree's files that holds the query, for Find in Files, in the set of files whose
/// patterns `set` gives where it gives any.
#[tauri::command]
async fn tree_search(app: State<'_, App>, query: String, how: files::Searching, set: Option<Vec<String>>) -> Result<Vec<files::Hit>, String> {
    let root = root_of(&app)?;
    let set = patterns::Patterns::read_set(&set.unwrap_or_default());
    tauri::async_runtime::spawn_blocking(move || files::search(&root, &query, how, &set)).await.map_err(|e| e.to_string())?
}

/// Sets the window's zoom, 1 being none.
#[tauri::command]
fn zoom_set(webview: tauri::Webview, factor: f64) -> Result<(), String> {
    webview.set_zoom(factor.clamp(0.5, 3.0)).map_err(|e| e.to_string())
}

#[tauri::command]
fn tree_changed(app: State<App>, repos: Option<Vec<String>>) -> Result<Vec<git::Changed>, String> {
    Ok(git::changed(&root_of(&app)?, repos.as_deref()))
}

/// The repositories of the tree, the one it is in first, each with the branch it is on.
#[tauri::command(async)]
fn git_repositories(app: State<App>) -> Result<Vec<git::Repository>, String> {
    Ok(git::repositories(&root_of(&app)?))
}

#[tauri::command]
fn tree_branch(app: State<App>, repo: Option<String>) -> Result<Option<String>, String> {
    Ok(git::branch(&repo_of(&app, repo.as_deref())?))
}

/// Files an error the window met, on its own where the reporter lets errors file. Answers where the
/// report went, or nothing.
#[tauri::command]
async fn report_error(app: State<'_, App>, category: String, message: String, detail: String) -> Result<Option<report::Filed>, String> {
    let root = app.root.lock().ok().and_then(|root| root.clone());
    tauri::async_runtime::spawn_blocking(move || report::error(&category, &message, &detail, root.as_deref())).await.map_err(|e| e.to_string())
}

/// Files a bug report the reader wrote, with the run's recent errors where they asked for them.
#[tauri::command]
async fn report_bug(app: State<'_, App>, report: report::Report, with_errors: bool) -> Result<report::Filed, String> {
    let root = app.root.lock().ok().and_then(|root| root.clone());
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

/// Whether the reporter has answered whether errors file on their own, and the question they are
/// asked where they have not.
#[tauri::command]
fn report_asked() -> (bool, &'static str) {
    (report::asked(), report::QUESTION)
}

#[tauri::command]
fn report_auto_set(on: bool) -> Result<(), String> {
    report::set_auto(on)
}

#[tauri::command]
fn report_open(url: String) -> Result<(), String> {
    report::open_page(&url)
}

/// A file's text as the last commit left it, for the editor's marks of what changed since.
#[tauri::command]
fn file_head(app: State<App>, path: String) -> Result<Option<String>, String> {
    Ok(git::head_text(&root_of(&app)?, &path))
}

#[tauri::command]
fn file_commits(app: State<App>, path: String) -> Result<Vec<git::Commit>, String> {
    git::commits(&root_of(&app)?, &path)
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
fn window_open(app: State<App>, root: Option<String>, words: Vec<String>) -> Result<(), String> {
    let root = match root {
        Some(root) => orior_cli::cli::tree(Some(&root))?,
        None => root_of(&app)?,
    };
    let program = std::env::current_exe().map_err(|error| error.to_string())?;
    let mut command = std::process::Command::new(program);
    command.arg("--root").arg(&root).args(&words).stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());
    orior_cli::runner::quiet(&mut command);
    command.spawn().map(|_| ()).map_err(|error| error.to_string())
}

/// Commits the files at `paths`, each under the tree, with `message`, git's hooks and signing as the
/// tree has them, and gives git's line for the commit.
#[tauri::command(async)]
fn git_commit(app: State<App>, message: String, paths: Vec<String>) -> Result<String, String> {
    git::commit(&root_of(&app)?, &message, &paths)
}

#[tauri::command(async)]
fn git_push(app: State<App>, repo: Option<String>) -> Result<String, String> {
    git::push(&repo_of(&app, repo.as_deref())?)
}

#[tauri::command(async)]
fn git_pull(app: State<App>, repo: Option<String>) -> Result<String, String> {
    git::pull(&repo_of(&app, repo.as_deref())?)
}

#[tauri::command(async)]
fn git_rollback(app: State<App>, path: String) -> Result<(), String> {
    git::rollback(&root_of(&app)?, &path)
}

/// How many commits the branch has that its remote does not, and the other way, or null where it
/// follows no remote.
#[tauri::command(async)]
fn git_ahead_behind(app: State<App>, repo: Option<String>) -> Result<Option<(u32, u32)>, String> {
    Ok(git::ahead_behind(&repo_of(&app, repo.as_deref())?))
}

/// Every branch's commits laid out as a graph, or the commits a search finds.
#[tauri::command(async)]
fn git_graph(app: State<App>, query: Option<String>, repo: Option<String>) -> Result<Vec<git::Drawn>, String> {
    Ok(git::graph(&repo_of(&app, repo.as_deref())?, query.as_deref().unwrap_or("")))
}

/// The files a commit changed, by their paths in the tree.
#[tauri::command(async)]
fn git_touched(app: State<App>, id: String, repo: Option<String>) -> Result<Vec<git::Touched>, String> {
    let mut found = git::touched(&repo_of(&app, repo.as_deref())?, &id)?;
    if let Some(repo) = repo.filter(|repo| !repo.is_empty()) {
        for file in &mut found {
            file.path = format!("{repo}/{}", file.path);
            file.was = file.was.take().map(|was| format!("{repo}/{was}"));
        }
    }
    Ok(found)
}

#[tauri::command(async)]
fn git_stashes(app: State<App>, repo: Option<String>) -> Result<Vec<git::Stash>, String> {
    Ok(git::stashes(&repo_of(&app, repo.as_deref())?))
}

#[tauri::command(async)]
fn git_stash(app: State<App>, act: String, name: Option<String>, message: Option<String>, repo: Option<String>) -> Result<String, String> {
    git::stash_act(&repo_of(&app, repo.as_deref())?, &act, name.as_deref().unwrap_or(""), message.as_deref().unwrap_or(""))
}

/// The commits of other branches the branch open does not hold, to cherry-pick from.
#[tauri::command(async)]
fn git_elsewhere(app: State<App>, repo: Option<String>) -> Result<Vec<git::Commit>, String> {
    Ok(git::elsewhere(&repo_of(&app, repo.as_deref())?))
}

#[tauri::command(async)]
fn git_cherry_pick(app: State<App>, id: String, repo: Option<String>) -> Result<String, String> {
    git::cherry_pick(&repo_of(&app, repo.as_deref())?, &id)
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

/// The commit that last changed each line of a file as the editor holds its text.
#[tauri::command(async)]
fn git_line_history(app: State<App>, path: String, text: String) -> Result<git::LineHistory, String> {
    git::line_history(&root_of(&app)?, &path, text)
}

#[tauri::command]
fn file_at(app: State<App>, path: String, id: String) -> Result<String, String> {
    git::text_at(&root_of(&app)?, &path, &id)
}

#[derive(serde::Serialize)]
struct Toolchains {
    tools: Vec<toolchains::Found>,
    groups: Vec<String>,
    own: Option<toolchains::Own>,
}

/// `text`, the file at `path` as the editor holds it, formatted by its language's formatter.
#[tauri::command(async)]
fn format_text(app: State<App>, path: String, language: String, text: String) -> Result<String, String> {
    format::format(&root::full(&root_of(&app)?, &path), &language, &text)
}

/// The width the formatter of `language` keeps the lines of the file at `path` to, as the project's
/// own settings for the formatter set it, or None where no formatter formats the language.
#[tauri::command(async)]
fn format_width(app: State<App>, path: String, language: String) -> Result<Option<u32>, String> {
    Ok(format::width(&root::full(&root_of(&app)?, &path), &language))
}

/// A path a server named, as the page names files: under the tree, from its top folder, and
/// elsewhere whole.
fn tree_path(root: &Path, path: &str) -> String {
    let path = PathBuf::from(path);
    let path = dunce::canonicalize(&path).unwrap_or(path);
    let root = dunce::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    // A file of a folder mounted beside the tree goes by its mount's name.
    if !path.starts_with(&root) && root::mounts().iter().any(|(_, base)| path.starts_with(base)) {
        return root::relative(&root, &path);
    }
    path.strip_prefix(&root).map(|inside| inside.to_string_lossy().replace('\\', "/")).unwrap_or_else(|_| path.display().to_string())
}

/// A folder mounted beside the tree: its name, its path on the disk, and whether it is a folder, a
/// single file being mounted alone.
#[derive(serde::Serialize)]
struct Mounted {
    name: String,
    path: String,
    dir: bool,
}

/// Whether the folder at `path` holds an orior tree.
#[tauri::command]
fn tree_holds(path: String) -> bool {
    root::holds_tree(Path::new(&path))
}

/// Mounts the folder or file at `path` beside the tree, and gives its name.
#[tauri::command]
fn tree_mount(path: String) -> Result<String, String> {
    root::mount(Path::new(&path))
}

#[tauri::command]
fn tree_unmount(name: String) {
    root::unmount(&name);
}

/// The folders and files mounted beside the tree, in the order they were mounted.
#[tauri::command]
fn tree_mounts() -> Vec<Mounted> {
    root::mounts().into_iter().map(|(name, path)| Mounted { name, dir: path.is_dir(), path: path.display().to_string() }).collect()
}

/// The folders a .code-workspace file names, each made whole against the file's own folder, as JSON
/// with comments and trailing commas reads them.
#[tauri::command]
fn workspace_read(path: String) -> Result<Vec<String>, String> {
    let file = PathBuf::from(&path);
    let text = std::fs::read_to_string(&file).map_err(|error| format!("{path}: {error}"))?;
    let value: serde_json::Value = serde_json::from_str(&plain_json(&text)).map_err(|error| format!("{path}: {error}"))?;
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

/// JSON with comments and trailing commas, as VS Code's files hold it, made plain JSON: each comment
/// outside a string taken out, and each comma before a closing bracket.
fn plain_json(text: &str) -> String {
    let chars: Vec<char> = text.chars().collect();
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    let mut in_string = false;
    while at < chars.len() {
        let c = chars[at];
        if in_string {
            out.push(c);
            if c == '\\' && at + 1 < chars.len() {
                out.push(chars[at + 1]);
                at += 2;
                continue;
            }
            if c == '"' {
                in_string = false;
            }
            at += 1;
        } else if c == '"' {
            in_string = true;
            out.push(c);
            at += 1;
        } else if c == '/' && chars.get(at + 1) == Some(&'/') {
            while at < chars.len() && chars[at] != '\n' {
                at += 1;
            }
        } else if c == '/' && chars.get(at + 1) == Some(&'*') {
            at += 2;
            while at + 1 < chars.len() && !(chars[at] == '*' && chars[at + 1] == '/') {
                at += 1;
            }
            at += 2;
        } else if c == ',' {
            let next = chars[at + 1..].iter().find(|one| !one.is_whitespace());
            if !matches!(next, Some(']') | Some('}')) {
                out.push(c);
            }
            at += 1;
        } else {
            out.push(c);
            at += 1;
        }
    }
    out
}

/// Each file's edits, each path as `tree_path` gives it.
fn tree_edits(root: &Path, files: Vec<servers::FileEdit>) -> Vec<servers::FileEdit> {
    files.into_iter().map(|mut file| {
        file.path = tree_path(root, &file.path);
        file
    }).collect()
}

/// Hands a file the editor opened to its language's server, starting it where it is not running.
/// Says whether a server took it; the file's diagnostics come as "lsp-diagnostics", and an edit the
/// server asks for as "lsp-edits".
#[tauri::command(async)]
fn lsp_open(handle: AppHandle, app: State<App>, path: String, language: String, text: String) -> Result<bool, String> {
    let root = root_of(&app)?;
    app.servers.open(&root, &root::full(&root, &path), &language, &text, &emitter(handle, root.clone()))
}

/// What a server tells the page, each path as `tree_path` gives it under `tree`.
fn emitter(handle: AppHandle, tree: PathBuf) -> servers::Emit {
    Arc::new(move |told| match told {
        servers::Told::Diagnostics(mut diagnostics) => {
            diagnostics.path = tree_path(&tree, &diagnostics.path);
            let _ = handle.emit("lsp-diagnostics", diagnostics);
        }
        servers::Told::Edits(files) => {
            let _ = handle.emit("lsp-edits", tree_edits(&tree, files));
        }
        servers::Told::Hints => {
            let _ = handle.emit("lsp-hints", ());
        }
        servers::Told::Tokens => {
            handle.state::<App>().servers.forget_tokens();
            let _ = handle.emit("lsp-parse", ());
        }
        servers::Told::Checking { done, total } => {
            let _ = handle.emit("tree-check", serde_json::json!({"done": done, "total": total}));
        }
    })
}

/// The languages orior's own inspections read, by the editor's names for them.
#[tauri::command]
fn inspect_languages() -> Vec<&'static str> {
    orior_cli::inspect::LANGUAGES.to_vec()
}

/// Starts the tree's check: every file of the tree the editor does not have open handed to its
/// language's server, its diagnostics coming as "lsp-diagnostics" and how far the check has gone as
/// "tree-check". Gives how many files wait.
#[tauri::command(async)]
fn problems_check(handle: AppHandle, app: State<App>) -> Result<usize, String> {
    let root = root_of(&app)?;
    Ok(app.servers.check_tree(&root, files::all(&root), &emitter(handle, root.clone())))
}

/// Every place the symbol at a place is used, each path as `tree_path` gives it.
#[tauri::command(async)]
fn lsp_references(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Usage>, String> {
    let root = root_of(&app)?;
    let found = app.servers.references(&root::full(&root, &path), line, col)?;
    Ok(found.into_iter().map(|mut one| {
        one.path = tree_path(&root, &one.path);
        one
    }).collect())
}

#[tauri::command(async)]
fn lsp_renamable(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<servers::Renamable>, String> {
    app.servers.renamable(&root::full(&root_of(&app)?, &path), line, col)
}

#[tauri::command(async)]
fn lsp_rename(app: State<App>, path: String, line: u32, col: u32, name: String) -> Result<Vec<servers::FileEdit>, String> {
    let root = root_of(&app)?;
    Ok(tree_edits(&root, app.servers.rename(&root::full(&root, &path), line, col, &name)?))
}

#[tauri::command(async)]
fn lsp_actions(app: State<App>, path: String, from: servers::Place, to: servers::Place) -> Result<Vec<servers::Action>, String> {
    app.servers.actions(&root::full(&root_of(&app)?, &path), from, to)
}

#[tauri::command(async)]
fn lsp_act(app: State<App>, path: String, raw: serde_json::Value) -> Result<Vec<servers::FileEdit>, String> {
    let root = root_of(&app)?;
    Ok(tree_edits(&root, app.servers.act(&root::full(&root, &path), &raw)?))
}

/// The lines `from` to `to` of the parse of a file the editor has open, and its folds.
#[tauri::command(async)]
fn parse_colors(app: State<App>, path: String, from: u32, to: u32) -> Result<Option<servers::Colors>, String> {
    app.servers.colors(&root::full(&root_of(&app)?, &path), from, to)
}

/// The spans of a file the editor has open that hold `from` to `to` and are more than it, the least
/// first.
#[tauri::command(async)]
fn parse_spans(app: State<App>, path: String, from: servers::Place, to: servers::Place) -> Result<Vec<(servers::Place, servers::Place)>, String> {
    app.servers.spans(&root::full(&root_of(&app)?, &path), from, to)
}

/// What Search Structurally found: each match, and each file's edits where a template was given.
#[derive(serde::Serialize)]
struct Shapes {
    found: Vec<orior_cli::shape::Found>,
    files: Vec<servers::FileEdit>,
}

/// Every match of `pattern` in the file `path`, or in every file of the tree in `language` where no
/// path is given, each file read as the editor holds it or the disk has it; with what `template`
/// writes in each match's place.
#[tauri::command(async)]
fn shape_search(app: State<App>, language: String, pattern: String, template: Option<String>, path: Option<String>) -> Result<Shapes, String> {
    let root = root_of(&app)?;
    let files = match path {
        Some(path) => vec![path],
        None => {
            let languages = plugins::languages();
            files::all(&root).into_iter().filter(|file| Path::new(file).extension().and_then(|ext| languages.get(&ext.to_string_lossy().to_lowercase())).is_some_and(|one| *one == language)).collect()
        }
    };
    let mut shapes = Shapes { found: Vec::new(), files: Vec::new() };
    for file in files {
        let full = root.join(&file);
        let Some(text) = app.servers.text_of(&full).or_else(|| std::fs::read_to_string(&full).ok()) else {
            continue;
        };
        let (found, edits) = orior_cli::shape::search(&language, &file, &text, &pattern, template.as_deref());
        if !edits.is_empty() {
            shapes.files.push(servers::FileEdit { path: file.clone(), edits });
        }
        shapes.found.extend(found);
    }
    Ok(shapes)
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

/// The POM at `path` of the tree, `pom.xml` at its top where none is given, and its text, `text` where
/// the editor holds it.
fn pom_of(app: &State<App>, path: Option<String>, text: Option<String>) -> Result<(PathBuf, PathBuf, String), String> {
    let root = root_of(app)?;
    let pom = path.filter(|path| orior_cli::builds::kind_of(std::path::Path::new(path)) == Some(orior_cli::builds::Kind::Maven)).map(|path| root::full(&root, &path)).unwrap_or_else(|| root.join("pom.xml"));
    let text = match text {
        Some(text) => text,
        None => std::fs::read_to_string(&pom).unwrap_or_default(),
    };
    Ok((root, pom, text))
}

/// The compiler's settings the POM gives, and Maven's own.
#[tauri::command]
fn maven_settings(app: State<App>, path: Option<String>, text: Option<String>) -> Result<Vec<orior_cli::builds::maven::Setting>, String> {
    let (root, pom, text) = pom_of(&app, path, text)?;
    Ok(orior_cli::builds::maven::settings(&root, &pom, &text))
}

/// Sets one of the settings `maven_settings` gives: the POM's by the edits returned, each path as
/// `tree_path` gives it, and Maven's own in the user's `settings.xml`.
#[tauri::command]
fn maven_set(app: State<App>, path: Option<String>, text: Option<String>, key: String, value: String) -> Result<Vec<servers::FileEdit>, String> {
    let (root, pom, text) = pom_of(&app, path, text)?;
    Ok(tree_edits(&root, orior_cli::builds::maven::set(&pom, &text, &key, &value)?))
}

/// The folder, in the tree, and the line that update every snapshot dependency of the build the
/// file at `path` belongs to, or of the tree's top build where none is given.
#[tauri::command]
fn snapshots_line(app: State<App>, path: Option<String>) -> Result<(String, String), String> {
    let root = root_of(&app)?;
    orior_cli::builds::snapshots_line(&root, path.map(|path| root::full(&root, &path)).as_deref())
}

/// The tree's Python tests, found as pytest and unittest find them.
#[tauri::command(async)]
fn tests_found(app: State<App>) -> Result<Vec<orior_cli::testing::Test>, String> {
    Ok(orior_cli::testing::found(&root_of(&app)?))
}

/// Runs `given`, the tree's tests by their names or its files of tests by their paths: spread over the
/// machine's cores where `parallel`, recording which lines run where `cover`. What the run finds comes
/// as "tests-run". Says the run's number.
#[tauri::command(async)]
fn tests_run(handle: AppHandle, app: State<App>, given: Vec<String>, parallel: bool, cover: bool) -> Result<u64, String> {
    let root = root_of(&app)?;
    let tell: orior_cli::test_runs::Tell = Arc::new(move |heard| {
        let _ = handle.emit("tests-run", heard);
    });
    app.tests.start(&root, given, parallel, cover, tell)
}

/// Profiles the Python file at `path` of the tree, here, or on the machine `remote` names as
/// user@host:folder, in that folder by the same path. What the profile finds comes as "profile".
#[tauri::command(async)]
fn profile_start(handle: AppHandle, app: State<App>, path: String, remote: Option<String>) -> Result<(), String> {
    let root = root_of(&app)?;
    let tell: orior_cli::profile::Tell = Arc::new(move |heard| {
        let _ = handle.emit("profile", heard);
    });
    app.profiles.start(&root, &path, remote.as_deref(), tell)
}

#[tauri::command]
fn profile_stop(app: State<App>) {
    app.profiles.stop();
}

/// Stops the run of tests going, where one is.
#[tauri::command]
fn tests_stop(app: State<App>) {
    app.tests.stop();
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

/// Keeps the tree open as the template `name`. Gives the template's folder.
#[tauri::command(async)]
fn template_keep(app: State<App>, name: String) -> Result<String, String> {
    orior_cli::templates::keep(&root_of(&app)?, &name).map(|made| made.display().to_string())
}

/// Opens the folder that holds the reader's templates, made where it is not there yet.
#[tauri::command]
fn templates_reveal() -> Result<(), String> {
    let folder = orior_cli::templates::folder().ok_or("orior has no folder of its own to keep templates in")?;
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    home::reveal(&folder)
}

/// Names the checkers to run as files change, by name or id, and checks the files open with them.
/// Gives the names no checker answers to.
#[tauri::command(async)]
fn checkers_set(handle: AppHandle, app: State<App>, names: Vec<String>) -> Vec<String> {
    match root_of(&app) {
        Ok(root) => app.servers.set_checkers(&names, Some(&root), &emitter(handle, root.clone())),
        Err(_) => app.servers.set_checkers(&names, None, &(Arc::new(|_| {}) as servers::Emit)),
    }
}

/// Every checker the manifest knows: its id, its name, and the languages it checks.
#[tauri::command]
fn checkers_known() -> Vec<(String, String, Vec<String>)> {
    orior_cli::checkers::known().into_iter().map(|one| (one.id, one.name, one.spec.languages)).collect()
}

/// Every environment the tree holds, nearest the top first.
#[tauri::command(async)]
fn envs_found(app: State<App>) -> Result<Vec<orior_cli::envs::Environment>, String> {
    Ok(orior_cli::envs::found(&root_of(&app)?))
}

/// What choosing an environment did: its Python, and the packages beside it it has not installed.
#[derive(serde::Serialize)]
struct EnvironmentUsed {
    python: Option<String>,
    missing: Vec<String>,
}

/// Makes the environment of `kind` at `place` in the tree the tree's own, or, given no kind, the
/// toolchains' own again; each server is told, to ask for its settings again.
#[tauri::command(async)]
fn env_use(app: State<App>, kind: Option<String>, place: String, requires: Vec<String>) -> Result<EnvironmentUsed, String> {
    let used = match kind {
        Some(kind) => {
            let env = orior_cli::envs::resolve(&root_of(&app)?, &kind, &place)?;
            let missing = orior_cli::envs::missing(&env, &requires);
            let python = env.python.as_ref().map(|python| python.display().to_string());
            toolchains::set_environment(Some(env));
            EnvironmentUsed { python, missing }
        }
        None => {
            toolchains::set_environment(None);
            EnvironmentUsed { python: None, missing: Vec::new() }
        }
    };
    app.servers.settings_changed();
    Ok(used)
}

/// The classes a parse names, by their index.
#[tauri::command]
fn parse_classes() -> Vec<&'static str> {
    orior_cli::inspect::CLASSES.to_vec()
}

/// The hints the server of a file writes on its lines `from` to `to`.
#[tauri::command(async)]
fn lsp_hints(app: State<App>, path: String, from: u32, to: u32) -> Result<Vec<servers::Hint>, String> {
    app.servers.hints(&root::full(&root_of(&app)?, &path), from, to)
}

/// The function at a place, as the top of its call hierarchy, its paths as `tree_path` gives them.
#[tauri::command(async)]
fn calls_root(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<servers::Call>, String> {
    let root = root_of(&app)?;
    Ok(app.servers.call_root(&root::full(&root, &path), line, col)?.map(|call| tree_call(&root, call)))
}

/// The functions that call the function `item` names, or that it calls, as `incoming` says.
#[tauri::command(async)]
fn calls_of(app: State<App>, path: String, item: serde_json::Value, incoming: bool) -> Result<Vec<servers::Call>, String> {
    let root = root_of(&app)?;
    Ok(app.servers.calls(&root::full(&root, &path), &item, incoming)?.into_iter().map(|call| tree_call(&root, call)).collect())
}

/// A function of the call hierarchy, its paths as `tree_path` gives them.
fn tree_call(root: &Path, mut call: servers::Call) -> servers::Call {
    call.path = tree_path(root, &call.path);
    call.site = tree_path(root, &call.site);
    call
}

#[tauri::command(async)]
fn lsp_signature(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<servers::Signature>, String> {
    app.servers.signature(&root::full(&root_of(&app)?, &path), line, col)
}

/// Writes edits to files the editor does not have open, each under the tree. Says how many files it
/// wrote; a file outside the tree is left as it is and named.
#[tauri::command(async)]
fn edits_write(app: State<App>, files: Vec<servers::FileEdit>) -> Result<usize, String> {
    let root = root_of(&app)?;
    let mut written = 0;
    for file in files {
        let path = root.join(&file.path);
        if Path::new(&file.path).is_absolute() || file.path.split('/').any(|part| part == "..") {
            return Err(format!("{} is outside the tree and was not changed", file.path));
        }
        let text = std::fs::read_to_string(&path).map_err(|error| format!("{}: {error}", file.path))?;
        std::fs::write(&path, servers::apply(&text, &file.edits)).map_err(|error| format!("{}: {error}", file.path))?;
        written += 1;
    }
    Ok(written)
}

#[tauri::command(async)]
fn lsp_change(app: State<App>, path: String, text: String) -> Result<(), String> {
    app.servers.change(&root::full(&root_of(&app)?, &path), &text)
}

#[tauri::command(async)]
fn lsp_close(app: State<App>, path: String) -> Result<(), String> {
    app.servers.close(&root::full(&root_of(&app)?, &path))
}

#[tauri::command(async)]
fn lsp_hover(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<String>, String> {
    app.servers.hover(&root::full(&root_of(&app)?, &path), line, col)
}

/// Where the symbol at a place is defined, each path as `tree_path` gives it.
#[tauri::command(async)]
fn lsp_definition(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Found>, String> {
    let root = root_of(&app)?;
    let found = app.servers.definition(&root::full(&root, &path), line, col)?;
    Ok(found.into_iter().map(|mut one| {
        one.path = tree_path(&root, &one.path);
        one
    }).collect())
}

#[tauri::command(async)]
fn lsp_complete(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Item>, String> {
    app.servers.complete(&root::full(&root_of(&app)?, &path), line, col)
}

/// Starts a debug session as `start` says, with `options`: its breakpoints, the exceptions it stops on
/// and the stepping the tree asks for. Each session's events come as "debug-event", { session, event,
/// body }. Says what the session is.
#[tauri::command(async)]
fn debug_start(handle: AppHandle, app: State<App>, start: debug::Start, options: debug::Options) -> Result<debug::SessionInfo, String> {
    let root = root_of(&app)?;
    let debugger = app.debugger.clone();
    let emit: debug::Emit = Arc::new(move |session, event, body| {
        if event == "terminated" || event == "adapterStopped" {
            debugger.ended(session);
        }
        if event == "process" {
            if let Some(pid) = body["systemProcessId"].as_u64() {
                debugger.process(session, pid as u32);
            }
        }
        let _ = handle.emit("debug-event", serde_json::json!({"session": session, "event": event, "body": body}));
    });
    app.debugger.start(&root, &start, &options, emit)
}

/// The sessions running.
#[tauri::command]
fn debug_sessions(app: State<App>) -> Vec<debug::SessionInfo> {
    app.debugger.sessions()
}

/// Sets the breakpoints of the file at `path` in each session that debugs its language, and gives
/// each line one was placed on and whether it is bound to code.
#[tauri::command(async)]
fn debug_breakpoints(app: State<App>, path: String, breakpoints: Vec<debug::Breakpoint>) -> Result<Option<Vec<(u32, bool)>>, String> {
    Ok(app.debugger.breakpoints(&root_of(&app)?, &path, &breakpoints))
}

#[tauri::command(async)]
fn debug_exceptions(app: State<App>, session: u64, on: Vec<String>) -> Result<(), String> {
    app.debugger.exceptions(session, &on)
}

#[tauri::command(async)]
fn debug_raised(app: State<App>, session: u64, thread: i64) -> Result<debug::Raised, String> {
    app.debugger.raised(session, thread)
}

#[tauri::command(async)]
fn debug_threads(app: State<App>, session: u64) -> Result<Vec<debug::Thread>, String> {
    app.debugger.threads(session)
}

/// The stopped thread's frames, each path as `tree_path` gives it.
#[tauri::command(async)]
fn debug_stack(app: State<App>, session: u64, thread: i64) -> Result<Vec<debug::Frame>, String> {
    let root = root_of(&app)?;
    Ok(app.debugger.stack(session, thread)?.into_iter().map(|mut frame| {
        frame.path = frame.path.map(|path| tree_path(&root, &path));
        frame
    }).collect())
}

#[tauri::command(async)]
fn debug_scopes(app: State<App>, session: u64, frame: i64) -> Result<Vec<debug::Scope>, String> {
    app.debugger.scopes(session, frame)
}

#[tauri::command(async)]
fn debug_variables(app: State<App>, session: u64, reference: i64) -> Result<Vec<debug::Variable>, String> {
    app.debugger.variables(session, reference)
}

#[tauri::command(async)]
fn debug_evaluate(app: State<App>, session: u64, expression: String, frame: Option<i64>, context: String) -> Result<debug::Variable, String> {
    app.debugger.evaluate(session, &expression, frame, &context)
}

#[tauri::command(async)]
fn debug_step(app: State<App>, session: u64, how: String, thread: i64) -> Result<(), String> {
    app.debugger.step(session, &how, thread)
}

#[tauri::command(async)]
fn debug_memory(app: State<App>, session: u64, reference: String, offset: i64, count: u64) -> Result<debug::Memory, String> {
    app.debugger.memory(session, &reference, offset, count)
}

#[tauri::command(async)]
fn debug_instructions(app: State<App>, session: u64, reference: String, offset: i64, count: i64) -> Result<Vec<debug::Instruction>, String> {
    app.debugger.instructions(session, &reference, offset, count)
}

/// The bytecode of the function of the Python file at `path` that holds `line`: its name, then its
/// instructions.
#[tauri::command(async)]
fn debug_bytecode(app: State<App>, path: String, line: u32) -> Result<(String, Vec<debug::Instruction>), String> {
    let root = root_of(&app)?;
    debug::bytecode(&root, &root::full(&root, &path), line)
}

#[tauri::command(async)]
fn debug_watch_data(app: State<App>, session: u64, name: String, reference: Option<i64>, bytes: Option<u64>) -> Result<String, String> {
    app.debugger.watch_data(session, &name, reference, bytes)
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

/// The values `name` of frame `frame` took through a recorded run, each with the line that gave it,
/// its path as `tree_path` gives it.
#[tauri::command(async)]
fn debug_history(app: State<App>, session: u64, frame: i64, name: String) -> Result<Vec<debug::Change>, String> {
    let root = root_of(&app)?;
    Ok(app.debugger.history(session, frame, &name)?.into_iter().map(|mut change| {
        change.path = tree_path(&root, &change.path);
        change
    }).collect())
}

#[tauri::command(async)]
fn debug_step_across(app: State<App>, session: u64, thread: i64) -> Result<(), String> {
    app.debugger.step_across(&root_of(&app)?, session, thread)
}

#[tauri::command(async)]
fn debug_goto(app: State<App>, session: u64, step: u64) -> Result<(), String> {
    app.debugger.goto(session, step)
}

/// The processes of the machine, for a session to attach to.
#[tauri::command(async)]
fn debug_processes() -> Vec<debug::Process> {
    debug::processes()
}

/// Ends session `session`, or every session where none is named.
#[tauri::command(async)]
fn debug_stop(app: State<App>, session: Option<u64>) {
    match session {
        Some(session) => app.debugger.stop(session),
        None => app.debugger.stop_all(),
    }
}

/// The file at `path`, under the tree, validated by the tool plugin for `language`.
#[tauri::command(async)]
fn validate_file(app: State<App>, path: String, language: String) -> Result<validate::Report, String> {
    let tool = validate::tool_for(&language).ok_or_else(|| format!("no tool plugin validates {language}"))?;
    validate::validate(&tool, &root::full(&root_of(&app)?, &path))
}

/// The shell line that runs the file at `path`, under the tree, with its language's toolchain.
#[tauri::command(async)]
fn run_file_line(app: State<App>, path: String, language: String) -> Result<run_file::RunLine, String> {
    let root = root_of(&app)?;
    run_file::line_for(&root, &root::full(&root, &path), &language)
}

/// Every language a formatter formats.
#[tauri::command]
fn format_languages() -> Vec<String> {
    format::languages()
}

/// Every toolchain as toolchains.rs finds it, and whether orior itself is on the PATH.
#[tauri::command(async)]
fn toolchains_check() -> Toolchains {
    Toolchains { tools: toolchains::check(), groups: toolchains::groups(), own: toolchains::own().ok() }
}

/// Adds a toolchain of the reader's, and gives its id.
#[tauri::command]
fn toolchain_add(tool: toolchains::Tool) -> Result<String, String> {
    toolchains::add(tool)
}

#[tauri::command]
fn toolchain_add_group(name: String) -> Result<(), String> {
    toolchains::add_group(&name)
}

#[tauri::command]
fn toolchain_remove(id: String) -> Result<(), String> {
    toolchains::remove(&id)
}

#[tauri::command]
fn toolchain_remove_group(name: String) -> Result<(), String> {
    toolchains::remove_group(&name)
}

/// What a toolchain says its version is.
#[tauri::command(async)]
fn toolchain_version(id: String) -> Result<String, String> {
    toolchains::version(&id)
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

/// Makes `folder` a repository, or the open tree where none is given, and answers the folder.
#[tauri::command(async)]
fn repo_init(app: State<App>, folder: Option<String>) -> Result<String, String> {
    let folder = match folder {
        Some(folder) => PathBuf::from(folder),
        None => root_of(&app)?,
    };
    git::init(&folder)?;
    Ok(folder.display().to_string())
}

#[tauri::command]
fn file_create(app: State<App>, path: String) -> Result<(), String> {
    files::create_file(&root_of(&app)?, &path)
}

#[tauri::command]
fn folder_create(app: State<App>, path: String) -> Result<(), String> {
    files::create_folder(&root_of(&app)?, &path)
}

/// A full path as the tree names it, relative and with forward slashes, or null where it is outside
/// the tree.
#[tauri::command]
fn tree_relative(app: State<App>, path: String) -> Option<String> {
    let root = root_of(&app).ok()?;
    let full = dunce::canonicalize(&path).ok()?;
    // A file under a folder mounted beside the tree goes by its mount's name as well.
    let mounted = root::mounts().iter().any(|(_, base)| full.starts_with(base));
    (full.starts_with(&root) || mounted).then(|| root::relative(&root, &full))
}

/// The shell line that installs a toolchain, for the terminal to run.
#[tauri::command]
fn toolchain_setup(id: String) -> Result<String, String> {
    toolchains::setup_line(&id)
}

/// Puts the folder of a toolchain, or of orior itself where `what` is "orior", on the reader's PATH.
#[tauri::command(async)]
fn toolchain_add_path(what: String) -> Result<String, String> {
    toolchains::add_to_path(&what)
}

/// Has orior run a toolchain from `folder`, and names the program found there.
#[tauri::command]
fn toolchain_use(id: String, folder: String) -> Result<String, String> {
    toolchains::choose(&id, &folder)
}

#[tauri::command]
fn toolchain_forget(id: String) -> Result<(), String> {
    toolchains::forget(&id)
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

#[tauri::command]
fn file_read(app: State<App>, path: String) -> Result<files::Opened, String> {
    files::read(&root_of(&app)?, &path)
}

#[tauri::command]
fn bridge_read(app: State<App>) -> Result<bridge::Bridge, String> {
    Ok(bridge::read(&root_of(&app)?))
}

#[tauri::command(async)]
fn file_window(app: State<App>, path: String, line: u64, half: u64) -> Result<files::Slice, String> {
    files::window(&root_of(&app)?, &path, line, half)
}

/// The whole lines from about `start` to about `end`, sent as bytes and not as JSON: the slice's
/// start, end and the file's size, each 8 bytes little-endian, then the text.
#[tauri::command(async)]
fn file_slice(app: State<App>, path: String, start: u64, end: u64) -> Result<tauri::ipc::Response, String> {
    let slice = files::slice(&root_of(&app)?, &path, start, end)?;
    let mut sent = Vec::with_capacity(24 + slice.text.len());
    sent.extend_from_slice(&slice.start.to_le_bytes());
    sent.extend_from_slice(&slice.end.to_le_bytes());
    sent.extend_from_slice(&slice.size.to_le_bytes());
    sent.extend_from_slice(slice.text.as_bytes());
    Ok(tauri::ipc::Response::new(sent))
}

#[tauri::command]
fn file_write(app: State<App>, path: String, text: String) -> Result<(), String> {
    let root = root_of(&app)?;
    let before = std::fs::read_to_string(root::full(&root, &path)).ok();
    files::write(&root, &path, &text)?;
    if let Err(error) = history::keep(&root, &path, before.as_deref(), &text) {
        eprintln!("orior: local history of {path}: {error}");
    }
    Ok(())
}

/// The Local History of the file at `path`, the newest first.
#[tauri::command(async)]
fn history_list(app: State<App>, path: String) -> Result<Vec<history::Snapshot>, String> {
    Ok(history::list(&root_of(&app)?, &path))
}

#[tauri::command(async)]
fn history_read(app: State<App>, path: String, at: u64) -> Result<String, String> {
    history::read(&root_of(&app)?, &path, at)
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
    let Ok(root) = root_of(&app) else { return refuse(StatusCode::NOT_FOUND, "no tree".into()) };
    let path = decoded(request.uri().path().trim_start_matches('/'));
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
    let root = launch.root.clone().or_else(root::find);
    let app = App { root: Mutex::new(root), launch: Mutex::new(Some(launch)), ..App::default() };
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
            watch_tree(app.handle(), &app.state::<App>());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            root_get,
            root_set,
            catalog_read,
            definitions_read,
            plugins_read,
            plugin_draft,
            plugin_create,
            user_css_read,
            reader_menus_read,
            tree_mount,
            tree_unmount,
            tree_mounts,
            tree_holds,
            workspace_read,
            home_reveal,
            format_text,
            format_width,
            format_languages,
            run_file_line,
            validate_file,
            lsp_open,
            lsp_change,
            lsp_close,
            problems_check,
            calls_root,
            lsp_hints,
            parse_colors,
            parse_spans,
            parse_classes,
            shape_search,
            code_docstring,
            checkers_set,
            envs_found,
            env_use,
            checkers_known,
            code_sort,
            fill_paragraph,
            reads_file,
            tests_found,
            tests_run,
            tests_stop,
            profile_start,
            profile_stop,
            maven_settings,
            maven_set,
            snapshots_line,
            templates_list,
            project_create,
            template_keep,
            templates_reveal,
            calls_of,
            inspect_languages,
            lsp_hover,
            lsp_definition,
            lsp_complete,
            lsp_references,
            lsp_renamable,
            lsp_rename,
            lsp_actions,
            lsp_act,
            lsp_signature,
            edits_write,
            debug_start,
            debug_sessions,
            debug_exceptions,
            debug_raised,
            debug_memory,
            debug_instructions,
            debug_bytecode,
            debug_watch_data,
            debug_processes,
            debug_history,
            debug_goto,
            debug_step_across,
            file_read_any,
            file_write_any,
            debug_breakpoints,
            debug_threads,
            debug_stack,
            debug_scopes,
            debug_variables,
            debug_evaluate,
            debug_step,
            debug_stop,
            toolchains_check,
            toolchain_add,
            toolchain_add_group,
            toolchain_remove,
            toolchain_remove_group,
            toolchain_version,
            toolchain_install,
            link_open,
            toolchain_setup,
            clone_start,
            repo_clone,
            toolchain_add_path,
            toolchain_use,
            toolchain_forget,
            bridge_read,
            job_start,
            job_stop,
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
            tree_list,
            tree_find,
            tree_files,
            files_find,
            patterns_set,
            kept_write,
            indent_for,
            git_branches,
            git_branch,
            git_resolve,
            git_commit_parts,
            files_copy,
            clip_files,
            files_paste,
            symbols_find,
            tree_search,
            zoom_set,
            tree_changed,
            tree_branch,
            report_error,
            report_bug,
            report_auto,
            report_auto_set,
            report_asked,
            report_open,
            file_commits,
            git_graph,
            git_touched,
            git_line_history,
            git_repositories,
            structure_compare,
            highlight_grammar,
            highlight_lines,
            git_stashes,
            git_stash,
            git_elsewhere,
            git_cherry_pick,
            history_list,
            scrollback_open,
            scrollback_keep,
            scrollback_read,
            scrollback_close,
            scrollback_reset,
            history_read,
            repos_folder,
            repo_opened,
            repo_init,
            file_create,
            folder_create,
            tree_relative,
            git_commit,
            git_push,
            git_pull,
            git_rollback,
            git_ahead_behind,
            window_act,
            file_head,
            file_at,
            file_read,
            file_window,
            file_slice,
            file_write,
            view_open,
            pick,
        ])
        .build(tauri::generate_context!())
        .expect("the app failed to start")
        .run(|handle, event| {
            // A language server orior started ends with it: on Windows a child outlives its parent.
            if let tauri::RunEvent::Exit = event {
                handle.state::<App>().servers.stop_all();
                handle.state::<App>().debugger.stop_all();
            }
        });
}

#[cfg(test)]
mod workspaces {
    #[test]
    fn a_workspace_with_comments_and_trailing_commas_reads_as_json() {
        let text = "{\n  // the folders\n  \"folders\": [\n    { \"path\": \"a // not a comment\" }, /* one more */\n    { \"path\": \"../b\", },\n  ],\n}\n";
        let value: serde_json::Value = serde_json::from_str(&super::plain_json(text)).unwrap();
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
