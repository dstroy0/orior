// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The container's transfer layer: what is too large for a message, a file's text, a statement's
//! rows, a run's output, goes through memory shared with the process it is for, and the message
//! carries only its length and the means to reach it.
//!
//! A region has no name another process could find. On Windows it is a section of the paging file,
//! and the container makes its handle in the other process itself, as a broker does: a process of the
//! container's hands over a region by the handle it has, and is handed one by a handle made for it,
//! which reads the region and cannot write it. On Linux a region is a memory file sealed against every
//! change once written, and on macOS a file in the reader's own temporary folder, unlinked as it is
//! made; either goes over the door's socket with the frame that names it, and a region from another
//! process is read only once its seals hold, or on macOS read out whole.

/// The least a body is, in bytes, that goes through the transfer layer; a smaller one goes in its
/// frame.
pub const SMALL: usize = 64 << 10;

/// The most a region holds, in bytes.
pub const MOST: usize = 1 << 30;

#[cfg(windows)]
pub use windows::Region;

#[cfg(unix)]
pub use unix::{receive, send_with, Fds, Region};

#[cfg(windows)]
mod windows {
    use windows_sys::Win32::Foundation::{CloseHandle, DuplicateHandle, HANDLE, INVALID_HANDLE_VALUE};
    use windows_sys::Win32::System::Memory::{CreateFileMappingW, MapViewOfFile, UnmapViewOfFile, FILE_MAP_READ, FILE_MAP_WRITE, MEMORY_MAPPED_VIEW_ADDRESS, PAGE_READWRITE};
    use windows_sys::Win32::System::Threading::{GetCurrentProcess, OpenProcess, PROCESS_DUP_HANDLE};

    /// A region of memory shared with another process: its section, and this process's view of it.
    pub struct Region {
        section: HANDLE,
        view: *mut u8,
        length: usize,
    }

    // SAFETY: the section and the view are this value's alone, and the view is only read once made.
    unsafe impl Send for Region {}
    unsafe impl Sync for Region {}

    impl Region {
        /// A region holding `bytes`, in the paging file.
        pub fn with(bytes: &[u8]) -> Result<Region, String> {
            if bytes.len() > super::MOST {
                return Err(format!("{} bytes is past the most a region holds", bytes.len()));
            }
            let size = bytes.len().max(1);
            // SAFETY: the section is checked before use and closed on each path that fails; the view
            // is as long as the section, and the bytes copied into it are no longer.
            unsafe {
                let section = CreateFileMappingW(INVALID_HANDLE_VALUE, std::ptr::null(), PAGE_READWRITE, (size >> 32) as u32, size as u32, std::ptr::null());
                if section.is_null() {
                    return Err(format!("the system made no region: {}", std::io::Error::last_os_error()));
                }
                let view = MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size).Value.cast::<u8>();
                if view.is_null() {
                    let failed = std::io::Error::last_os_error();
                    CloseHandle(section);
                    return Err(format!("the system showed no region: {failed}"));
                }
                std::ptr::copy_nonoverlapping(bytes.as_ptr(), view, bytes.len());
                Ok(Region { section, view, length: bytes.len() })
            }
        }

        /// The region's handle in this process.
        pub fn handle(&self) -> u64 {
            self.section as u64
        }

        /// Makes a handle to the region in the process `process`, which reads the region and cannot
        /// write it, and gives its value there.
        pub fn give(&self, process: u32) -> Result<u64, String> {
            // SAFETY: the process's handle is checked and closed; the handle made is the other
            // process's, and only its value is kept.
            unsafe {
                let target = OpenProcess(PROCESS_DUP_HANDLE, 0, process);
                if target.is_null() {
                    return Err(format!("process {process} was not opened: {}", std::io::Error::last_os_error()));
                }
                let mut theirs: HANDLE = std::ptr::null_mut();
                let made = DuplicateHandle(GetCurrentProcess(), self.section, target, &mut theirs, FILE_MAP_READ, 0, 0);
                let failed = std::io::Error::last_os_error();
                CloseHandle(target);
                if made == 0 {
                    return Err(format!("the region was not given to process {process}: {failed}"));
                }
                Ok(theirs as u64)
            }
        }

        /// The region of `length` bytes the process `process` names by its handle `handle` there,
        /// taken into this process to be read.
        pub fn take(process: u32, handle: u64, length: usize) -> Result<Region, String> {
            // SAFETY: the process's handle is checked and closed; the handle made is this process's,
            // and mapped() closes it where it is no region of `length` bytes.
            unsafe {
                let source = OpenProcess(PROCESS_DUP_HANDLE, 0, process);
                if source.is_null() {
                    return Err(format!("process {process} was not opened: {}", std::io::Error::last_os_error()));
                }
                let mut own: HANDLE = std::ptr::null_mut();
                let made = DuplicateHandle(source, handle as HANDLE, GetCurrentProcess(), &mut own, FILE_MAP_READ, 0, 0);
                let failed = std::io::Error::last_os_error();
                CloseHandle(source);
                if made == 0 {
                    return Err(format!("the region of process {process} was not taken: {failed}"));
                }
                Region::mapped(own, length)
            }
        }

        /// The region of `length` bytes the container made in this process as the handle `handle`.
        pub fn given(handle: u64, length: usize) -> Result<Region, String> {
            Region::mapped(handle as HANDLE, length)
        }

        fn mapped(section: HANDLE, length: usize) -> Result<Region, String> {
            if length > super::MOST {
                // SAFETY: the handle is this process's, and closed once.
                unsafe { CloseHandle(section) };
                return Err(format!("{length} bytes is past the most a region holds"));
            }
            // SAFETY: a view longer than the section is refused by the system, and the handle is
            // closed where it is.
            unsafe {
                let view = MapViewOfFile(section, FILE_MAP_READ, 0, 0, length.max(1)).Value.cast::<u8>();
                if view.is_null() {
                    let failed = std::io::Error::last_os_error();
                    CloseHandle(section);
                    return Err(format!("no region of {length} bytes: {failed}"));
                }
                Ok(Region { section, view, length })
            }
        }

        pub fn len(&self) -> usize {
            self.length
        }

        pub fn is_empty(&self) -> bool {
            self.length == 0
        }

        pub fn bytes(&self) -> &[u8] {
            // SAFETY: the view is at least `length` bytes and lives as long as the region.
            unsafe { std::slice::from_raw_parts(self.view, self.length) }
        }
    }

    impl Drop for Region {
        fn drop(&mut self) {
            // SAFETY: the view and the section are this region's alone, and let go once.
            unsafe {
                UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS { Value: self.view.cast() });
                CloseHandle(self.section);
            }
        }
    }
}

#[cfg(unix)]
mod unix {
    use std::collections::VecDeque;
    use std::ffi::c_void;
    use std::sync::Mutex;

    unsafe extern "C" {
        fn mmap(address: *mut c_void, length: usize, protect: i32, flags: i32, fd: i32, offset: i64) -> *mut c_void;
        fn munmap(address: *mut c_void, length: usize) -> i32;
        #[cfg(target_os = "linux")]
        fn ftruncate(fd: i32, length: i64) -> i32;
        fn lseek(fd: i32, offset: i64, whence: i32) -> i64;
        fn close(fd: i32) -> i32;
        fn fcntl(fd: i32, command: i32, ...) -> i32;
        fn sendmsg(socket: i32, message: *const Header, flags: i32) -> isize;
        fn recvmsg(socket: i32, message: *mut Header, flags: i32) -> isize;
        fn send(socket: i32, buffer: *const c_void, length: usize, flags: i32) -> isize;
        #[cfg(target_os = "linux")]
        fn memfd_create(name: *const std::ffi::c_char, flags: u32) -> i32;
        #[cfg(not(target_os = "linux"))]
        fn pread(fd: i32, buffer: *mut c_void, length: usize, offset: i64) -> isize;
    }

    const PROT_READ: i32 = 1;
    const PROT_WRITE: i32 = 2;
    const MAP_SHARED: i32 = 1;
    const SEEK_END: i32 = 2;
    const F_SETFD: i32 = 2;
    const FD_CLOEXEC: i32 = 1;
    const SCM_RIGHTS: i32 = 1;
    #[cfg(target_os = "linux")]
    const SOL_SOCKET: i32 = 1;
    #[cfg(not(target_os = "linux"))]
    const SOL_SOCKET: i32 = 0xffff;
    #[cfg(target_os = "linux")]
    const SEND_FLAGS: i32 = 0x4000;
    #[cfg(not(target_os = "linux"))]
    const SEND_FLAGS: i32 = 0;
    #[cfg(target_os = "linux")]
    const RECEIVE_FLAGS: i32 = 0x4000_0000;
    #[cfg(not(target_os = "linux"))]
    const RECEIVE_FLAGS: i32 = 0;

    /// The seals a region from another process holds before it is read: against writing, shrinking
    /// and growing, and against any of those seals taken off.
    #[cfg(target_os = "linux")]
    const SEALED: i32 = 1 | 2 | 4 | 8;
    #[cfg(target_os = "linux")]
    const F_ADD_SEALS: i32 = 1033;
    #[cfg(target_os = "linux")]
    const F_GET_SEALS: i32 = 1034;

    #[repr(C)]
    struct Piece {
        base: *mut c_void,
        length: usize,
    }

    /// The system's msghdr: on Linux its counts are a word wide, on macOS half a word.
    #[repr(C)]
    #[cfg(target_os = "linux")]
    struct Header {
        name: *mut c_void,
        name_length: u32,
        pieces: *mut Piece,
        piece_count: usize,
        control: *mut c_void,
        control_length: usize,
        flags: i32,
    }

    #[repr(C)]
    #[cfg(not(target_os = "linux"))]
    struct Header {
        name: *mut c_void,
        name_length: u32,
        pieces: *mut Piece,
        piece_count: i32,
        control: *mut c_void,
        control_length: u32,
        flags: i32,
    }

    /// The length of a cmsghdr, its first field, and its whole size; on Linux the length is a word and
    /// the data starts on a word, on macOS both on four bytes.
    #[cfg(target_os = "linux")]
    const CONTROL_HEAD: usize = 16;
    #[cfg(not(target_os = "linux"))]
    const CONTROL_HEAD: usize = 12;
    #[cfg(target_os = "linux")]
    const CONTROL_ALIGN: usize = 8;
    #[cfg(not(target_os = "linux"))]
    const CONTROL_ALIGN: usize = 4;

    const fn aligned(length: usize) -> usize {
        (length + CONTROL_ALIGN - 1) & !(CONTROL_ALIGN - 1)
    }

    /// The descriptors that came with the bytes read from a socket and are not yet taken, in the
    /// order they came; those never taken are closed with it.
    #[derive(Default)]
    pub struct Fds(Mutex<VecDeque<i32>>);

    impl Fds {
        pub fn take(&self) -> Option<i32> {
            self.0.lock().ok()?.pop_front()
        }

        fn keep(&self, fd: i32) {
            match self.0.lock() {
                Ok(mut kept) => kept.push_back(fd),
                // SAFETY: the descriptor came with the bytes and is this process's to close.
                Err(_) => unsafe {
                    close(fd);
                },
            }
        }
    }

    impl Drop for Fds {
        fn drop(&mut self) {
            if let Ok(kept) = self.0.get_mut() {
                for fd in kept.drain(..) {
                    // SAFETY: each descriptor kept is this process's, and closed once.
                    unsafe { close(fd) };
                }
            }
        }
    }

    /// Writes `frame` to `socket`, the descriptor `fd` going with its first byte.
    pub fn send_with(socket: i32, frame: &[u8], fd: i32) -> std::io::Result<()> {
        let Some((first, rest)) = frame.split_first() else { return Ok(()) };
        let mut control = [0u64; 4];
        let mut piece = Piece { base: (first as *const u8).cast_mut().cast(), length: 1 };
        let length = CONTROL_HEAD + 4;
        // SAFETY: the control buffer is longer than one header and one descriptor, and aligned for a
        // word; the header is written field by field at the offsets the system reads.
        unsafe {
            let base = control.as_mut_ptr().cast::<u8>();
            if CONTROL_HEAD == 16 {
                base.cast::<usize>().write(length);
                base.add(8).cast::<i32>().write(SOL_SOCKET);
                base.add(12).cast::<i32>().write(SCM_RIGHTS);
            } else {
                base.cast::<u32>().write(length as u32);
                base.add(4).cast::<i32>().write(SOL_SOCKET);
                base.add(8).cast::<i32>().write(SCM_RIGHTS);
            }
            base.add(CONTROL_HEAD).cast::<i32>().write_unaligned(fd);
            let header = Header {
                name: std::ptr::null_mut(),
                name_length: 0,
                pieces: &mut piece,
                piece_count: 1,
                control: control.as_mut_ptr().cast(),
                control_length: aligned(length) as _,
                flags: 0,
            };
            loop {
                if sendmsg(socket, &header, SEND_FLAGS) == 1 {
                    break;
                }
                let error = std::io::Error::last_os_error();
                if error.kind() != std::io::ErrorKind::Interrupted {
                    return Err(error);
                }
            }
            let mut sent = 0;
            while sent < rest.len() {
                let wrote = send(socket, rest[sent..].as_ptr().cast(), rest.len() - sent, SEND_FLAGS);
                if wrote < 0 {
                    let error = std::io::Error::last_os_error();
                    if error.kind() == std::io::ErrorKind::Interrupted {
                        continue;
                    }
                    return Err(error);
                }
                sent += wrote as usize;
            }
        }
        Ok(())
    }

    /// Reads into `buffer` from `socket` as read does, keeping each descriptor that comes with the
    /// bytes in `fds`.
    pub fn receive(socket: i32, buffer: &mut [u8], fds: &Fds) -> std::io::Result<usize> {
        let mut control = [0u64; 16];
        let mut piece = Piece { base: buffer.as_mut_ptr().cast(), length: buffer.len() };
        let mut header = Header {
            name: std::ptr::null_mut(),
            name_length: 0,
            pieces: &mut piece,
            piece_count: 1,
            control: control.as_mut_ptr().cast(),
            control_length: std::mem::size_of_val(&control) as _,
            flags: 0,
        };
        // SAFETY: the buffer and the control buffer are as long as the header says; the system
        // writes no further, and each header read lies within the length it gives back.
        unsafe {
            let read = loop {
                let read = recvmsg(socket, &mut header, RECEIVE_FLAGS);
                if read >= 0 {
                    break read as usize;
                }
                let error = std::io::Error::last_os_error();
                if error.kind() != std::io::ErrorKind::Interrupted {
                    return Err(error);
                }
            };
            let filled = (header.control_length as usize).min(std::mem::size_of_val(&control));
            let base = control.as_ptr().cast::<u8>();
            let mut at = 0;
            while at + CONTROL_HEAD <= filled {
                let (length, level, kind) = if CONTROL_HEAD == 16 {
                    (base.add(at).cast::<usize>().read(), base.add(at + 8).cast::<i32>().read(), base.add(at + 12).cast::<i32>().read())
                } else {
                    (base.add(at).cast::<u32>().read() as usize, base.add(at + 4).cast::<i32>().read(), base.add(at + 8).cast::<i32>().read())
                };
                if length < CONTROL_HEAD || at + length > filled {
                    break;
                }
                if level == SOL_SOCKET && kind == SCM_RIGHTS {
                    for one in 0..(length - CONTROL_HEAD) / 4 {
                        let fd = base.add(at + CONTROL_HEAD + one * 4).cast::<i32>().read_unaligned();
                        fcntl(fd, F_SETFD, FD_CLOEXEC);
                        fds.keep(fd);
                    }
                }
                at += aligned(length);
            }
            Ok(read)
        }
    }

    /// A region of memory shared with another process: its descriptor, and this process's view of
    /// it, or what was read out of it.
    pub struct Region {
        fd: i32,
        view: *mut u8,
        length: usize,
        read: Option<Vec<u8>>,
    }

    // SAFETY: the descriptor and the view are this value's alone, and the view is only read once made.
    unsafe impl Send for Region {}
    unsafe impl Sync for Region {}

    /// A descriptor of new memory of `size` bytes no other process can find.
    #[cfg(target_os = "linux")]
    fn made(size: usize) -> Result<i32, String> {
        // MFD_CLOEXEC and MFD_ALLOW_SEALING.
        // SAFETY: the name is a string ending in a nul.
        let fd = unsafe { memfd_create(c"orior-transfer".as_ptr(), 1 | 2) };
        if fd < 0 {
            return Err(format!("the system made no region: {}", std::io::Error::last_os_error()));
        }
        // SAFETY: the descriptor is this process's.
        if unsafe { ftruncate(fd, size as i64) } != 0 {
            let failed = std::io::Error::last_os_error();
            unsafe { close(fd) };
            return Err(format!("the region was not sized: {failed}"));
        }
        Ok(fd)
    }

    #[cfg(not(target_os = "linux"))]
    fn made(size: usize) -> Result<i32, String> {
        use std::os::unix::fs::OpenOptionsExt;
        use std::os::unix::io::IntoRawFd;
        let secret: String = crate::comms::secret(8)?.iter().map(|byte| format!("{byte:02x}")).collect();
        let path = std::env::temp_dir().join(format!("orior-transfer-{}-{secret}", std::process::id()));
        let file = std::fs::OpenOptions::new().read(true).write(true).create_new(true).mode(0o600).open(&path).map_err(|error| format!("{}: {error}", path.display()))?;
        let _ = std::fs::remove_file(&path);
        file.set_len(size as u64).map_err(|error| format!("the region was not sized: {error}"))?;
        Ok(file.into_raw_fd())
    }

    impl Region {
        /// A region holding `bytes`, sealed on Linux against every change from now on.
        pub fn with(bytes: &[u8]) -> Result<Region, String> {
            if bytes.len() > super::MOST {
                return Err(format!("{} bytes is past the most a region holds", bytes.len()));
            }
            let size = bytes.len().max(1);
            let fd = made(size)?;
            // SAFETY: the view is as long as the region, the bytes copied into it no longer, and the
            // descriptor is closed on each path that fails.
            unsafe {
                let view = mmap(std::ptr::null_mut(), size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                if view as isize == -1 {
                    let failed = std::io::Error::last_os_error();
                    close(fd);
                    return Err(format!("the system showed no region: {failed}"));
                }
                std::ptr::copy_nonoverlapping(bytes.as_ptr(), view.cast::<u8>(), bytes.len());
                #[cfg(target_os = "linux")]
                {
                    // A region is sealed against writing only once no view of it can write.
                    munmap(view, size);
                    if fcntl(fd, F_ADD_SEALS, SEALED) != 0 {
                        let failed = std::io::Error::last_os_error();
                        close(fd);
                        return Err(format!("the region was not sealed: {failed}"));
                    }
                    let view = mmap(std::ptr::null_mut(), size, PROT_READ, MAP_SHARED, fd, 0);
                    if view as isize == -1 {
                        let failed = std::io::Error::last_os_error();
                        close(fd);
                        return Err(format!("the system showed no region: {failed}"));
                    }
                    Ok(Region { fd, view: view.cast(), length: bytes.len(), read: None })
                }
                #[cfg(not(target_os = "linux"))]
                {
                    Ok(Region { fd, view: view.cast(), length: bytes.len(), read: None })
                }
            }
        }

        pub fn fd(&self) -> i32 {
            self.fd
        }

        /// The region of `length` bytes whose descriptor `fd` came from another process; the
        /// descriptor is the region's from now on. On Linux it is read in place once its seals hold;
        /// elsewhere it is read out whole, as the process that made it could change it still.
        pub fn received(fd: i32, length: usize) -> Result<Region, String> {
            // SAFETY: the descriptor is this process's, and closed on each path that fails.
            let size = unsafe { lseek(fd, 0, SEEK_END) };
            let refused = |why: String| {
                unsafe { close(fd) };
                Err(why)
            };
            if length > super::MOST {
                return refused(format!("{length} bytes is past the most a region holds"));
            }
            if size < length as i64 {
                return refused(format!("the region holds {size} bytes, and was said to hold {length}"));
            }
            #[cfg(target_os = "linux")]
            {
                // SAFETY: as above; the view is no longer than the region, which no one can shrink.
                unsafe {
                    if fcntl(fd, F_GET_SEALS) & SEALED != SEALED {
                        return refused("the region is not sealed against change".into());
                    }
                    let view = mmap(std::ptr::null_mut(), length.max(1), PROT_READ, MAP_SHARED, fd, 0);
                    if view as isize == -1 {
                        return refused(format!("the system showed no region: {}", std::io::Error::last_os_error()));
                    }
                    Ok(Region { fd, view: view.cast(), length, read: None })
                }
            }
            #[cfg(not(target_os = "linux"))]
            {
                let mut read = vec![0u8; length];
                let mut at = 0;
                while at < length {
                    // SAFETY: the buffer is as long as the length asked for past `at`.
                    let got = unsafe { pread(fd, read[at..].as_mut_ptr().cast(), length - at, at as i64) };
                    if got <= 0 {
                        return refused(format!("the region ended at {at} of its {length} bytes"));
                    }
                    at += got as usize;
                }
                Ok(Region { fd, view: std::ptr::null_mut(), length, read: Some(read) })
            }
        }

        pub fn len(&self) -> usize {
            self.length
        }

        pub fn is_empty(&self) -> bool {
            self.length == 0
        }

        pub fn bytes(&self) -> &[u8] {
            match &self.read {
                Some(read) => read,
                // SAFETY: the view is at least `length` bytes and lives as long as the region.
                None => unsafe { std::slice::from_raw_parts(self.view, self.length) },
            }
        }
    }

    impl Drop for Region {
        fn drop(&mut self) {
            // SAFETY: the view and the descriptor are this region's alone, and let go once.
            unsafe {
                if !self.view.is_null() {
                    munmap(self.view.cast(), self.length.max(1));
                }
                close(self.fd);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pattern(length: usize) -> Vec<u8> {
        (0..length).map(|at| (at * 31 % 251) as u8).collect()
    }

    #[test]
    fn a_region_handed_over_holds_what_was_written() {
        let bytes = pattern(SMALL * 3 + 7);
        let region = Region::with(&bytes).unwrap();
        assert_eq!(region.bytes(), &bytes[..]);
        #[cfg(windows)]
        let taken = Region::take(std::process::id(), region.handle(), bytes.len()).unwrap();
        #[cfg(unix)]
        let taken = {
            unsafe extern "C" {
                fn dup(fd: i32) -> i32;
            }
            // SAFETY: the descriptor is the region's, and the copy is the taken region's.
            Region::received(unsafe { dup(region.fd()) }, bytes.len()).unwrap()
        };
        drop(region);
        assert_eq!(taken.bytes(), &bytes[..], "the region outlives the handle it was made by");
    }

    #[test]
    fn a_region_said_to_hold_more_than_it_does_is_refused() {
        let region = Region::with(&pattern(4096)).unwrap();
        #[cfg(windows)]
        let refused = Region::take(std::process::id(), region.handle(), 1 << 20).err().unwrap();
        #[cfg(unix)]
        let refused = {
            unsafe extern "C" {
                fn dup(fd: i32) -> i32;
            }
            // SAFETY: as above.
            Region::received(unsafe { dup(region.fd()) }, 1 << 20).err().unwrap()
        };
        assert!(!refused.is_empty());
    }

    #[cfg(windows)]
    #[test]
    fn a_region_given_to_a_process_reads_and_cannot_write() {
        use windows_sys::Win32::System::Memory::{MapViewOfFile, FILE_MAP_WRITE};
        let region = Region::with(&pattern(100)).unwrap();
        let given = region.give(std::process::id()).unwrap();
        // SAFETY: the handle was made in this process, and a view the system refuses is null.
        let written = unsafe { MapViewOfFile(given as _, FILE_MAP_WRITE, 0, 0, 100).Value };
        assert!(written.is_null(), "a given region cannot be written");
        assert_eq!(Region::given(given, 100).unwrap().bytes(), &pattern(100)[..]);
    }

    #[cfg(target_os = "linux")]
    #[test]
    fn a_region_is_sealed_against_every_change_once_written() {
        unsafe extern "C" {
            fn ftruncate(fd: i32, length: i64) -> i32;
        }
        let region = Region::with(&pattern(100)).unwrap();
        // SAFETY: the descriptor is the region's; the call fails, as the test asks.
        assert_ne!(unsafe { ftruncate(region.fd(), 10) }, 0, "a sealed region cannot be shrunk");
    }
}
