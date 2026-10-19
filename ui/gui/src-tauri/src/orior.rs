// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's side of the window, in C (ui/cli/window.h), linked in by build.rs: the command line run on the words
//! the program was started with; the window's tree side made and ended; every call of the page answered by name, the
//! tree's over the link where the tree is on another machine and the window's own here; and a terminal's key to the
//! comms layer.

use std::ffi::{c_char, c_int, c_uint, c_ulonglong, c_void, CString};

use tauri::{AppHandle, Emitter};

/// Where what orior's side tells goes: the event's name, and its body, JSON.
type Emit = extern "C" fn(*mut c_void, *const c_char, *const c_char, c_ulonglong);

unsafe extern "C" {
    fn cli_window_command_line(words: *const *const c_char, count: c_uint, window: *mut c_int) -> c_int;
    fn cli_window_begun(emit: Emit, context: *mut c_void);
    fn cli_window_ended();
    fn cli_window_called(own: c_int, name: *const c_char, args: *const c_char, size: c_ulonglong) -> c_uint;
    fn cli_window_answer(room: c_uint, text: *mut *const c_char, size: *mut c_ulonglong) -> c_int;
    fn cli_window_given(room: c_uint);
    fn cli_window_terminal_key(key: *mut c_char, address: *mut c_char) -> c_int;
    fn cli_window_terminal_bound(key: *const c_char, process: c_uint);
    fn cli_window_terminal_left(key: *const c_char);
}

/// The bytes a terminal's key and the door's address are written into, more than either takes.
const HELD: usize = 4096;

/// The variables a process started under the window finds the comms layer's door and its key in (door.h).
pub const ADDRESS_VARIABLE: &str = "ORIOR_COMMS";
pub const KEY_VARIABLE: &str = "ORIOR_KEY";

/// A text made one C reads, a 0 in it cut there.
fn c_text(text: &str) -> CString {
    CString::new(text.split('\0').next().unwrap_or_default()).unwrap_or_default()
}

/// The command line run on `words`: the code to exit with, and whether the words open the window instead.
pub fn command_line(words: &[String]) -> (i32, bool) {
    let held: Vec<CString> = words.iter().map(|word| c_text(word)).collect();
    let pointers: Vec<*const c_char> = held.iter().map(|word| word.as_ptr()).collect();
    let mut window: c_int = 0;
    // SAFETY: the words live while the call runs, `count` says how many, and `window` is one int to write.
    let code = unsafe { cli_window_command_line(pointers.as_ptr(), pointers.len() as c_uint, &mut window) };
    (code, window != 0)
}

/// An event orior's side tells handed on to the page.
extern "C" fn told(context: *mut c_void, event: *const c_char, body: *const c_char, size: c_ulonglong) {
    // SAFETY: the context is the handle `begun` leaked, which lives as long as the program; the event is a C text and
    // the body `size` bytes, each valid while the call runs.
    let handle = unsafe { &*(context as *const AppHandle) };
    let event = unsafe { std::ffi::CStr::from_ptr(event) }.to_string_lossy().into_owned();
    let bytes = unsafe { std::slice::from_raw_parts(body as *const u8, size as usize) };
    let value: serde_json::Value = serde_json::from_slice(bytes).unwrap_or(serde_json::Value::Null);
    if let Err(error) = handle.emit(&event, &value) {
        eprintln!("the page could not be told {event} of {} bytes: {error}", bytes.len());
    }
}

/// The window opened as the command line left it, what orior's side tells going to the page.
pub fn begun(handle: AppHandle) {
    let context = Box::into_raw(Box::new(handle)) as *mut c_void;
    // SAFETY: the context lives as long as the program, which `told` relies on.
    unsafe { cli_window_begun(told, context) };
}

/// Every server, session, link and process of the window's ended, as it ends.
pub fn ended() {
    // SAFETY: it takes nothing.
    unsafe { cli_window_ended() };
}

/// The call `name` answered, the window's own where `own`, else the tree's: its answer, or why it failed.
fn called(own: bool, name: &str, args: &serde_json::Value) -> Result<serde_json::Value, String> {
    let name = c_text(name);
    let args = serde_json::to_vec(args).unwrap_or_default();
    // SAFETY: the name is a C text and the arguments `len` bytes, each valid while the call runs.
    let room = unsafe { cli_window_called(own as c_int, name.as_ptr(), args.as_ptr() as *const c_char, args.len() as c_ulonglong) };
    let mut text: *const c_char = std::ptr::null();
    let mut size: c_ulonglong = 0;
    // SAFETY: the room is the one the call was answered in, and it is given back once its answer is read.
    let answered = unsafe { cli_window_answer(room, &mut text, &mut size) } != 0;
    let bytes = unsafe { std::slice::from_raw_parts(text as *const u8, size as usize) }.to_vec();
    unsafe { cli_window_given(room) };
    if answered {
        serde_json::from_slice(&bytes).map_err(|error| error.to_string())
    } else {
        Err(String::from_utf8_lossy(&bytes).into_owned())
    }
}

/// A call of the window's own.
pub fn own(name: &str, args: serde_json::Value) -> Result<serde_json::Value, String> {
    called(true, name, &args)
}

/// A call of the tree's side: over the link where the tree is on another machine, and here where not.
pub fn tree(name: &str, args: serde_json::Value) -> Result<serde_json::Value, String> {
    called(false, name, &args)
}

/// A call of the window's own read as `T`.
pub fn own_as<T: serde::de::DeserializeOwned>(name: &str, args: serde_json::Value) -> Result<T, String> {
    own(name, args).and_then(|value| serde_json::from_value(value).map_err(|error| error.to_string()))
}

/// A terminal's part of the comms layer: its key, given to the shell once it starts and taken away as it ends.
pub struct Part {
    key: CString,
}

impl Part {
    /// A key that listens and speaks on `runs/*`, and the door's address, or nothing where there is no door.
    pub fn made() -> Option<(Part, String, String)> {
        let mut key = vec![0u8; HELD];
        let mut address = vec![0u8; HELD];
        // SAFETY: each holds HELD bytes, more than a key and an address take.
        let made = unsafe { cli_window_terminal_key(key.as_mut_ptr() as *mut c_char, address.as_mut_ptr() as *mut c_char) } != 0;
        if !made {
            return None;
        }
        let key = std::ffi::CStr::from_bytes_until_nul(&key).ok()?.to_owned();
        let address = std::ffi::CStr::from_bytes_until_nul(&address).ok()?.to_string_lossy().into_owned();
        let text = key.to_string_lossy().into_owned();
        Some((Part { key }, address, text))
    }

    /// The part bound to the shell's process.
    pub fn bind(&self, process: u32) {
        // SAFETY: the key is a C text.
        unsafe { cli_window_terminal_bound(self.key.as_ptr(), process) };
    }

    /// The part let go, as the shell ends.
    pub fn leave(&self) {
        // SAFETY: the key is a C text.
        unsafe { cli_window_terminal_left(self.key.as_ptr()) };
    }
}
