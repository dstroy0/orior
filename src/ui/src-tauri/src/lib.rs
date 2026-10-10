// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the window, a layer on the orior-cli crate. Every job, every reading of the tree and
//! every command of the menus is that crate's; this layer adds the window, the commands its page
//! calls, the `view` scheme its page windows load from, the terminal's pseudo-terminals and the
//! clipboard. The same program is the command line, handing it any words it is started with.

mod dragging;
mod memory;
mod scrollback;
mod terminal;

use orior_cli::cli::{self, Launch, Outcome};
use orior_cli::{bridge, catalog, commands, debug, defs, files, format, git, history, home, plugins, report, root, run_file, runner, servers, symbols, toolchains, validate};

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
    windows: AtomicU64,
}

fn root_of(app: &App) -> Result<PathBuf, String> {
    app.root.lock().map_err(|e| e.to_string())?.clone().ok_or_else(|| "no orior tree is open".to_string())
}

#[tauri::command]
fn root_get(app: State<App>) -> Option<String> {
    app.root.lock().ok()?.as_ref().map(|p| p.to_string_lossy().into_owned())
}

#[tauri::command]
fn root_set(app: State<App>, path: String) -> Result<String, String> {
    let path = dunce::canonicalize(&path).map_err(|e| format!("{path}: {e}"))?;
    if !root::holds_tree(&path) {
        return Err(format!("{} holds no orior tree", path.display()));
    }
    let mut root = app.root.lock().map_err(|e| e.to_string())?;
    let moved = root.as_ref() != Some(&path);
    *root = Some(path.clone());
    drop(root);
    // A server answers for the tree it started in. Another tree starts its own as its files open.
    if moved {
        app.servers.let_go();
        app.symbols.forget();
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
fn term_open(handle: AppHandle, app: State<App>, cols: u16, rows: u16) -> Result<u64, String> {
    let home = std::env::var_os(if cfg!(windows) { "USERPROFILE" } else { "HOME" }).map(PathBuf::from);
    let at = root_of(&app).ok().or(home).or_else(|| std::env::current_dir().ok()).ok_or("no folder to open a terminal in")?;
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
fn memory_use() -> Option<memory::Memory> {
    memory::read()
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

/// Every line in the tree's files that holds the query, for Find in Files.
#[tauri::command]
async fn tree_search(app: State<'_, App>, query: String, how: files::Searching) -> Result<Vec<files::Hit>, String> {
    let root = root_of(&app)?;
    tauri::async_runtime::spawn_blocking(move || files::search(&root, &query, how)).await.map_err(|e| e.to_string())?
}

/// Sets the window's zoom, 1 being none.
#[tauri::command]
fn zoom_set(webview: tauri::Webview, factor: f64) -> Result<(), String> {
    webview.set_zoom(factor.clamp(0.5, 3.0)).map_err(|e| e.to_string())
}

#[tauri::command]
fn tree_changed(app: State<App>) -> Result<Vec<git::Changed>, String> {
    Ok(git::changed(&root_of(&app)?))
}

#[tauri::command]
fn tree_branch(app: State<App>) -> Result<Option<String>, String> {
    Ok(git::branch(&root_of(&app)?))
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
        "drag" => return dragging::start_drag(&window).map(|()| window.is_maximized().unwrap_or(false)),
        "state" => Ok(()),
        other => return Err(format!("{other} is not something the window does")),
    };
    done.map_err(|error| error.to_string())?;
    Ok(window.is_maximized().unwrap_or(false))
}

/// Commits the files at `paths`, each under the tree, with `message`, git's hooks and signing as the
/// tree has them, and gives git's line for the commit.
#[tauri::command(async)]
fn git_commit(app: State<App>, message: String, paths: Vec<String>) -> Result<String, String> {
    git::commit(&root_of(&app)?, &message, &paths)
}

#[tauri::command(async)]
fn git_push(app: State<App>) -> Result<String, String> {
    git::push(&root_of(&app)?)
}

#[tauri::command(async)]
fn git_pull(app: State<App>) -> Result<String, String> {
    git::pull(&root_of(&app)?)
}

#[tauri::command(async)]
fn git_rollback(app: State<App>, path: String) -> Result<(), String> {
    git::rollback(&root_of(&app)?, &path)
}

/// How many commits the branch has that its remote does not, and the other way, or null where it
/// follows no remote.
#[tauri::command(async)]
fn git_ahead_behind(app: State<App>) -> Result<Option<(u32, u32)>, String> {
    Ok(git::ahead_behind(&root_of(&app)?))
}

/// The commits of the branch the tree is on, the newest first.
#[tauri::command(async)]
fn tree_commits(app: State<App>) -> Result<Vec<git::Commit>, String> {
    Ok(git::log(&root_of(&app)?))
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
    format::format(&root_of(&app)?.join(path), &language, &text)
}

/// The width the formatter of `language` keeps the lines of the file at `path` to, as the project's
/// own settings for the formatter set it, or None where no formatter formats the language.
#[tauri::command(async)]
fn format_width(app: State<App>, path: String, language: String) -> Result<Option<u32>, String> {
    Ok(format::width(&root_of(&app)?.join(path), &language))
}

/// A path a server named, as the page names files: under the tree, from its top folder, and
/// elsewhere whole.
fn tree_path(root: &Path, path: &str) -> String {
    let path = PathBuf::from(path);
    let path = dunce::canonicalize(&path).unwrap_or(path);
    let root = dunce::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    path.strip_prefix(&root).map(|inside| inside.to_string_lossy().replace('\\', "/")).unwrap_or_else(|_| path.display().to_string())
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
    app.servers.open(&root, &root.join(path), &language, &text, &emitter(handle, root.clone()))
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
    })
}

/// Every place the symbol at a place is used, each path as `tree_path` gives it.
#[tauri::command(async)]
fn lsp_references(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Usage>, String> {
    let root = root_of(&app)?;
    let found = app.servers.references(&root.join(path), line, col)?;
    Ok(found.into_iter().map(|mut one| {
        one.path = tree_path(&root, &one.path);
        one
    }).collect())
}

#[tauri::command(async)]
fn lsp_renamable(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<servers::Renamable>, String> {
    app.servers.renamable(&root_of(&app)?.join(path), line, col)
}

#[tauri::command(async)]
fn lsp_rename(app: State<App>, path: String, line: u32, col: u32, name: String) -> Result<Vec<servers::FileEdit>, String> {
    let root = root_of(&app)?;
    Ok(tree_edits(&root, app.servers.rename(&root.join(path), line, col, &name)?))
}

#[tauri::command(async)]
fn lsp_actions(app: State<App>, path: String, from: servers::Place, to: servers::Place) -> Result<Vec<servers::Action>, String> {
    app.servers.actions(&root_of(&app)?.join(path), from, to)
}

#[tauri::command(async)]
fn lsp_act(app: State<App>, path: String, raw: serde_json::Value) -> Result<Vec<servers::FileEdit>, String> {
    let root = root_of(&app)?;
    Ok(tree_edits(&root, app.servers.act(&root.join(path), &raw)?))
}

#[tauri::command(async)]
fn lsp_signature(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<servers::Signature>, String> {
    app.servers.signature(&root_of(&app)?.join(path), line, col)
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
    app.servers.change(&root_of(&app)?.join(path), &text)
}

#[tauri::command(async)]
fn lsp_close(app: State<App>, path: String) -> Result<(), String> {
    app.servers.close(&root_of(&app)?.join(path))
}

#[tauri::command(async)]
fn lsp_hover(app: State<App>, path: String, line: u32, col: u32) -> Result<Option<String>, String> {
    app.servers.hover(&root_of(&app)?.join(path), line, col)
}

/// Where the symbol at a place is defined, each path as `tree_path` gives it.
#[tauri::command(async)]
fn lsp_definition(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Found>, String> {
    let root = root_of(&app)?;
    let found = app.servers.definition(&root.join(path), line, col)?;
    Ok(found.into_iter().map(|mut one| {
        one.path = tree_path(&root, &one.path);
        one
    }).collect())
}

#[tauri::command(async)]
fn lsp_complete(app: State<App>, path: String, line: u32, col: u32) -> Result<Vec<servers::Item>, String> {
    app.servers.complete(&root_of(&app)?.join(path), line, col)
}

/// Builds where it is built, and debugs, the file at `path` in `language`, with `breakpoints`, each
/// file's lines. The adapter's events come as "debug-event", { event, body }. Says which toolchain
/// debugs it.
#[tauri::command(async)]
fn debug_start(handle: AppHandle, app: State<App>, path: String, language: String, breakpoints: HashMap<String, Vec<u32>>) -> Result<String, String> {
    let root = root_of(&app)?;
    let debugger = app.debugger.clone();
    let emit: debug::Emit = Arc::new(move |event, body| {
        if event == "terminated" || event == "adapterStopped" {
            debugger.ended();
        }
        let _ = handle.emit("debug-event", serde_json::json!({"event": event, "body": body}));
    });
    app.debugger.start(&root, &root.join(path), &language, &breakpoints, emit)
}

#[tauri::command(async)]
fn debug_breakpoints(app: State<App>, path: String, lines: Vec<u32>) -> Result<Vec<(u32, bool)>, String> {
    app.debugger.breakpoints(&root_of(&app)?.join(path), &lines)
}

#[tauri::command(async)]
fn debug_threads(app: State<App>) -> Result<Vec<debug::Thread>, String> {
    app.debugger.threads()
}

/// The stopped thread's frames, each path as `tree_path` gives it.
#[tauri::command(async)]
fn debug_stack(app: State<App>, thread: i64) -> Result<Vec<debug::Frame>, String> {
    let root = root_of(&app)?;
    Ok(app.debugger.stack(thread)?.into_iter().map(|mut frame| {
        frame.path = frame.path.map(|path| tree_path(&root, &path));
        frame
    }).collect())
}

#[tauri::command(async)]
fn debug_scopes(app: State<App>, frame: i64) -> Result<Vec<debug::Scope>, String> {
    app.debugger.scopes(frame)
}

#[tauri::command(async)]
fn debug_variables(app: State<App>, reference: i64) -> Result<Vec<debug::Variable>, String> {
    app.debugger.variables(reference)
}

#[tauri::command(async)]
fn debug_evaluate(app: State<App>, expression: String, frame: Option<i64>, context: String) -> Result<debug::Variable, String> {
    app.debugger.evaluate(&expression, frame, &context)
}

#[tauri::command(async)]
fn debug_step(app: State<App>, how: String, thread: i64) -> Result<(), String> {
    app.debugger.step(&how, thread)
}

#[tauri::command(async)]
fn debug_stop(app: State<App>) {
    app.debugger.stop();
}

/// The file at `path`, under the tree, validated by the tool plugin for `language`.
#[tauri::command(async)]
fn validate_file(app: State<App>, path: String, language: String) -> Result<validate::Report, String> {
    let tool = validate::tool_for(&language).ok_or_else(|| format!("no tool plugin validates {language}"))?;
    validate::validate(&tool, &root_of(&app)?.join(path))
}

/// The shell line that runs the file at `path`, under the tree, with its language's toolchain.
#[tauri::command(async)]
fn run_file_line(app: State<App>, path: String, language: String) -> Result<run_file::RunLine, String> {
    let root = root_of(&app)?;
    run_file::line_for(&root, &root.join(path), &language)
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
    full.starts_with(&root).then(|| root::relative(&root, &full))
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

/// Opens one of orior's own places as the system opens it: "user-css", made first where it is not
/// there, "plugins", the reader's plugins folder, made first likewise, or the folder of one of the
/// reader's plugins.
#[tauri::command]
fn home_reveal(what: String) -> Result<(), String> {
    let plugins_dir = home::plugins().ok_or("orior has no folder of its own")?;
    let path = match what.as_str() {
        "user-css" => home::ensure_user_css()?,
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
    let before = std::fs::read_to_string(root.join(&path)).ok();
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
        .manage(app)
        .register_uri_scheme_protocol("view", view_scheme)
        .setup(|app| {
            // The web view's own ground, which shows between one page and the next, is the page's.
            // A page that never asks for the window still has it shown after SHOW_ANYWAY.
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_background_color(Some(tauri::window::Color(0x13, 0x13, 0x31, 0xff)));
                std::thread::spawn(move || {
                    std::thread::sleep(SHOW_ANYWAY);
                    let _ = window.show();
                });
            }
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
            home_reveal,
            format_text,
            format_width,
            format_languages,
            run_file_line,
            validate_file,
            lsp_open,
            lsp_change,
            lsp_close,
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
            commands_read,
            launch_take,
            app_version,
            app_exit,
            window_show,
            memory_use,
            tree_list,
            tree_find,
            tree_files,
            files_find,
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
            tree_commits,
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
                handle.state::<App>().debugger.stop();
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
