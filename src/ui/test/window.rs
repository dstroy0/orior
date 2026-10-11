// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's window for the tests: built as released, with the page inside it, into a folder of the
//! harness's own, and started with a home, a tree and a web view profile of the run's own and its
//! debugging port open on a port of its own: a run neither reads nor changes the reader's orior
//! and runs beside an orior already open. On Windows the window is put on the monitor the run is
//! given, filling it, without taking the keyboard from the window the reader works in.

use std::fs::File;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};

use crate::cdp;

pub struct Window {
    child: Child,
    pub port: u16,
    pub page: String,
}

impl Drop for Window {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// Builds the app into `target`, and gives the program built.
pub fn build(ui: &Path, target: &Path) -> Result<PathBuf, String> {
    let manifest = ui.join("src-tauri").join("Cargo.toml");
    let built = Command::new("cargo").args(["build", "--manifest-path"]).arg(&manifest).arg("--target-dir").arg(target).stdin(Stdio::null()).status().map_err(|error| format!("cargo: {error}"))?;
    if !built.success() {
        return Err("the app did not build".into());
    }
    Ok(target.join("debug").join(if cfg!(windows) { "orior.exe" } else { "orior" }))
}

/// A port nothing listens on, of the ports the system gives out.
fn free_port() -> Result<u16, String> {
    let listener = std::net::TcpListener::bind(("127.0.0.1", 0)).map_err(|error| error.to_string())?;
    listener.local_addr().map(|address| address.port()).map_err(|error| error.to_string())
}

/// The browser's arguments the window's settings give it, with the debugging port at `port`.
fn browser_arguments(ui: &Path, port: u16) -> String {
    let settings = std::fs::read_to_string(ui.join("src-tauri").join("tauri.conf.json")).ok().and_then(|text| serde_json::from_str::<serde_json::Value>(&text).ok());
    let given = settings.as_ref().and_then(|settings| settings["app"]["windows"][0]["additionalBrowserArgs"].as_str()).unwrap_or("");
    format!("{given} --remote-debugging-port={port}").trim().to_string()
}

/// Starts the window from `program` with `run` as the run's folder and `tree` as its tree, on
/// `monitor`, and waits for its page.
pub fn start(program: &Path, ui: &Path, run: &Path, tree: &Path, monitor: Option<u32>) -> Result<Window, String> {
    keep_own_handles();
    let port = free_port()?;
    let log = File::create(run.join("window.log")).map_err(|error| error.to_string())?;
    let child = Command::new(program)
        .current_dir(tree)
        .env("ORIOR_HOME", run.join("home"))
        .env("ORIOR_ROOT", tree)
        // Each error the window meets in a run files as an issue, once.
        .env_remove("ORIOR_NO_REPORTS")
        .env("WEBVIEW2_USER_DATA_FOLDER", run.join("webview"))
        .env("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", browser_arguments(ui, port))
        .stdin(Stdio::null())
        .stdout(log.try_clone().map_err(|error| error.to_string())?)
        .stderr(log)
        .spawn()
        .map_err(|error| format!("{}: {error}", program.display()))?;
    let mut window = Window { child, port, page: String::new() };
    let until = Instant::now() + Duration::from_secs(120);
    let mut placed = false;
    while Instant::now() < until {
        if let Ok(Some(status)) = window.child.try_wait() {
            return Err(format!("the window closed as it started ({status}); its log is {}", run.join("window.log").display()));
        }
        if !placed {
            placed = place(window.child.id(), monitor);
        }
        if let Some(page) = cdp::window_page(port) {
            window.page = page;
            if placed || !cfg!(windows) {
                return Ok(window);
            }
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    Err(format!("the window's page did not open in 120 s; its log is {}", run.join("window.log").display()))
}

/// Keeps the window on its monitor once it shows, as the app may place it itself when it does.
pub fn keep_placed(window: &Window, monitor: Option<u32>) {
    let until = Instant::now() + Duration::from_secs(20);
    while Instant::now() < until {
        if shown_on(window.child.id(), monitor) {
            return;
        }
        place(window.child.id(), monitor);
        std::thread::sleep(Duration::from_millis(100));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_browser_is_given_the_window_s_own_arguments_and_the_port() {
        let ui = Path::new(env!("CARGO_MANIFEST_DIR")).parent().unwrap();
        let given = browser_arguments(ui, 41234);
        assert!(given.ends_with("--remote-debugging-port=41234"), "{given}");
        assert!(given.contains("--disable-direct-composition"), "{given}");
        assert_eq!(browser_arguments(Path::new("/nowhere"), 1), "--remote-debugging-port=1");
    }

    #[test]
    fn a_free_port_takes_a_listener() {
        let port = free_port().unwrap();
        assert!(std::net::TcpListener::bind(("127.0.0.1", port)).is_ok());
    }

    #[test]
    fn the_monitors_have_one_main_one_where_there_are_any() {
        let all = monitors();
        assert!(all.is_empty() || all.iter().filter(|(_, primary, _)| *primary).count() == 1);
        assert!(all.iter().all(|(_, _, [_, _, width, height])| *width > 0 && *height > 0));
    }
}

/// Keeps the harness's own input and output from the window: a window kept open past the run
/// would otherwise hold them open, and whatever reads the harness's output would wait on it.
/// A program the harness runs with its output, as cargo, is given its own copy.
#[cfg(windows)]
fn keep_own_handles() {
    use windows_sys::Win32::Foundation::{SetHandleInformation, HANDLE_FLAG_INHERIT};
    use windows_sys::Win32::System::Console::{GetStdHandle, STD_ERROR_HANDLE, STD_INPUT_HANDLE, STD_OUTPUT_HANDLE};
    for which in [STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE] {
        unsafe {
            let handle = GetStdHandle(which);
            if !handle.is_null() {
                SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
            }
        }
    }
}

#[cfg(not(windows))]
fn keep_own_handles() {}

#[cfg(windows)]
pub use screen::{monitors, place, shown_on};

#[cfg(not(windows))]
pub fn monitors() -> Vec<(String, bool, [i32; 4])> {
    Vec::new()
}

#[cfg(not(windows))]
pub fn place(_process: u32, _monitor: Option<u32>) -> bool {
    true
}

#[cfg(not(windows))]
pub fn shown_on(_process: u32, _monitor: Option<u32>) -> bool {
    true
}

#[cfg(windows)]
mod screen {
    use windows_sys::Win32::Foundation::{HWND, LPARAM, RECT};
    use windows_sys::Win32::Graphics::Gdi::{EnumDisplayMonitors, GetMonitorInfoW, MonitorFromWindow, HDC, HMONITOR, MONITORINFO, MONITORINFOEXW, MONITOR_DEFAULTTONEAREST};
    use windows_sys::Win32::UI::WindowsAndMessaging::{EnumWindows, GetWindow, GetWindowTextW, GetWindowThreadProcessId, IsWindowVisible, SetProcessDPIAware, SetWindowPos, GW_OWNER, SWP_NOACTIVATE, SWP_NOZORDER};
    use windows_sys::core::BOOL;

    const PRIMARY: u32 = 1;

    struct Monitor {
        handle: HMONITOR,
        device: String,
        primary: bool,
        work: RECT,
        whole: RECT,
    }

    unsafe extern "system" fn each_monitor(handle: HMONITOR, _: HDC, _: *mut RECT, found: LPARAM) -> BOOL {
        let found = unsafe { &mut *(found as *mut Vec<Monitor>) };
        let mut info = MONITORINFOEXW::default();
        info.monitorInfo.cbSize = std::mem::size_of::<MONITORINFOEXW>() as u32;
        if unsafe { GetMonitorInfoW(handle, &mut info as *mut MONITORINFOEXW as *mut MONITORINFO) } != 0 {
            let length = info.szDevice.iter().position(|unit| *unit == 0).unwrap_or(info.szDevice.len());
            found.push(Monitor { handle, device: String::from_utf16_lossy(&info.szDevice[..length]), primary: info.monitorInfo.dwFlags & PRIMARY != 0, work: info.monitorInfo.rcWork, whole: info.monitorInfo.rcMonitor });
        }
        1
    }

    fn all() -> Vec<Monitor> {
        unsafe { SetProcessDPIAware() };
        let mut found: Vec<Monitor> = Vec::new();
        unsafe { EnumDisplayMonitors(std::ptr::null_mut(), std::ptr::null(), Some(each_monitor), &mut found as *mut Vec<Monitor> as LPARAM) };
        found
    }

    fn number(device: &str) -> Option<u32> {
        device.rsplit("DISPLAY").next()?.parse().ok()
    }

    /// The monitors, each with its name, whether it is the main one, and its place and size.
    pub fn monitors() -> Vec<(String, bool, [i32; 4])> {
        all().into_iter().map(|one| (one.device, one.primary, [one.whole.left, one.whole.top, one.whole.right - one.whole.left, one.whole.bottom - one.whole.top])).collect()
    }

    /// The monitor numbered `monitor`, as the system's display settings number it, or else the
    /// highest numbered that is not the main one, the main one where it is the only one.
    fn chosen(monitor: Option<u32>) -> Option<Monitor> {
        let mut all = all();
        all.sort_by_key(|one| number(&one.device).unwrap_or(0));
        match monitor {
            Some(wanted) => all.into_iter().find(|one| number(&one.device) == Some(wanted)),
            None => {
                let main = all.iter().position(|one| one.primary);
                let other = all.iter().rposition(|one| !one.primary);
                other.or(main).map(|at| all.swap_remove(at))
            }
        }
    }

    unsafe extern "system" fn each_window(window: HWND, found: LPARAM) -> BOOL {
        let found = unsafe { &mut *(found as *mut (u32, Vec<HWND>)) };
        let mut process = 0u32;
        unsafe { GetWindowThreadProcessId(window, &mut process) };
        if process == found.0 && unsafe { GetWindow(window, GW_OWNER) }.is_null() {
            let mut title = [0u16; 64];
            let length = unsafe { GetWindowTextW(window, title.as_mut_ptr(), title.len() as i32) };
            if String::from_utf16_lossy(&title[..length.max(0) as usize]) == "orior" {
                found.1.push(window);
            }
        }
        1
    }

    fn window_of(process: u32) -> Option<HWND> {
        let mut found: (u32, Vec<HWND>) = (process, Vec::new());
        unsafe { EnumWindows(Some(each_window), &mut found as *mut (u32, Vec<HWND>) as LPARAM) };
        found.1.into_iter().next()
    }

    /// Moves the window of `process` onto `monitor`, filling its work area, and says whether the
    /// window was there to move.
    pub fn place(process: u32, monitor: Option<u32>) -> bool {
        let (Some(window), Some(on)) = (window_of(process), chosen(monitor)) else { return false };
        let work = on.work;
        unsafe { SetWindowPos(window, std::ptr::null_mut(), work.left, work.top, work.right - work.left, work.bottom - work.top, SWP_NOZORDER | SWP_NOACTIVATE) };
        true
    }

    /// Whether the window of `process` shows on `monitor`.
    pub fn shown_on(process: u32, monitor: Option<u32>) -> bool {
        let (Some(window), Some(on)) = (window_of(process), chosen(monitor)) else { return false };
        unsafe { IsWindowVisible(window) != 0 && MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) == on.handle }
    }
}
