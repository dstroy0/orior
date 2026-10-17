// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! This process as a member of a run (`qry_buffer.h`), the buffer compiled in by `build.rs`: a blob
//! handed goes into this process's own ring in the buffer, which puts the process on the writer's
//! turn, and a blob read is read in the buffer where it stands. Nothing is copied on the way and no
//! process stands between this one and the writer. A finished run's `.qry` is read through the
//! same library, mapped.

use std::ffi::{c_char, c_int, c_uint, c_void};
use std::path::Path;
use std::sync::{Arc, Mutex};

use crate::writer::Priority;

#[repr(C)]
pub struct QryBuffer {
    _opaque: [u8; 0],
}

unsafe extern "C" {
    fn qry_open(path: *const c_char) -> *mut QryBuffer;
    fn qry_close(buffer: *mut QryBuffer);
    fn qry_hand(buffer: *mut QryBuffer, name: *const c_char, bytes: *const c_void, size: u64, priority: c_uint) -> u64;
    fn qry_moved(buffer: *mut QryBuffer) -> u64;
    fn qry_latest(buffer: *mut QryBuffer, name: *const c_char, bytes: *mut *const u8, size: *mut u64, sequence: *mut u64) -> c_int;
    fn qry_joined(buffer: *mut QryBuffer, name: *const c_char, bytes: *mut *mut u8, size: *mut u64) -> c_int;
    fn qry_file_read(buffer: *mut QryBuffer, path: *const c_char, bytes: *mut *mut u8, size: *mut u64) -> c_int;
    fn qry_file_joined(path: *const c_char, name: *const c_char, bytes: *mut *mut u8, size: *mut u64, error: *mut c_char, room: usize) -> c_int;
    fn qry_file_latest_each(path: *const c_char, each: unsafe extern "C" fn(*mut c_void, *const c_char, *const u8, u64), context: *mut c_void, error: *mut c_char, room: usize) -> c_int;
    fn malloc(size: usize) -> *mut c_void;
    fn free(pointer: *mut c_void);
}

/// A name or a path as the buffer takes it: the bytes and a NUL, in room on the stack.
struct Text {
    bytes: [u8; 1024],
}

impl Text {
    fn of(text: &str) -> Option<Text> {
        if text.len() >= 1024 || text.bytes().any(|byte| byte == 0) {
            return None;
        }
        let mut bytes = [0u8; 1024];
        bytes[..text.len()].copy_from_slice(text.as_bytes());
        Some(Text { bytes })
    }

    fn of_path(path: &Path) -> Option<Text> {
        Text::of(path.to_str()?)
    }

    fn as_ptr(&self) -> *const c_char {
        self.bytes.as_ptr().cast()
    }
}

/// Bytes the buffer's library gave in memory of this process's, freed with them.
pub struct Freed {
    bytes: *mut u8,
    size: usize,
}

impl Freed {
    /// `bytes` copied into memory of this process's.
    pub fn of(bytes: &[u8]) -> Option<Freed> {
        Freed::copied(bytes.as_ptr(), bytes.len() as u64)
    }

    /// `size` bytes copied into memory of this process's, so that bytes standing for one call alone
    /// outlive it.
    fn copied(bytes: *const u8, size: u64) -> Option<Freed> {
        let size = usize::try_from(size).ok()?;
        if size == 0 {
            return Some(Freed { bytes: std::ptr::null_mut(), size: 0 });
        }
        // the room asked is the count given, from the C library's own heap
        let room = unsafe { malloc(size) }.cast::<u8>();
        if room.is_null() {
            return None;
        }
        // `bytes` holds `size` bytes for this call, and `room` holds `size`
        unsafe { std::ptr::copy_nonoverlapping(bytes, room, size) };
        Some(Freed { bytes: room, size })
    }
}

impl std::ops::Deref for Freed {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        if self.bytes.is_null() {
            return &[];
        }
        // the library gave `size` bytes at `bytes`, which stand until they are freed here
        unsafe { std::slice::from_raw_parts(self.bytes, self.size) }
    }
}

impl Drop for Freed {
    fn drop(&mut self) {
        if !self.bytes.is_null() {
            // memory the C library's malloc gave, freed once
            unsafe { free(self.bytes.cast()) };
        }
    }
}

/// Bytes of a blob: in the buffer where this process holds it, or freed where they were read out of a file.
pub enum Bytes<'a> {
    Held(&'a [u8]),
    Freed(Freed),
}

impl std::ops::Deref for Bytes<'_> {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        match self {
            Bytes::Held(bytes) => bytes,
            Bytes::Freed(freed) => freed,
        }
    }
}

/// The run's buffer open in this process.
pub struct Member {
    buffer: *mut QryBuffer,
}

// the buffer's words are taken from one thread of this process at a time; the library locks the buffer against
// other processes itself
unsafe impl Send for Member {}
unsafe impl Sync for Member {}

fn priority_of(priority: Priority) -> c_uint {
    match priority {
        Priority::Panic => 0,
        Priority::Critical => 1,
        Priority::Ordinary => 2,
    }
}

impl Member {
    /// The buffer of the `.qry` at `qry` opened, its writer running: None where no writer runs there.
    pub fn opened(qry: &Path) -> Option<Member> {
        let path = Text::of_path(qry)?;
        // the path is NUL ended in its room
        let buffer = unsafe { qry_open(path.as_ptr()) };
        if buffer.is_null() { None } else { Some(Member { buffer }) }
    }

    /// `bytes` handed as one blob of `name` at `priority`: 1 where the writer takes it.
    pub fn hand(&self, name: &str, bytes: &[u8], priority: Priority) -> bool {
        let Some(name) = Text::of(name) else { return false };
        // the buffer is open, the name NUL ended, and `bytes` holds its count
        unsafe { qry_hand(self.buffer, name.as_ptr(), bytes.as_ptr().cast(), bytes.len() as u64, priority_of(priority)) != 0 }
    }

    /// `line` and a line's end handed as one blob of `name`.
    pub fn hand_line(&self, name: &str, line: &str) -> bool {
        let mut room = [0u8; 8192];
        if line.len() < room.len() {
            room[..line.len()].copy_from_slice(line.as_bytes());
            room[line.len()] = b'\n';
            return self.hand(name, &room[..line.len() + 1], Priority::Ordinary);
        }
        self.hand(name, line.as_bytes(), Priority::Ordinary) && self.hand(name, b"\n", Priority::Ordinary)
    }

    /// Every blob this process handed moved into the run's buffer: taken before another process is to read them.
    pub fn moved(&self) -> u64 {
        // the buffer is open
        unsafe { qry_moved(self.buffer) }
    }

    /// The latest blob of `name`, where it stands in the buffer.
    pub fn latest(&self, name: &str) -> Option<&[u8]> {
        let name = Text::of(name)?;
        let mut bytes: *const u8 = std::ptr::null();
        let mut size: u64 = 0;
        // the buffer is open and the name NUL ended; the bytes stand while this process holds the buffer
        let found = unsafe { qry_latest(self.buffer, name.as_ptr(), &mut bytes, &mut size, std::ptr::null_mut()) };
        if found == 0 {
            return None;
        }
        let size = usize::try_from(size).ok()?;
        if size == 0 {
            return Some(&[]);
        }
        Some(unsafe { std::slice::from_raw_parts(bytes, size) })
    }

    /// Every blob of `name` joined.
    pub fn joined(&self, name: &str) -> Option<Freed> {
        let name = Text::of(name)?;
        let mut bytes: *mut u8 = std::ptr::null_mut();
        let mut size: u64 = 0;
        // the buffer is open and the name NUL ended; the memory is this process's to free
        let found = unsafe { qry_joined(self.buffer, name.as_ptr(), &mut bytes, &mut size) };
        if found == 0 {
            return None;
        }
        Some(Freed { bytes, size: usize::try_from(size).ok()? })
    }

    /// The file at `path` as the run holds it: its latest blob, or the file where the run holds none.
    pub fn file_read(&self, path: &Path) -> Option<Freed> {
        let path = Text::of_path(path)?;
        let mut bytes: *mut u8 = std::ptr::null_mut();
        let mut size: u64 = 0;
        // the buffer is open and the path NUL ended; the memory is this process's to free
        let found = unsafe { qry_file_read(self.buffer, path.as_ptr(), &mut bytes, &mut size) };
        if found == 0 {
            return None;
        }
        Some(Freed { bytes, size: usize::try_from(size).ok()? })
    }

    /// Every blob of `name` in the finished `.qry` at `qry`, joined.
    pub fn file_joined(qry: &Path, name: &str) -> Option<Freed> {
        let path = Text::of_path(qry)?;
        let name = Text::of(name)?;
        let mut bytes: *mut u8 = std::ptr::null_mut();
        let mut size: u64 = 0;
        let mut error = [0 as c_char; 256];
        // the path and the name are NUL ended and the error has its room; the memory is this process's to free
        let found = unsafe { qry_file_joined(path.as_ptr(), name.as_ptr(), &mut bytes, &mut size, error.as_mut_ptr(), error.len()) };
        if found == 0 {
            return None;
        }
        Some(Freed { bytes, size: usize::try_from(size).ok()? })
    }

    /// The latest blob of `name` in the finished `.qry` at `qry`.
    pub fn file_latest(qry: &Path, name: &str) -> Option<Freed> {
        let path = Text::of_path(qry)?;
        let mut wanted = Wanted { name: Text::of(name)?, length: name.len(), found: None };
        let mut error = [0 as c_char; 256];
        // the path is NUL ended and the error has its room; `wanted` outlives the call
        let read = unsafe { qry_file_latest_each(path.as_ptr(), latest_wanted, (&raw mut wanted).cast(), error.as_mut_ptr(), error.len()) };
        if read == 0 {
            return None;
        }
        wanted.found
    }
}

/// The name looked for over a finished file's latest blobs, and the one found.
struct Wanted {
    name: Text,
    length: usize,
    found: Option<Freed>,
}

unsafe extern "C" fn latest_wanted(context: *mut c_void, name: *const c_char, bytes: *const u8, size: u64) {
    // `context` is the Wanted the caller gave, and `name` a NUL ended name of the file
    let wanted = unsafe { &mut *context.cast::<Wanted>() };
    let given = unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes();
    if given.len() == wanted.length && given == &wanted.name.bytes[..wanted.length] {
        wanted.found = Freed::copied(bytes, size);
    }
}

impl Drop for Member {
    fn drop(&mut self) {
        // the buffer opened by this member, let go once
        unsafe { qry_close(self.buffer) };
    }
}

/// The run this process is a member of now, and the name its own lines go to the run under.
struct Current {
    member: Option<Arc<Member>>,
    console: [u8; 64],
    console_length: usize,
}

static CURRENT: Mutex<Current> = Mutex::new(Current { member: None, console: [0u8; 64], console_length: 0 });

/// `member` made the run this process's own lines go to; None as the run ends.
pub fn current_set(member: Option<Arc<Member>>) {
    if let Ok(mut current) = CURRENT.lock() {
        current.member = member;
    }
}

/// This process's own lines go to the run as `<step>.console`.
pub fn console_named(step: &str) {
    if let Ok(mut current) = CURRENT.lock() {
        let mut name = [0u8; 64];
        let length = step.len().min(64 - 8);
        name[..length].copy_from_slice(&step.as_bytes()[..length]);
        name[length..length + 8].copy_from_slice(b".console");
        current.console = name;
        current.console_length = length + 8;
    }
}

/// `line` handed to the run this process is a member of, as it is said; nothing where it is a member of none.
pub fn said(line: &str) {
    let Ok(current) = CURRENT.lock() else { return };
    let (Some(member), true) = (&current.member, current.console_length != 0) else { return };
    let Ok(console) = std::str::from_utf8(&current.console[..current.console_length]) else { return };
    member.hand_line(console, line);
}
