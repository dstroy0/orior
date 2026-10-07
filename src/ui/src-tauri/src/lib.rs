// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the commands its window calls, and the `view` scheme its page windows load from. The
//! same program is the command line, in `cli`, over the same jobs, runner and bridge.

mod bridge;
mod catalog;
mod cli;
mod defs;
mod files;
mod root;
mod runner;
mod terminal;

use std::borrow::Cow;
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};

use tauri::http::{Request, Response, StatusCode};
use tauri::{AppHandle, Emitter, Manager, State, UriSchemeContext, WebviewUrl, WebviewWindowBuilder};

#[derive(Default)]
struct App {
    root: Mutex<Option<PathBuf>>,
    runs: runner::Runs,
    terms: terminal::Terms,
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
    *app.root.lock().map_err(|e| e.to_string())? = Some(path.clone());
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

#[tauri::command]
fn file_read(app: State<App>, path: String) -> Result<files::Opened, String> {
    files::read(&root_of(&app)?, &path)
}

#[tauri::command]
fn bridge_read(app: State<App>) -> Result<bridge::Bridge, String> {
    Ok(bridge::read(&root_of(&app)?))
}

#[tauri::command]
fn file_window(app: State<App>, path: String, line: u64, half: u64) -> Result<files::Slice, String> {
    files::window(&root_of(&app)?, &path, line, half)
}

#[tauri::command]
fn file_slice(app: State<App>, path: String, start: u64, end: u64) -> Result<files::Slice, String> {
    files::slice(&root_of(&app)?, &path, start, end)
}

#[tauri::command]
fn file_write(app: State<App>, path: String, text: String) -> Result<(), String> {
    files::write(&root_of(&app)?, &path, &text)
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
    let words: Vec<String> = std::env::args().skip(1).collect();
    if words.is_empty() {
        cli::console_let_go();
        run();
        0
    } else {
        cli::run(words)
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let app = App { root: Mutex::new(root::find()), ..App::default() };
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(app)
        .register_uri_scheme_protocol("view", view_scheme)
        .invoke_handler(tauri::generate_handler![
            root_get,
            root_set,
            catalog_read,
            definitions_read,
            bridge_read,
            job_start,
            job_stop,
            term_open,
            term_write,
            term_resize,
            term_close,
            clip_read,
            tree_list,
            tree_find,
            file_read,
            file_window,
            file_slice,
            file_write,
            view_open,
            pick,
        ])
        .run(tauri::generate_context!())
        .expect("the app failed to start");
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
