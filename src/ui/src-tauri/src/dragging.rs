// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Tells the page when the window's frame is being dragged or its edges pulled: `window-drag`, true
//! as it starts and false as it ends, which the page stops its animation for.
//!
//! On Windows the frame's own messages say so: WM_ENTERSIZEMOVE as the press on the title bar or an
//! edge begins the move, before the window has gone anywhere, and WM_EXITSIZEMOVE as the press ends.
//! Elsewhere the first move or resize starts it, and a rest of REST with neither ends it.

use tauri::{AppHandle, WebviewWindow};

#[cfg(windows)]
mod frame {
    use std::sync::OnceLock;

    use tauri::{AppHandle, Emitter, WebviewWindow};
    use windows_sys::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
    use windows_sys::Win32::UI::Shell::{DefSubclassProc, SetWindowSubclass};
    use windows_sys::Win32::UI::WindowsAndMessaging::{WM_ENTERSIZEMOVE, WM_EXITSIZEMOVE};

    static HANDLE: OnceLock<AppHandle> = OnceLock::new();

    // The number the subclass is known by on the window.
    const ID: usize = 0x6f72;

    unsafe extern "system" fn watch(hwnd: HWND, message: u32, wparam: WPARAM, lparam: LPARAM, _id: usize, _data: usize) -> LRESULT {
        let moving = match message {
            WM_ENTERSIZEMOVE => Some(true),
            WM_EXITSIZEMOVE => Some(false),
            _ => None,
        };
        if let (Some(moving), Some(handle)) = (moving, HANDLE.get()) {
            let _ = handle.emit("window-drag", moving);
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
