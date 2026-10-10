// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Moves the window with the pointer from a press on the page's own top bar until it is let go.
//!
//! On Windows the window is told to start moving with the mouse, as its system menu's Move does,
//! the move measured from where the pointer is: the move starts at once, with no wait to tell a
//! click from a drag, and the window keeps its place under the pointer. Snapping to the screen's
//! edges works as it does for any title bar. Elsewhere the window manager's own move starts it.

use tauri::WebviewWindow;

#[cfg(windows)]
pub fn start_drag(window: &WebviewWindow) -> Result<(), String> {
    use windows_sys::Win32::Foundation::HWND;
    use windows_sys::Win32::UI::Input::KeyboardAndMouse::ReleaseCapture;
    use windows_sys::Win32::UI::WindowsAndMessaging::{PostMessageW, HTCAPTION, SC_MOVE, WM_SYSCOMMAND};
    let hwnd = window.hwnd().map_err(|error| error.to_string())?.0 as HWND;
    // SAFETY: the window is this app's own, and the call takes no pointer.
    unsafe {
        ReleaseCapture();
        // SC_MOVE with HTCAPTION in its low bits is a move by the mouse.
        if PostMessageW(hwnd, WM_SYSCOMMAND, (SC_MOVE | HTCAPTION) as usize, 0) == 0 {
            return Err("the window could not be moved".into());
        }
    }
    Ok(())
}

#[cfg(not(windows))]
pub fn start_drag(window: &WebviewWindow) -> Result<(), String> {
    window.start_dragging().map_err(|error| error.to_string())
}
