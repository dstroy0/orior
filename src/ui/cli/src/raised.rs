// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Writing a file the system refuses to write for the reader, once the system has asked the reader for
//! the rights the write needs. The text is put in a file of orior's own first, and copied over the
//! file with the rights: on Windows by orior's own program run again as `orior write-raised`, raised
//! by the system's own prompt, which writes that one file and ends; on Linux by cp run through pkexec;
//! on macOS by cp run with the system's authorization. A reader who declines the prompt leaves the
//! file as it was.

use std::path::{Path, PathBuf};

/// Whether `said`, what a write that failed said, is the system refusing it the rights.
pub fn is_refused(said: &str) -> bool {
    let lower = said.to_ascii_lowercase();
    ["access is denied", "permission denied", "operation not permitted", "os error 5)", "os error 13)", "os error 1)", "read-only file system"].iter().any(|part| lower.contains(part))
}

/// Copies the file `from` over the file `to`, as `orior write-raised` does once raised.
pub fn copy_over(from: &Path, to: &Path) -> Result<(), String> {
    let bytes = std::fs::read(from).map_err(|error| format!("{}: {error}", from.display()))?;
    std::fs::write(to, bytes).map_err(|error| format!("{}: {error}", to.display()))
}

fn staged(text: &[u8]) -> Result<PathBuf, String> {
    let dir = std::env::temp_dir().join("orior-raised");
    std::fs::create_dir_all(&dir).map_err(|error| format!("{}: {error}", dir.display()))?;
    let file = dir.join(format!("{}-{}", std::process::id(), std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|time| time.as_nanos()).unwrap_or(0)));
    std::fs::write(&file, text).map_err(|error| format!("{}: {error}", file.display()))?;
    Ok(file)
}

/// Writes `text` to the file `path` with the rights the system asks the reader for.
pub fn write(path: &Path, text: &[u8]) -> Result<(), String> {
    let from = staged(text)?;
    let done = raise(&from, path);
    let _ = std::fs::remove_file(&from);
    done
}

#[cfg(windows)]
fn raise(from: &Path, to: &Path) -> Result<(), String> {
    use std::ffi::c_void;
    use std::os::windows::ffi::OsStrExt;

    #[repr(C)]
    struct Info {
        size: u32,
        mask: u32,
        window: *mut c_void,
        verb: *const u16,
        file: *const u16,
        parameters: *const u16,
        directory: *const u16,
        show: i32,
        instance: *mut c_void,
        list: *mut c_void,
        class: *const u16,
        class_key: *mut c_void,
        hot_key: u32,
        icon: *mut c_void,
        process: *mut c_void,
    }

    #[link(name = "shell32")]
    unsafe extern "system" {
        fn ShellExecuteExW(info: *mut Info) -> i32;
    }

    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn WaitForSingleObject(handle: *mut c_void, millis: u32) -> u32;
        fn GetExitCodeProcess(process: *mut c_void, code: *mut u32) -> i32;
        fn CloseHandle(handle: isize) -> i32;
        fn GetLastError() -> u32;
    }

    const NO_CLOSE_PROCESS: u32 = 0x40;
    const NO_CONSOLE: u32 = 0x8000;
    const HIDE: i32 = 0;
    const CANCELLED: u32 = 1223;

    let wide = |text: &std::ffi::OsStr| text.encode_wide().chain([0]).collect::<Vec<u16>>();
    let program = std::env::current_exe().map_err(|error| error.to_string())?;
    let quote = |path: &Path| format!("\"{}\"", path.display());
    let parameters = format!("write-raised {} {}", quote(from), quote(to));
    let (verb, file, parameters) = (wide("runas".as_ref()), wide(program.as_os_str()), wide(parameters.as_ref()));
    let mut info = Info {
        size: std::mem::size_of::<Info>() as u32,
        mask: NO_CLOSE_PROCESS | NO_CONSOLE,
        window: std::ptr::null_mut(),
        verb: verb.as_ptr(),
        file: file.as_ptr(),
        parameters: parameters.as_ptr(),
        directory: std::ptr::null(),
        show: HIDE,
        instance: std::ptr::null_mut(),
        list: std::ptr::null_mut(),
        class: std::ptr::null(),
        class_key: std::ptr::null_mut(),
        hot_key: 0,
        icon: std::ptr::null_mut(),
        process: std::ptr::null_mut(),
    };
    if unsafe { ShellExecuteExW(&mut info) } == 0 {
        return Err(match unsafe { GetLastError() } {
            CANCELLED => "the reader did not give the rights".to_string(),
            code => format!("the system's prompt for the rights failed ({code})"),
        });
    }
    let mut code = 1u32;
    unsafe {
        WaitForSingleObject(info.process, u32::MAX);
        GetExitCodeProcess(info.process, &mut code);
        CloseHandle(info.process as isize);
    }
    if code == 0 {
        Ok(())
    } else {
        Err(format!("the raised write of {} failed ({code})", to.display()))
    }
}

#[cfg(target_os = "macos")]
fn raise(from: &Path, to: &Path) -> Result<(), String> {
    let quoted = |path: &Path| format!("'{}'", path.display().to_string().replace('\'', "'\\''"));
    let script = format!("do shell script \"/bin/cp {} {}\" with administrator privileges", quoted(from).replace('"', "\\\""), quoted(to).replace('"', "\\\""));
    let out = std::process::Command::new("/usr/bin/osascript").args(["-e", &script]).output().map_err(|error| format!("osascript: {error}"))?;
    if out.status.success() {
        Ok(())
    } else {
        Err(format!("the system's authorization did not write {}: {}", to.display(), String::from_utf8_lossy(&out.stderr).trim()))
    }
}

#[cfg(all(unix, not(target_os = "macos")))]
fn raise(from: &Path, to: &Path) -> Result<(), String> {
    let status = std::process::Command::new("pkexec").arg("/bin/cp").arg("--").arg(from).arg(to).status().map_err(|error| format!("pkexec: {error}"))?;
    match status.code() {
        Some(0) => Ok(()),
        Some(126) | Some(127) => Err("the reader did not give the rights".into()),
        _ => Err(format!("pkexec did not write {}", to.display())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_refusal_is_told_from_any_other_failure() {
        assert!(is_refused("a.txt: Access is denied. (os error 5)"));
        assert!(is_refused("a.txt: Permission denied (os error 13)"));
        assert!(is_refused("a.txt: Read-only file system (os error 30)"));
        assert!(!is_refused("a.txt: The system cannot find the path specified. (os error 3)"));
        assert!(!is_refused("a.txt: No space left on device (os error 28)"));
    }

    #[test]
    fn the_raised_helper_copies_the_staged_text_over_the_file() {
        let staged = staged(b"new text").unwrap();
        let target = std::env::temp_dir().join(format!("orior_ui_raised_{}.txt", std::process::id()));
        std::fs::write(&target, "old").unwrap();
        copy_over(&staged, &target).unwrap();
        assert_eq!(std::fs::read_to_string(&target).unwrap(), "new text");
        let _ = std::fs::remove_file(&staged);
        let _ = std::fs::remove_file(&target);
    }
}
