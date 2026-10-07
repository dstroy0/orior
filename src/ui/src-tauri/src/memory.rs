// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What the app holds in memory: its own process and every process it started, the web view's
//! browser, renderers and GPU process among them. Each counts its private working set, the memory
//! only it uses that is in RAM, and its commit, the private memory it has reserved whether in RAM or
//! paged out. The app's own process alone is a small share of the whole.

use serde::Serialize;

#[derive(Serialize, Default, Clone)]
pub struct Part {
    pub name: String,
    pub processes: u32,
    pub working: u64,
    pub commit: u64,
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

    let mut parts: BTreeMap<String, Part> = BTreeMap::new();
    let mut memory = Memory::default();
    for (pid, (working, commit, _)) in found {
        let name = if pid == own { "orior".to_string() } else { all.get(&pid).map(|(_, name)| name.trim_end_matches(".exe").to_string()).unwrap_or_default() };
        let part = parts.entry(name.clone()).or_insert_with(|| Part { name, ..Part::default() });
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

#[cfg(not(windows))]
pub fn read() -> Option<Memory> {
    None
}
