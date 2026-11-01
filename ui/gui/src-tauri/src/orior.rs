// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's side of the window, in C (ui/cli/window.h), linked in by build.rs: the command line run on the words
//! the program was started with; the window's tree side made and ended; every call of the page answered by name, the
//! tree's over the link where the tree is on another machine and the window's own here; the window's place, its
//! page's first script and its title bar; the web view's browser process; and a panic filed.

use std::ffi::{c_char, c_int, c_uint, c_ulonglong, c_void};

use tauri::{AppHandle, Emitter};

/// Where what orior's side tells goes: the event's name, and its body, JSON.
type Emit = extern "C" fn(*mut c_void, *const c_char, *const c_char, c_ulonglong);

/// Where a file a page window asks for is handed: its status, its media type, and its bytes (window.h).
type Served = extern "C" fn(*mut c_void, c_uint, *const c_char, *const c_char, c_ulonglong);

unsafe extern "C" {
    fn cli_window_command_line(window: *mut c_int) -> c_int;
    fn cli_window_begun(emit: Emit, context: *mut c_void);
    fn cli_window_ended();
    fn cli_window_called(own: c_int, name: *const c_char, args: *const c_char, size: c_ulonglong) -> c_uint;
    fn cli_window_answer(room: c_uint, text: *mut *const c_char, size: *mut c_ulonglong) -> c_int;
    fn cli_window_given(room: c_uint);
    fn cli_window_browser_known(pid: c_uint);
    fn cli_window_kept_script(size: *mut c_ulonglong) -> *const c_char;
    fn cli_window_kept_script_given();
    fn cli_window_title_bar_system() -> c_int;
    fn cli_window_panicked(message: *const c_char, message_size: c_ulonglong, detail: *const c_char, detail_size: c_ulonglong);
    fn cli_window_place_kept(areas: *const Area, count: c_uint, place: *mut Place) -> c_int;
    fn cli_window_place_keep(place: *const Place);
    fn cli_window_view_served(path: *const c_char, size: c_ulonglong, served: Served, context: *mut c_void);
}

/// A place on the screen in the system's own pixels, and whether the window filled its monitor there (window.h).
#[repr(C)]
pub struct Place {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub maximized: c_int,
}

/// A monitor's area, in the system's own pixels (window.h).
#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
pub struct Area {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

/// The place kept for the tree open, fitted to the monitors `areas`, the system's main one first; none where none is
/// kept.
pub fn place_kept(areas: &[Area]) -> Option<Place> {
    let mut place = Place { x: 0, y: 0, width: 0, height: 0, maximized: 0 };
    // SAFETY: the areas are `len` of them and the place one to write, each valid while the call runs.
    let kept = unsafe { cli_window_place_kept(areas.as_ptr(), areas.len() as c_uint, &mut place) } != 0;
    kept.then_some(place)
}

/// The place the tree's window stands in kept, as it closes.
pub fn place_keep(place: &Place) {
    // SAFETY: the place is valid while the call runs.
    unsafe { cli_window_place_keep(place) };
}

/// The bytes a panic's message and its backtrace are written in, each cut where its room ends.
const PANIC_MESSAGE: usize = 4096;
const PANIC_DETAIL: usize = 64 << 10;

/// Text written into a room of fixed size, cut where the room ends.
struct Room<'a> {
    bytes: &'a mut [u8],
    at: usize,
}

impl std::fmt::Write for Room<'_> {
    fn write_str(&mut self, text: &str) -> std::fmt::Result {
        let taken = text.len().min(self.bytes.len() - self.at);
        self.bytes[self.at..self.at + taken].copy_from_slice(&text.as_bytes()[..taken]);
        self.at += taken;
        Ok(())
    }
}

/// A panic of the window's filed by orior's side as an error is, its message and its backtrace written into rooms on
/// the panicking thread's stack. The backtrace's capture is the standard library's, which takes memory of its own as
/// the process ends.
pub fn panicked(info: &std::panic::PanicHookInfo<'_>) {
    use std::fmt::Write;
    let mut message = [0u8; PANIC_MESSAGE];
    let mut detail = [0u8; PANIC_DETAIL];
    let mut message_room = Room { bytes: &mut message, at: 0 };
    let _ = write!(message_room, "{info}");
    let message_size = message_room.at;
    let mut detail_room = Room { bytes: &mut detail, at: 0 };
    let _ = write!(detail_room, "{}", std::backtrace::Backtrace::force_capture());
    let detail_size = detail_room.at;
    // SAFETY: each room holds the bytes its size says, valid while the call runs.
    unsafe { cli_window_panicked(message.as_ptr() as *const c_char, message_size as c_ulonglong, detail.as_ptr() as *const c_char, detail_size as c_ulonglong) };
}

/// The page's first script, as orior's side writes it, handed to `taken` while it is kept and given back after.
pub fn kept_script<T>(taken: impl FnOnce(&str) -> T) -> T {
    let mut size: c_ulonglong = 0;
    // SAFETY: `size` is one number to write; the script is `size` bytes, valid until it is given back below.
    let script = unsafe { bytes_at(cli_window_kept_script(&mut size), size) };
    let kept = taken(std::str::from_utf8(script).unwrap_or_default());
    // SAFETY: the script is read, and nothing reads it after.
    unsafe { cli_window_kept_script_given() };
    kept
}

/// Whether the reader asked for the system's own title bar.
pub fn title_bar_system() -> bool {
    // SAFETY: it takes nothing.
    unsafe { cli_window_title_bar_system() != 0 }
}

/// The web view's browser process, counted with every process under it in what the window holds.
pub fn browser_known(pid: u32) {
    // SAFETY: it takes one number.
    unsafe { cli_window_browser_known(pid) };
}

/// The bytes a call's name is handed to orior's side in, its 0 after it, more than any call's name takes.
const NAME_MOST: usize = 128;

/// `size` bytes at `at`, none where there are none.
///
/// # Safety
/// `at` holds `size` bytes, valid while the slice is read.
unsafe fn bytes_at<'a>(at: *const c_char, size: c_ulonglong) -> &'a [u8] {
    if at.is_null() || size == 0 {
        return &[];
    }
    // SAFETY: the caller's.
    unsafe { std::slice::from_raw_parts(at as *const u8, size as usize) }
}

/// The command line run on the words the program was started with, which orior's side reads of the system itself: the
/// code to exit with, and whether the words open the window instead.
pub fn command_line() -> (i32, bool) {
    let mut window: c_int = 0;
    // SAFETY: `window` is one int to write.
    let code = unsafe { cli_window_command_line(&mut window) };
    (code, window != 0)
}

/// An event orior's side tells handed on to the page, its body as orior's side wrote it: read where it lies and
/// never taken apart, or null where it is no JSON.
extern "C" fn event_callback(context: *mut c_void, event: *const c_char, body: *const c_char, size: c_ulonglong) {
    // SAFETY: the context is the handle `begun` leaked, which lives as long as the program; the event is a C text and
    // the body `size` bytes, each valid while the call runs.
    let handle = unsafe { &*(context as *const AppHandle) };
    let Ok(event) = unsafe { std::ffi::CStr::from_ptr(event) }.to_str() else {
        return;
    };
    let bytes = unsafe { bytes_at(body, size) };
    let sent = match serde_json::from_slice::<&serde_json::value::RawValue>(bytes) {
        Ok(raw) => handle.emit(event, raw),
        Err(_) => handle.emit(event, ()),
    };
    if let Err(error) = sent {
        eprintln!("the page could not be told {event} of {} bytes: {error}", bytes.len());
    }
}

/// The window opened as the command line left it, what orior's side tells going to the page.
pub fn begun(handle: AppHandle) {
    let context = Box::into_raw(Box::new(handle)) as *mut c_void;
    // SAFETY: the context lives as long as the program, which `event_callback` relies on.
    unsafe { cli_window_begun(event_callback, context) };
}

/// Every server, session, link and process of the window's ended, as it ends.
pub fn ended() {
    // SAFETY: it takes nothing.
    unsafe { cli_window_ended() };
}

/// The call `name` answered by orior's side, the window's own where `own`, else the tree's: `args` handed over as the
/// page sent them, and the answer handed back to the page as orior's side wrote it, or why it failed. The one copy
/// is of the answer into the body the page's IPC takes and owns.
pub fn passed(own: bool, name: &str, args: &[u8]) -> Result<tauri::ipc::Response, String> {
    let mut named = [0u8; NAME_MOST];
    if name.len() >= NAME_MOST || name.as_bytes().contains(&0) {
        return Err(format!("{name}: no call is named so"));
    }
    named[..name.len()].copy_from_slice(name.as_bytes());
    // SAFETY: the name ends in its 0 and the arguments are `len` bytes, each valid while the call runs.
    let room = unsafe { cli_window_called(own as c_int, named.as_ptr() as *const c_char, args.as_ptr() as *const c_char, args.len() as c_ulonglong) };
    let mut text: *const c_char = std::ptr::null();
    let mut size: c_ulonglong = 0;
    // SAFETY: the room is the one the call was answered in, read before it is given back.
    let answered = unsafe { cli_window_answer(room, &mut text, &mut size) } != 0;
    let bytes = unsafe { bytes_at(text, size) };
    let handed = if answered {
        String::from_utf8(bytes.to_vec()).map(tauri::ipc::Response::new).map_err(|error| error.to_string())
    } else {
        Err(String::from_utf8_lossy(bytes).into_owned())
    };
    // SAFETY: the room's answer is read, and nothing reads it after.
    unsafe { cli_window_given(room) };
    handed
}

/// A file a page window asks for, its answer made by `answer` while orior's side holds it.
struct Answering<F, T> {
    answer: Option<F>,
    answered: Option<T>,
}

/// The file orior's side hands over for a page window, its answer made once.
extern "C" fn view_handed<F: FnOnce(u16, &str, &[u8]) -> T, T>(context: *mut c_void, status: c_uint, media_type: *const c_char, bytes: *const c_char, size: c_ulonglong) {
    // SAFETY: the context is the Answering `view_served` lent for the call; the media type is a C text and the bytes
    // `size` of them, each valid while the call runs.
    let served = unsafe { &mut *(context as *mut Answering<F, T>) };
    let media_type = unsafe { std::ffi::CStr::from_ptr(media_type) }.to_str().unwrap_or("application/octet-stream");
    let bytes = unsafe { bytes_at(bytes, size) };
    if let Some(answer) = served.answer.take() {
        served.answered = Some(answer(u16::try_from(status).unwrap_or(500), media_type, bytes));
    }
}

/// The file a page window asks for by its address's path, `answer` given its status, its media type and its bytes while
/// orior's side holds them: what `answer` made, or none where orior's side handed nothing.
pub fn view_served<F: FnOnce(u16, &str, &[u8]) -> T, T>(path: &str, answer: F) -> Option<T> {
    let mut served = Answering { answer: Some(answer), answered: None };
    // SAFETY: the path is `len` bytes and the context the Answering above, each valid while the call runs.
    unsafe { cli_window_view_served(path.as_ptr() as *const c_char, path.len() as c_ulonglong, view_handed::<F, T>, &mut served as *mut Answering<F, T> as *mut c_void) };
    served.answered
}
