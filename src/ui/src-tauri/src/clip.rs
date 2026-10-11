// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Files on the system clipboard, as the system's file manager puts them there and reads them.
//!
//! Files copied or cut in the explorer go on the clipboard as a list of their full paths, which
//! another window of orior or the file manager pastes. On Windows a cut says so in the clipboard's
//! Preferred DropEffect, as the file manager's own cut does, and a paste of files the file manager
//! cut moves them. Elsewhere a cut is known only to the window it was made in, and a paste in any
//! other window or program copies.

use std::path::PathBuf;
use std::sync::Mutex;

/// The files this window last cut, where the clipboard cannot say a cut.
static CUT: Mutex<Option<Vec<PathBuf>>> = Mutex::new(None);

/// Puts the files on the clipboard, to be moved when pasted where `cut` and copied otherwise.
pub fn put(paths: &[PathBuf], cut: bool) -> Result<(), String> {
    let mut clip = arboard::Clipboard::new().map_err(|error| error.to_string())?;
    clip.set().file_list(paths).map_err(|error| error.to_string())?;
    effect::put(cut)?;
    if let Ok(mut held) = CUT.lock() {
        *held = cut.then(|| paths.to_vec());
    }
    Ok(())
}

/// The files on the clipboard and whether they were cut. None where it holds no files.
pub fn held() -> Option<(Vec<PathBuf>, bool)> {
    let paths = arboard::Clipboard::new().ok()?.get().file_list().ok().filter(|paths| !paths.is_empty())?;
    let cut = effect::held().unwrap_or_else(|| CUT.lock().ok().and_then(|held| held.clone()).as_ref() == Some(&paths));
    Some((paths, cut))
}

/// Clears the clipboard once files cut are moved, as the file manager does. A paste after it finds
/// no files, and none from where they no longer are.
pub fn let_go() {
    if let Ok(mut clip) = arboard::Clipboard::new() {
        let _ = clip.clear();
    }
    if let Ok(mut held) = CUT.lock() {
        *held = None;
    }
}

#[cfg(windows)]
mod effect {
    use std::thread::sleep;
    use std::time::Duration;

    use windows_sys::Win32::System::DataExchange::{CloseClipboard, GetClipboardData, OpenClipboard, RegisterClipboardFormatW, SetClipboardData};
    use windows_sys::Win32::Foundation::GlobalFree;
    use windows_sys::Win32::System::Memory::{GMEM_MOVEABLE, GlobalAlloc, GlobalLock, GlobalUnlock};

    /// The drop effects a file manager reads: a copy, a move, and a link to the file.
    const COPY: u32 = 1;
    const MOVE: u32 = 2;
    const LINK: u32 = 4;

    /// How many times the clipboard is asked for while another program holds it, and how long apart.
    const TRIES: u32 = 10;
    const APART: Duration = Duration::from_millis(5);

    fn format() -> u32 {
        let name: Vec<u16> = "Preferred DropEffect".encode_utf16().chain([0]).collect();
        // SAFETY: the name ends in a nul.
        unsafe { RegisterClipboardFormatW(name.as_ptr()) }
    }

    fn open() -> bool {
        for _ in 0..TRIES {
            // SAFETY: a null window opens the clipboard for this thread.
            if unsafe { OpenClipboard(std::ptr::null_mut()) } != 0 {
                return true;
            }
            sleep(APART);
        }
        false
    }

    /// Adds the effect to the files on the clipboard: a move for a cut, else a copy or a link, as the
    /// file manager writes them.
    pub fn put(cut: bool) -> Result<(), String> {
        let effect = if cut { MOVE } else { COPY | LINK };
        if !open() {
            return Err("the clipboard is held by another program".into());
        }
        // SAFETY: the memory is four bytes, written while locked, and once on the clipboard it is the
        // clipboard's; where it is not taken it is freed here.
        unsafe {
            let memory = GlobalAlloc(GMEM_MOVEABLE, 4);
            let at = GlobalLock(memory) as *mut u32;
            let taken = !at.is_null() && {
                at.write_unaligned(effect);
                GlobalUnlock(memory);
                !SetClipboardData(format(), memory).is_null()
            };
            if !taken && !memory.is_null() {
                GlobalFree(memory);
            }
            CloseClipboard();
            if taken { Ok(()) } else { Err("the clipboard did not take the files' effect".into()) }
        }
    }

    /// Whether the files on the clipboard were cut, or None where it does not say.
    pub fn held() -> Option<bool> {
        if !open() {
            return None;
        }
        // SAFETY: the clipboard's memory is read while locked, and only its first four bytes, which a
        // drop effect is.
        unsafe {
            let memory = GetClipboardData(format());
            let at = if memory.is_null() { std::ptr::null() } else { GlobalLock(memory) as *const u32 };
            let effect = (!at.is_null()).then(|| {
                let effect = at.read_unaligned();
                GlobalUnlock(memory);
                effect
            });
            CloseClipboard();
            effect.map(|effect| effect & MOVE != 0 && effect & COPY == 0)
        }
    }
}

#[cfg(not(windows))]
mod effect {
    pub fn put(_cut: bool) -> Result<(), String> {
        Ok(())
    }

    pub fn held() -> Option<bool> {
        None
    }
}
