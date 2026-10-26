// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The app: the window, a viewport on orior's C side (orior.rs). Every call of its page, every job, every reading of
//! the tree and every command of the menus is that side's; this layer adds the window itself, its controls, the `view`
//! scheme its page windows load from, and printing. The same program is the command line, which orior's side runs on
//! the words the program is started with.

mod dragging;
mod orior;
mod printing;

use std::borrow::Cow;

use tauri::http::{Request, Response, StatusCode};
use tauri::{AppHandle, Manager, UriSchemeContext};

/// Answers a call of the page's by its name, as serve.c answers it: the header orior-call names it, orior-own says it
/// is the window's own and not the tree's side's, which goes over the link where the tree is on another machine, and
/// the body is its arguments' bytes, handed to orior's side as they came, its answer handed back as written.
#[tauri::command(async)]
fn call(request: tauri::ipc::Request<'_>) -> Result<tauri::ipc::Response, String> {
    let header = |key: &str| request.headers().get(key).and_then(|value| value.to_str().ok()).unwrap_or_default();
    let name = header("orior-call");
    let tauri::ipc::InvokeBody::Raw(args) = request.body() else {
        return Err(format!("{name}: the page sends a call's arguments as their bytes"));
    };
    orior::passed(header("orior-own") == "1", name, args)
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

/// The app's version, as its package gives it.
#[tauri::command]
fn app_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

/// Whether the app is a debug build, which asks no reader whether to send its reports.
#[tauri::command]
fn app_debug() -> bool {
    cfg!(debug_assertions)
}

/// What each release of the window changed, as CHANGES.md gives it, built into the app.
#[tauri::command]
fn app_changes() -> &'static str {
    include_str!("../../../CHANGES.md")
}

/// Shows the window once its page has its scheme and its colors, so that no frame before them shows.
#[tauri::command]
fn window_show(window: tauri::WebviewWindow) -> Result<(), String> {
    window.show().map_err(|e| e.to_string())
}

/// Ends the app, every window of it, from the File menu.
#[tauri::command]
fn app_exit(handle: AppHandle) {
    handle.exit(0);
}

/// Sets the window's zoom, 1 being none.
#[tauri::command]
fn zoom_set(webview: tauri::Webview, factor: f64) -> Result<(), String> {
    webview.set_zoom(factor.clamp(0.5, 3.0)).map_err(|e| e.to_string())
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

/// Serves a file a page window asks for, as orior's side reads it: under the tree, and nothing outside it. The one
/// copy is of the file's bytes into the body the scheme's answer takes and owns.
fn view_scheme(_context: UriSchemeContext<'_, tauri::Wry>, request: Request<Vec<u8>>) -> Response<Cow<'static, [u8]>> {
    orior::view_served(request.uri().path(), |status, media_type, bytes| {
        Response::builder().status(status).header("Content-Type", media_type).body(Cow::Owned(bytes.to_vec())).expect("a file response")
    })
    .unwrap_or_else(|| Response::builder().status(StatusCode::NOT_FOUND).body(Cow::Borrowed(&[][..])).expect("an empty response"))
}

/// The program: the command line where it is given words, else the window. Returns the code to
/// exit with.
pub fn start() -> i32 {
    // A panic of the window's files as an error does, its category by where its backtrace enters orior's code.
    let shown = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        shown(info);
        orior::panicked(info);
    }));
    let (code, window) = orior::command_line();
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

/// The most monitors the window's place is fitted to.
const MONITORS: usize = 16;

/// Puts the window where it was left for the tree open, fitted to the monitors there are now, the main one first.
fn put_back(window: &tauri::WebviewWindow) {
    let area = |monitor: &tauri::Monitor| orior::Area { x: monitor.position().x, y: monitor.position().y, width: monitor.size().width, height: monitor.size().height };
    let mut areas = [orior::Area { x: 0, y: 0, width: 0, height: 0 }; MONITORS];
    let mut count = 0;
    if let Ok(Some(main)) = window.primary_monitor() {
        areas[0] = area(&main);
        count = 1;
    }
    for one in window.available_monitors().unwrap_or_default().iter().map(area) {
        if count < MONITORS && !areas[..count].contains(&one) {
            areas[count] = one;
            count += 1;
        }
    }
    let Some(fitted) = orior::place_kept(&areas[..count]) else { return };
    let _ = window.set_size(tauri::PhysicalSize::new(fitted.width, fitted.height));
    let _ = window.set_position(tauri::PhysicalPosition::new(fitted.x, fitted.y));
    if fitted.maximized != 0 {
        let _ = window.maximize();
    }
}

/// Opens the window on the tree the launch names, else the one `cli_tree_found` finds, to run the
/// launch's command once the page is up.
fn open() {
    tauri::Builder::default()
        .plugin(orior::kept_script(|script| tauri::plugin::Builder::<tauri::Wry, ()>::new("kept").js_init_script(script).build()))
        .register_uri_scheme_protocol("view", view_scheme)
        .setup(|app| {
            // orior's side: the tree open, its jobs, its servers and its debugger, what it tells sent to the page.
            orior::begun(app.handle().clone());
            // The web view's own ground, which shows between one page and the next, is the page's.
            // A page that never asks for the window still has it shown after SHOW_ANYWAY.
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_background_color(Some(tauri::window::Color(0x13, 0x13, 0x31, 0xff)));
                // The web view's browser, counted in what the app holds wherever it stands.
                #[cfg(windows)]
                let _ = window.with_webview(|platform| {
                    let mut browser = 0u32;
                    // SAFETY: the web view's own interface, on the thread that owns it, writing one number.
                    let known = unsafe { platform.controller().CoreWebView2().and_then(|core| core.BrowserProcessId(&mut browser)) };
                    if known.is_ok() {
                        orior::browser_known(browser);
                    }
                });
                // The system's own title bar where the reader asked for it, set before the window shows.
                if orior::title_bar_system() {
                    let _ = window.set_decorations(true);
                }
                // The tree's window where it was left, fitted to the monitors there are now.
                put_back(&window);
                std::thread::spawn(move || {
                    std::thread::sleep(SHOW_ANYWAY);
                    let _ = window.show();
                });
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            call,
            app_version,
            app_debug,
            app_changes,
            app_exit,
            window_show,
            printers_list,
            print_page,
            zoom_set,
            window_act,
        ])
        .on_window_event(|window, event| {
            // The tree's window kept where it stands as it is asked to close.
            if let (tauri::WindowEvent::CloseRequested { .. }, "main") = (event, window.label()) {
                if let (Ok(at), Ok(size)) = (window.outer_position(), window.inner_size()) {
                    let maximized = window.is_maximized().unwrap_or(false);
                    orior::place_keep(&orior::Place { x: at.x, y: at.y, width: size.width, height: size.height, maximized: maximized.into() });
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
