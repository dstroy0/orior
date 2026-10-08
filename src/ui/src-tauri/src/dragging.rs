// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Tells the page when the window's frame is held, dragged or its edges pulled: `window-drag`, true
//! as it starts and false as it ends, which the page suspends its work for.
//!
//! On Windows it starts with the press itself. A press on the title bar or an edge comes as
//! WM_NCLBUTTONDOWN, and Windows runs the whole press, any move or resize in it, inside its answer to
//! that message, which comes back only once the button is let go: the page is told before the
//! answer begins and again when it is done, a click that moves nothing included. WM_ENTERSIZEMOVE
//! and WM_EXITSIZEMOVE say the same of a move the keys make from the window's menu. Elsewhere the
//! first move or resize starts it, and a rest of REST with neither ends it.

use tauri::{AppHandle, WebviewWindow};

#[cfg(windows)]
mod frame {
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::OnceLock;

    use tauri::{AppHandle, Emitter, WebviewWindow};
    use windows_sys::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
    use windows_sys::Win32::UI::Shell::{DefSubclassProc, SetWindowSubclass};
    use windows_sys::Win32::UI::WindowsAndMessaging::{HTBOTTOMRIGHT, HTCAPTION, HTLEFT, WM_ENTERSIZEMOVE, WM_EXITSIZEMOVE, WM_NCLBUTTONDOWN};

    static HANDLE: OnceLock<AppHandle> = OnceLock::new();

    // Whether the page was last told the frame is held, which a second telling of the same repeats.
    static HELD: AtomicBool = AtomicBool::new(false);

    // The number the subclass is known by on the window.
    const ID: usize = 0x6f72;

    fn tell(held: bool) {
        if HELD.swap(held, Ordering::SeqCst) != held {
            if let Some(handle) = HANDLE.get() {
                let _ = handle.emit("window-drag", held);
            }
        }
    }

    // Whether a press at hit test `hit` is on the title bar or an edge, which move and size the
    // window; the buttons, the menu and the page are not.
    fn on_frame(hit: usize) -> bool {
        hit == HTCAPTION as usize || (HTLEFT as usize..=HTBOTTOMRIGHT as usize).contains(&hit)
    }

    unsafe extern "system" fn watch(hwnd: HWND, message: u32, wparam: WPARAM, lparam: LPARAM, _id: usize, _data: usize) -> LRESULT {
        match message {
            WM_NCLBUTTONDOWN if on_frame(wparam) => {
                tell(true);
                let answer = unsafe { DefSubclassProc(hwnd, message, wparam, lparam) };
                tell(false);
                return answer;
            }
            WM_ENTERSIZEMOVE => tell(true),
            WM_EXITSIZEMOVE => tell(false),
            _ => {}
        }
        unsafe { DefSubclassProc(hwnd, message, wparam, lparam) }
    }

    pub fn watch_frame(handle: &AppHandle, window: &WebviewWindow) {
        let _ = HANDLE.set(handle.clone());
        if let Ok(hwnd) = window.hwnd() {
            unsafe {
                SetWindowSubclass(hwnd.0 as HWND, Some(watch), ID, 0);
            }
        }
    }
}

#[cfg(not(windows))]
mod frame {
    use std::sync::{Arc, Mutex};
    use std::time::{Duration, Instant};

    use tauri::{AppHandle, Emitter, WebviewWindow, WindowEvent};

    // How long the frame rests before a drag counts as over.
    const REST: Duration = Duration::from_millis(180);

    pub fn watch_frame(handle: &AppHandle, window: &WebviewWindow) {
        let last: Arc<Mutex<Option<Instant>>> = Arc::new(Mutex::new(None));
        let moved = last.clone();
        let emitter = handle.clone();
        window.on_window_event(move |event| {
            if matches!(event, WindowEvent::Moved(_) | WindowEvent::Resized(_)) {
                if let Ok(mut at) = moved.lock() {
                    if at.is_none() {
                        let _ = emitter.emit("window-drag", true);
                    }
                    *at = Some(Instant::now());
                }
            }
        });
        let ender = handle.clone();
        std::thread::spawn(move || loop {
            std::thread::sleep(REST / 3);
            if let Ok(mut at) = last.lock() {
                if at.is_some_and(|when| when.elapsed() >= REST) {
                    *at = None;
                    let _ = ender.emit("window-drag", false);
                }
            }
        });
    }
}

/// Starts telling the page about drags of `window`'s frame.
pub fn watch(handle: &AppHandle, window: &WebviewWindow) {
    frame::watch_frame(handle, window);
}
