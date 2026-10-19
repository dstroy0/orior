// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What the app holds in memory: its own process and every process it started, the web view's
//! browser, renderers and GPU process among them. Each counts its private working set, the memory
//! only it uses that is in RAM, and its commit, the private memory it has reserved whether in RAM or
//! paged out. The app's own process alone is a small share of the whole.

use serde::Serialize;

/// What one program the app started holds, with every process it started in turn: its name, the
/// process the app started, how many processes, what they hold in RAM and what they have reserved,
/// and, for a language server, its toolchain's id.
#[derive(Serialize, Default, Clone)]
pub struct Part {
    pub name: String,
    pub pid: u32,
    pub processes: u32,
    pub working: u64,
    pub commit: u64,
    pub server: Option<String>,
}

#[derive(Serialize, Default)]
pub struct Memory {
    pub working: u64,
    pub commit: u64,
    pub parts: Vec<Part>,
}

#[cfg(windows)]
pub fn read() -> Option<Memory> {
    use std::collections::{BTreeMap, HashMap};
    use windows_sys::Win32::Foundation::{CloseHandle, FILETIME, INVALID_HANDLE_VALUE};
    use windows_sys::Win32::System::Diagnostics::ToolHelp::{CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS};
    use windows_sys::Win32::System::ProcessStatus::{K32GetProcessMemoryInfo, PROCESS_MEMORY_COUNTERS, PROCESS_MEMORY_COUNTERS_EX2};
    use windows_sys::Win32::System::Threading::{GetCurrentProcessId, GetProcessTimes, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION};

    // A process's private working set, commit and start, or nothing where it cannot be read.
    fn measure(pid: u32) -> Option<(u64, u64, u64)> {
        // SAFETY: the handle is checked before use and closed on every path; the counters and the
        // times are plain structures the calls fill, sized as they ask.
        unsafe {
            let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
            if handle.is_null() {
                return None;
            }
            let mut counters: PROCESS_MEMORY_COUNTERS_EX2 = std::mem::zeroed();
            counters.cb = std::mem::size_of::<PROCESS_MEMORY_COUNTERS_EX2>() as u32;
            let read = K32GetProcessMemoryInfo(handle, (&raw mut counters).cast::<PROCESS_MEMORY_COUNTERS>(), counters.cb) != 0;
            let zero = FILETIME { dwLowDateTime: 0, dwHighDateTime: 0 };
            let (mut made, mut ended, mut kernel, mut user) = (zero, zero, zero, zero);
            let timed = GetProcessTimes(handle, &mut made, &mut ended, &mut kernel, &mut user) != 0;
            CloseHandle(handle);
            (read && timed).then(|| (counters.PrivateWorkingSetSize as u64, counters.PrivateUsage as u64, (u64::from(made.dwHighDateTime) << 32) | u64::from(made.dwLowDateTime)))
        }
    }

    // Every process: its parent and its program's name.
    let mut all: HashMap<u32, (u32, String)> = HashMap::new();
    // SAFETY: the snapshot is checked and closed; each entry is sized as the walk asks.
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if snapshot == INVALID_HANDLE_VALUE {
            return None;
        }
        let mut entry: PROCESSENTRY32W = std::mem::zeroed();
        entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;
        let mut more = Process32FirstW(snapshot, &mut entry) != 0;
        while more {
            let end = entry.szExeFile.iter().position(|&unit| unit == 0).unwrap_or(entry.szExeFile.len());
            all.insert(entry.th32ProcessID, (entry.th32ParentProcessID, String::from_utf16_lossy(&entry.szExeFile[..end])));
            more = Process32NextW(snapshot, &mut entry) != 0;
        }
        CloseHandle(snapshot);
    }

    // The app's process, then each process whose parent is one of the app's and that started after
    // it: a process that took an ended parent's number before that parent started is not the app's.
    // SAFETY: a call that takes nothing and cannot fail.
    let own = unsafe { GetCurrentProcessId() };
    let first = measure(own)?;
    let mut found: Vec<(u32, (u64, u64, u64))> = vec![(own, first)];
    let mut at = 0;
    while at < found.len() {
        let (parent, (_, _, parent_start)) = found[at];
        for (&pid, (of, _)) in &all {
            if *of != parent || pid == parent || found.iter().any(|(seen, _)| *seen == pid) {
                continue;
            }
            if let Some(measured) = measure(pid).filter(|&(_, _, start)| start >= parent_start) {
                found.push((pid, measured));
            }
        }
        at += 1;
    }
    // A process of the container's whose parent has ended stands apart from the tree, and counts all
    // the same.
    for pid in crate::orior::own_as::<Vec<u32>>("container_members", serde_json::json!({})).unwrap_or_default() {
        if !found.iter().any(|(seen, _)| *seen == pid) {
            if let Some(measured) = measure(pid) {
                found.push((pid, measured));
            }
        }
    }

    // Each process counts with the process the app started it from: a server's helpers with the
    // server, the web view's renderers with its browser.
    let started_from = |pid: u32| {
        let mut at = pid;
        while let Some(&(parent, _)) = all.get(&at) {
            if parent == own || at == own || parent == at {
                break;
            }
            at = parent;
        }
        at
    };
    let mut parts: BTreeMap<u32, Part> = BTreeMap::new();
    let mut memory = Memory::default();
    for (pid, (working, commit, _)) in found {
        let root = if pid == own { own } else { started_from(pid) };
        let name = if root == own { "orior".to_string() } else { all.get(&root).map(|(_, name)| name.trim_end_matches(".exe").to_string()).unwrap_or_default() };
        let part = parts.entry(root).or_insert_with(|| Part { name, pid: root, ..Part::default() });
        part.processes += 1;
        part.working += working;
        part.commit += commit;
        memory.working += working;
        memory.commit += commit;
    }
    memory.parts = parts.into_values().collect();
    memory.parts.sort_by_key(|part| std::cmp::Reverse(part.working));
    Some(memory)
}

/// What the app holds on Linux and macOS: its own process and each of the container's, each counted
/// with the process the app started it from. On Linux a process's working set is its private pages
/// in RAM and its commit its anonymous pages in RAM and swapped out; on macOS both are its physical
/// footprint, as the system counts it against a process's limit.
#[cfg(any(target_os = "linux", target_os = "macos"))]
pub fn read() -> Option<Memory> {
    use std::collections::{BTreeMap, HashMap};
    let own = std::process::id();
    let parents: HashMap<u32, u32> = crate::orior::own_as::<Vec<(u32, u32)>>("container_parents", serde_json::json!({})).unwrap_or_default().into_iter().collect();
    let started_from = |pid: u32| {
        let mut at = pid;
        while let Some(&parent) = parents.get(&at) {
            if parent == own || parent == at || parent <= 1 {
                break;
            }
            at = parent;
        }
        at
    };
    let mut parts: BTreeMap<u32, Part> = BTreeMap::new();
    let mut memory = Memory::default();
    for pid in std::iter::once(own).chain(crate::orior::own_as::<Vec<u32>>("container_members", serde_json::json!({})).unwrap_or_default()) {
        let Some((working, commit)) = measure(pid) else { continue };
        let root = if pid == own { own } else { started_from(pid) };
        let name = if root == own { "orior".to_string() } else { name_of(root) };
        let part = parts.entry(root).or_insert_with(|| Part { name, pid: root, ..Part::default() });
        part.processes += 1;
        part.working += working;
        part.commit += commit;
        memory.working += working;
        memory.commit += commit;
    }
    memory.parts = parts.into_values().collect();
    memory.parts.sort_by_key(|part| std::cmp::Reverse(part.working));
    Some(memory)
}

/// A process's private pages in RAM, and its anonymous pages in RAM and in swap, in bytes.
#[cfg(target_os = "linux")]
fn measure(pid: u32) -> Option<(u64, u64)> {
    let field = |text: &str, name: &str| text.lines().find_map(|line| line.strip_prefix(name)?.split_whitespace().next()?.parse::<u64>().ok()).unwrap_or(0) * 1024;
    let rollup = std::fs::read_to_string(format!("/proc/{pid}/smaps_rollup")).ok()?;
    let status = std::fs::read_to_string(format!("/proc/{pid}/status")).ok()?;
    Some((field(&rollup, "Private_Clean:") + field(&rollup, "Private_Dirty:"), field(&status, "RssAnon:") + field(&status, "VmSwap:")))
}

#[cfg(target_os = "linux")]
fn name_of(pid: u32) -> String {
    std::fs::read_to_string(format!("/proc/{pid}/comm")).map(|name| name.trim().to_string()).unwrap_or_default()
}

/// A process's physical footprint, in bytes, as the system counts it, for both.
#[cfg(target_os = "macos")]
fn measure(pid: u32) -> Option<(u64, u64)> {
    unsafe extern "C" {
        fn proc_pid_rusage(pid: i32, flavor: i32, buffer: *mut std::ffi::c_void) -> i32;
    }
    // RUSAGE_INFO_V4's answer, a rusage_info_v4, holds the physical footprint 72 bytes in.
    const RUSAGE_INFO_V4: i32 = 4;
    let mut info = [0u8; 296];
    // SAFETY: the buffer is larger than the rusage_info_v4 the call writes.
    let read = unsafe { proc_pid_rusage(pid as i32, RUSAGE_INFO_V4, info.as_mut_ptr().cast()) };
    if read != 0 {
        return None;
    }
    let footprint = u64::from_ne_bytes(info[72..80].try_into().ok()?);
    Some((footprint, footprint))
}

#[cfg(target_os = "macos")]
fn name_of(pid: u32) -> String {
    unsafe extern "C" {
        fn proc_name(pid: i32, buffer: *mut std::ffi::c_void, size: u32) -> i32;
    }
    let mut name = [0u8; 256];
    // SAFETY: the buffer is as long as the size told, and read only as far as the call wrote.
    let length = unsafe { proc_name(pid as i32, name.as_mut_ptr().cast(), name.len() as u32) };
    String::from_utf8_lossy(&name[..length.max(0) as usize]).to_string()
}

#[cfg(not(any(windows, target_os = "linux", target_os = "macos")))]
pub fn read() -> Option<Memory> {
    None
}
