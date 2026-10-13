// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A file's bytes mapped into the process to be read where they lie, with no copy of them made: the
//! system's own cache of the file is what is read. The file is opened and mapped and let go at once,
//! the view holding what it needs; a page of it is brought in where it is first read, by whatever
//! thread reads it. A file under MAPPED_FROM, or one that cannot be mapped, is read whole, and an empty
//! file is empty bytes.

use std::fs::File;
use std::ops::Deref;
use std::path::Path;

/// The smallest file mapped: a smaller one costs fewer of the system's calls read whole than mapped.
pub const MAPPED_FROM: usize = 256 * 1024;

/// A file's bytes, mapped or read.
pub struct Mapped {
    view: *const u8,
    length: usize,
    read: Vec<u8>,
}

// SAFETY: the view is read alone, by one owner at a time, and let go once when that owner drops it.
unsafe impl Send for Mapped {}

impl Mapped {
    /// The bytes of the file at `path`.
    pub fn open(path: &Path) -> std::io::Result<Mapped> {
        let file = File::open(path)?;
        let length = file.metadata()?.len() as usize;
        if length == 0 {
            return Ok(Mapped { view: std::ptr::null(), length: 0, read: Vec::new() });
        }
        if length < MAPPED_FROM {
            let mut read = Vec::with_capacity(length);
            std::io::Read::read_to_end(&mut &file, &mut read)?;
            return Ok(Mapped { view: std::ptr::null(), length: 0, read });
        }
        if let Some(view) = system::map(&file, length) {
            return Ok(Mapped { view, length, read: Vec::new() });
        }
        Ok(Mapped { view: std::ptr::null(), length: 0, read: std::fs::read(path)? })
    }
}

impl Deref for Mapped {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        if self.view.is_null() {
            &self.read
        } else {
            // SAFETY: the view is `length` bytes mapped to be read, and stays until this drops.
            unsafe { std::slice::from_raw_parts(self.view, self.length) }
        }
    }
}

impl Drop for Mapped {
    fn drop(&mut self) {
        if !self.view.is_null() {
            system::unmap(self.view, self.length);
        }
    }
}

#[cfg(windows)]
mod system {
    use std::fs::File;
    use std::os::windows::io::AsRawHandle;
    use windows_sys::Win32::Foundation::CloseHandle;
    use windows_sys::Win32::System::Memory::{CreateFileMappingW, MapViewOfFile, UnmapViewOfFile, FILE_MAP_READ, MEMORY_MAPPED_VIEW_ADDRESS, PAGE_READONLY};

    /// The whole of `file`, of `length` bytes, mapped to be read.
    pub fn map(file: &File, length: usize) -> Option<*const u8> {
        let _ = length;
        // SAFETY: the handle is the open file's; the section is let go once the view holds it.
        unsafe {
            let section = CreateFileMappingW(file.as_raw_handle(), std::ptr::null(), PAGE_READONLY, 0, 0, std::ptr::null());
            if section.is_null() {
                return None;
            }
            let view = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);
            CloseHandle(section);
            (!view.Value.is_null()).then_some(view.Value as *const u8)
        }
    }

    pub fn unmap(view: *const u8, _length: usize) {
        // SAFETY: the view is one `map` gave, let go once.
        unsafe {
            UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS { Value: view as *mut _ });
        }
    }
}

#[cfg(unix)]
mod system {
    use std::ffi::{c_int, c_void};
    use std::fs::File;
    use std::os::unix::io::AsRawFd;

    unsafe extern "C" {
        fn mmap(at: *mut c_void, length: usize, protection: c_int, flags: c_int, file: c_int, offset: i64) -> *mut c_void;
        fn munmap(at: *mut c_void, length: usize) -> c_int;
    }

    /// The whole of `file`, of `length` bytes, mapped to be read: PROT_READ, MAP_PRIVATE.
    pub fn map(file: &File, length: usize) -> Option<*const u8> {
        // SAFETY: the descriptor is the open file's, and the mapping outlives it as mmap allows.
        let view = unsafe { mmap(std::ptr::null_mut(), length, 1, 2, file.as_raw_fd(), 0) };
        (view as usize != usize::MAX).then_some(view as *const u8)
    }

    pub fn unmap(view: *const u8, length: usize) {
        // SAFETY: the view is one `map` gave, of `length` bytes, let go once.
        unsafe {
            munmap(view as *mut c_void, length);
        }
    }
}

#[cfg(not(any(windows, unix)))]
mod system {
    use std::fs::File;

    pub fn map(_file: &File, _length: usize) -> Option<*const u8> {
        None
    }

    pub fn unmap(_view: *const u8, _length: usize) {}
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_file_mapped_reads_as_its_bytes_and_an_empty_one_as_none() {
        let folder = std::env::temp_dir().join(format!("orior-mapped-{}", std::process::id()));
        std::fs::create_dir_all(&folder).unwrap();
        let full = folder.join("bytes.txt");
        for length in [70000u32, MAPPED_FROM as u32 + 4099] {
            let bytes: Vec<u8> = (0..length).map(|at| (at % 251) as u8).collect();
            std::fs::write(&full, &bytes).unwrap();
            assert_eq!(&*Mapped::open(&full).unwrap(), &bytes[..], "{length} bytes");
        }
        let empty = folder.join("empty.txt");
        std::fs::write(&empty, b"").unwrap();
        assert!(Mapped::open(&empty).unwrap().is_empty());
        assert!(Mapped::open(&folder.join("absent.txt")).is_err());
        std::fs::remove_dir_all(&folder).unwrap();
    }
}
