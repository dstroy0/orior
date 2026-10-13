// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's container: the one process every process orior starts stands under, the language
//! servers, kernels, debuggers, shells, database links and runs, and every process those start.
//!
//! On Windows the process puts itself in a job object as it starts. Every process it starts joins
//! the job on its own, and so does every process those start; the job lists them, and ends them all
//! with the container, however the container ends. On Linux and macOS the container reads its
//! descendants from the system's list of processes, and ends them as it ends.
//!
//! What orior opens for the reader to keep, a page in the browser, a folder in the file manager or
//! another window of orior's, is started outside the container and outlives it. On Windows it breaks
//! away from the job. On Linux and macOS it is its own process group, the container leaves it and
//! every process under it out of its members, and a thread gathers it as it ends.

/// Puts this process at the head of its container. Says whether the container holds: on Windows,
/// whether the job was made and this process put in it; elsewhere, always.
pub fn hold() -> bool {
    #[cfg(windows)]
    {
        windows::hold()
    }
    #[cfg(not(windows))]
    {
        true
    }
}

/// The processes started outside the container that have not yet ended.
#[cfg(unix)]
static OUTSIDE: std::sync::Mutex<Vec<u32>> = std::sync::Mutex::new(Vec::new());

/// Starts `command` outside the container, and gives its process's id: what it starts is the
/// reader's to keep, and does not end with orior.
pub fn start_outside(command: &mut std::process::Command) -> std::io::Result<u32> {
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(windows_sys::Win32::System::Threading::CREATE_BREAKAWAY_FROM_JOB);
        command.spawn().map(|child| child.id())
    }
    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        command.process_group(0);
        let mut child = command.spawn()?;
        let id = child.id();
        if let Ok(mut outside) = OUTSIDE.lock() {
            outside.push(id);
        }
        std::thread::spawn(move || {
            let _ = child.wait();
            if let Ok(mut outside) = OUTSIDE.lock() {
                outside.retain(|one| *one != id);
            }
        });
        Ok(id)
    }
}

/// Every process in the container but the container itself, by its id.
pub fn members() -> Vec<u32> {
    #[cfg(windows)]
    {
        windows::members()
    }
    #[cfg(not(windows))]
    {
        let all = unix::parents();
        let outside = OUTSIDE.lock().map(|outside| outside.clone()).unwrap_or_default();
        descendants(std::process::id(), &all).into_iter().filter(|member| !outside.iter().any(|one| under(*one, *member, &all))).collect()
    }
}

/// Every process the system lists, by its id and its parent's.
pub fn parents() -> Vec<(u32, u32)> {
    #[cfg(windows)]
    {
        windows::parents()
    }
    #[cfg(not(windows))]
    {
        unix::parents()
    }
}

/// Whether the process `id` is `root` or stands under it in the tree `parents` gives. A parent's id
/// the system has given again to a later process can close a loop, and the walk ends after as many
/// steps as the tree has processes.
pub fn under(root: u32, id: u32, parents: &[(u32, u32)]) -> bool {
    let mut at = id;
    for _ in 0..=parents.len() {
        if at == root {
            return true;
        }
        match parents.iter().find(|(child, _)| *child == at) {
            Some(&(_, parent)) if parent != at => at = parent,
            _ => return false,
        }
    }
    false
}

/// The processes under `root` in the tree `parents` gives, by child and parent, each once, children
/// after their parents.
pub fn descendants(root: u32, parents: &[(u32, u32)]) -> Vec<u32> {
    let mut found = Vec::new();
    let mut frontier = vec![root];
    while let Some(parent) = frontier.pop() {
        for &(child, of) in parents {
            if of == parent && child != root && !found.contains(&child) {
                found.push(child);
                frontier.push(child);
            }
        }
    }
    found
}

/// Ends every process in the container but the container itself: on Windows the job does it as the
/// container ends, and this does it now; elsewhere this is how they end.
pub fn end_all() {
    #[cfg(windows)]
    {
        windows::end_members();
    }
    #[cfg(not(windows))]
    {
        let members = members();
        unix::signal(&members, 15);
        std::thread::sleep(std::time::Duration::from_millis(300));
        unix::signal(&members, 9);
    }
}

/// How often a supervised process may die and be started again: at most `most` deaths in any span of
/// `within`. One past that is stopped and said, as a supervisor of Erlang's stops a child that
/// fails faster than starting it again can help.
pub struct Restarts {
    most: usize,
    within: std::time::Duration,
    deaths: std::collections::VecDeque<std::time::Instant>,
}

impl Restarts {
    pub fn new(most: usize, within: std::time::Duration) -> Restarts {
        Restarts { most, within, deaths: std::collections::VecDeque::new() }
    }

    /// Counts a death at `at`, and says whether the process may be started again.
    pub fn died(&mut self, at: std::time::Instant) -> bool {
        while self.deaths.front().is_some_and(|first| at.duration_since(*first) > self.within) {
            self.deaths.pop_front();
        }
        self.deaths.push_back(at);
        self.deaths.len() <= self.most
    }

    /// The deaths counted within the span ending at `at`.
    pub fn count(&self, at: std::time::Instant) -> usize {
        self.deaths.iter().filter(|death| at.duration_since(**death) <= self.within).count()
    }
}

#[cfg(windows)]
mod windows {
    use std::sync::OnceLock;
    use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
    use windows_sys::Win32::System::JobObjects::{
        AssignProcessToJobObject, CreateJobObjectW, JobObjectBasicProcessIdList, JobObjectExtendedLimitInformation, QueryInformationJobObject, SetInformationJobObject, JOBOBJECT_BASIC_PROCESS_ID_LIST, JOBOBJECT_EXTENDED_LIMIT_INFORMATION, JOB_OBJECT_LIMIT_BREAKAWAY_OK, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
    };
    use windows_sys::Win32::System::Threading::{GetCurrentProcess, OpenProcess, TerminateProcess, PROCESS_TERMINATE};

    /// The job, kept as a number: a handle is a pointer, which a static cannot hold across threads.
    static JOB: OnceLock<usize> = OnceLock::new();

    pub fn hold() -> bool {
        *JOB.get_or_init(|| {
            // SAFETY: the job is made, set and joined through the calls Windows documents for it, the
            // information a plain structure sized as the call asks; a job that fails is closed.
            unsafe {
                let job = CreateJobObjectW(std::ptr::null(), std::ptr::null());
                if job.is_null() {
                    return 0;
                }
                let mut limits: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std::mem::zeroed();
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
                let set = SetInformationJobObject(job, JobObjectExtendedLimitInformation, (&raw const limits).cast(), std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32) != 0;
                if !set || AssignProcessToJobObject(job, GetCurrentProcess()) == 0 {
                    CloseHandle(job);
                    return 0;
                }
                // The handle stays open as long as the process does; it closing as the process ends
                // is what ends every member.
                job as usize
            }
        }) != 0
    }

    fn job() -> Option<HANDLE> {
        JOB.get().copied().filter(|job| *job != 0).map(|job| job as HANDLE)
    }

    pub fn members() -> Vec<u32> {
        let Some(job) = job() else { return Vec::new() };
        let own = std::process::id();
        // Room for 4096 ids after the list's two counts.
        let mut buffer = vec![0usize; 4096 + 2];
        // SAFETY: the buffer is as long as the length told, and the list the call writes into it is
        // read only as far as the count it gives.
        unsafe {
            let list = buffer.as_mut_ptr().cast::<JOBOBJECT_BASIC_PROCESS_ID_LIST>();
            let length = (buffer.len() * std::mem::size_of::<usize>()) as u32;
            if QueryInformationJobObject(job, JobObjectBasicProcessIdList, list.cast(), length, std::ptr::null_mut()) == 0 {
                return Vec::new();
            }
            let count = (*list).NumberOfProcessIdsInList as usize;
            let ids = std::ptr::addr_of!((*list).ProcessIdList).cast::<usize>();
            (0..count).map(|at| *ids.add(at) as u32).filter(|id| *id != own).collect()
        }
    }

    pub fn parents() -> Vec<(u32, u32)> {
        use windows_sys::Win32::Foundation::INVALID_HANDLE_VALUE;
        use windows_sys::Win32::System::Diagnostics::ToolHelp::{CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS};
        let mut found = Vec::new();
        // SAFETY: the snapshot is checked before use and closed once, and each entry is given its own
        // size, as the system asks.
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if snapshot == INVALID_HANDLE_VALUE {
                return found;
            }
            let mut entry: PROCESSENTRY32W = std::mem::zeroed();
            entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;
            let mut more = Process32FirstW(snapshot, &mut entry) != 0;
            while more {
                found.push((entry.th32ProcessID, entry.th32ParentProcessID));
                more = Process32NextW(snapshot, &mut entry) != 0;
            }
            CloseHandle(snapshot);
        }
        found
    }

    pub fn end_members() {
        for id in members() {
            // SAFETY: each handle is checked and closed after the process is told to end.
            unsafe {
                let process = OpenProcess(PROCESS_TERMINATE, 0, id);
                if !process.is_null() {
                    TerminateProcess(process, 1);
                    CloseHandle(process);
                }
            }
        }
    }
}

#[cfg(unix)]
mod unix {
    unsafe extern "C" {
        fn kill(pid: i32, signal: i32) -> i32;
    }

    pub fn signal(ids: &[u32], signal: i32) {
        for id in ids {
            // SAFETY: kill takes any process id and signal number, and says by its answer whether the
            // process was there to take it.
            unsafe {
                kill(*id as i32, signal);
            }
        }
    }

    /// Every process the system lists, by its id and its parent's.
    #[cfg(target_os = "linux")]
    pub fn parents() -> Vec<(u32, u32)> {
        let Ok(entries) = std::fs::read_dir("/proc") else { return Vec::new() };
        entries
            .flatten()
            .filter_map(|entry| {
                let id: u32 = entry.file_name().to_str()?.parse().ok()?;
                let stat = std::fs::read_to_string(entry.path().join("stat")).ok()?;
                // The parent is the second field after the program's name, which stands in parentheses
                // and may hold spaces of its own.
                let after = &stat[stat.rfind(')')? + 1..];
                let parent: u32 = after.split_whitespace().nth(1)?.parse().ok()?;
                Some((id, parent))
            })
            .collect()
    }

    #[cfg(target_os = "macos")]
    pub fn parents() -> Vec<(u32, u32)> {
        use std::ffi::c_void;
        unsafe extern "C" {
            fn proc_listallpids(buffer: *mut c_void, size: i32) -> i32;
            fn proc_pidinfo(pid: i32, flavor: i32, arg: u64, buffer: *mut c_void, size: i32) -> i32;
        }
        // PROC_PIDTBSDINFO's answer, a proc_bsdinfo, holds the parent's id 16 bytes in.
        const BSD_INFO: i32 = 3;
        const BSD_INFO_SIZE: usize = 136;
        let mut ids = vec![0i32; 8192];
        // SAFETY: each buffer is as long as the size told, and read no further than the call wrote.
        unsafe {
            let count = proc_listallpids(ids.as_mut_ptr().cast(), (ids.len() * 4) as i32);
            if count <= 0 {
                return Vec::new();
            }
            ids.truncate(count as usize);
            ids.into_iter()
                .filter_map(|id| {
                    let mut info = [0u8; BSD_INFO_SIZE];
                    let read = proc_pidinfo(id, BSD_INFO, 0, info.as_mut_ptr().cast(), BSD_INFO_SIZE as i32);
                    (read as usize == BSD_INFO_SIZE).then(|| (id as u32, u32::from_ne_bytes([info[16], info[17], info[18], info[19]])))
                })
                .collect()
        }
    }

    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    pub fn parents() -> Vec<(u32, u32)> {
        Vec::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{Duration, Instant};

    #[test]
    fn descendants_are_every_process_under_a_root_and_no_other() {
        let tree = [(2, 1), (3, 2), (4, 3), (5, 1), (6, 9), (7, 6)];
        let mut found = descendants(1, &tree);
        found.sort();
        assert_eq!(found, vec![2, 3, 4, 5]);
        assert!(descendants(4, &tree).is_empty());
        assert_eq!(descendants(9, &tree), vec![6, 7]);
    }

    #[test]
    fn a_process_stands_under_each_process_above_it_and_no_other() {
        let tree = [(2, 1), (3, 2), (4, 3), (5, 1), (6, 9), (7, 6), (8, 8)];
        assert!(under(1, 4, &tree));
        assert!(under(4, 4, &tree));
        assert!(!under(5, 4, &tree));
        assert!(!under(1, 7, &tree));
        assert!(!under(1, 8, &tree), "a process listed as its own parent ends the walk");
        assert!(!under(1, 2, &[(2, 3), (3, 2)]), "a loop of ids given again ends the walk");
        let listed = parents();
        let own = std::process::id();
        let parent = listed.iter().find(|(child, _)| *child == own).map(|(_, parent)| *parent).expect("the system lists this process");
        assert!(under(parent, own, &listed));
    }

    #[test]
    fn a_process_dying_too_often_is_not_started_again() {
        let start = Instant::now();
        let mut restarts = Restarts::new(3, Duration::from_secs(60));
        assert!(restarts.died(start));
        assert!(restarts.died(start + Duration::from_secs(1)));
        assert!(restarts.died(start + Duration::from_secs(2)));
        assert!(!restarts.died(start + Duration::from_secs(3)), "a fourth death within a minute stops it");
        assert_eq!(restarts.count(start + Duration::from_secs(3)), 4);
        assert!(restarts.died(start + Duration::from_secs(200)), "deaths long past are forgotten");
        assert_eq!(restarts.count(start + Duration::from_secs(200)), 1);
    }

    /// A program that waits a while, as a server does, holding none of the test's input or output.
    fn waiting_program() -> std::process::Command {
        let mut command = if cfg!(windows) {
            let mut command = std::process::Command::new("ping");
            command.args(["-n", "30", "127.0.0.1"]);
            command
        } else {
            let mut command = std::process::Command::new("sleep");
            command.arg("30");
            command
        };
        command.stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());
        command
    }

    fn alive(id: u32) -> bool {
        #[cfg(windows)]
        {
            use windows_sys::Win32::Foundation::{CloseHandle, STILL_ACTIVE};
            use windows_sys::Win32::System::Threading::{GetExitCodeProcess, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION};
            // SAFETY: the handle is checked and closed; the exit code is a plain number.
            unsafe {
                let process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, id);
                if process.is_null() {
                    return false;
                }
                let mut code = 0u32;
                let read = GetExitCodeProcess(process, &mut code) != 0;
                CloseHandle(process);
                read && code == STILL_ACTIVE as u32
            }
        }
        #[cfg(target_os = "linux")]
        {
            // A process that has ended and is not yet gathered by its parent, a zombie, runs no more.
            std::fs::read_to_string(format!("/proc/{id}/stat")).is_ok_and(|stat| stat.rsplit_once(") ").is_some_and(|(_, rest)| !rest.starts_with('Z')))
        }
        #[cfg(all(unix, not(target_os = "linux")))]
        {
            std::process::Command::new("kill").args(["-0", &id.to_string()]).status().is_ok_and(|status| status.success())
        }
    }

    /// The test's own program, run again as `orior serve` at the head of a container: it starts a
    /// waiting program inside, serves until its input ends, at once, and says the program's id.
    #[test]
    fn orior_serve_ends_what_it_started_as_its_input_ends() {
        if std::env::var_os("ORIOR_SERVE_HEAD").is_some() {
            assert!(hold(), "the container holds");
            let inside = waiting_program().spawn().unwrap();
            std::thread::sleep(Duration::from_millis(300));
            let _ = crate::cli::run(vec!["serve".to_string()]);
            println!("ids {}", inside.id());
            std::mem::forget(inside);
            return;
        }
        let me = std::env::current_exe().unwrap();
        let out = std::process::Command::new(me)
            .args(["--exact", "container::tests::orior_serve_ends_what_it_started_as_its_input_ends", "--nocapture", "--test-threads", "1"])
            .env("ORIOR_SERVE_HEAD", "1")
            .stdin(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .output()
            .unwrap();
        let said = String::from_utf8_lossy(&out.stdout).to_string();
        let id: u32 = said.split("ids ").nth(1).and_then(|rest| rest.split_whitespace().next()).and_then(|id| id.parse().ok()).unwrap_or_else(|| panic!("the head failed: {said}"));
        let until = Instant::now() + Duration::from_secs(5);
        while alive(id) && Instant::now() < until {
            std::thread::sleep(Duration::from_millis(100));
        }
        assert!(!alive(id), "what serve started ended as its input did");
    }

    /// The test's own program, run again as the head of a container: it starts a waiting program
    /// inside and one outside, says their ids, and ends without ending either.
    #[test]
    fn a_container_s_members_end_with_it_and_what_it_starts_outside_does_not() {
        if std::env::var_os("ORIOR_CONTAINER_HEAD").is_some() {
            assert!(hold(), "the container holds");
            let inside = waiting_program().spawn().unwrap();
            let outside_child = start_outside(&mut waiting_program()).unwrap();
            std::thread::sleep(Duration::from_millis(300));
            assert!(members().contains(&inside.id()), "the container lists what it started");
            assert!(!members().contains(&outside_child), "the container leaves out what it started outside");
            println!("ids {} {outside_child}", inside.id());
            std::mem::forget(inside);
            #[cfg(not(windows))]
            end_all();
            return;
        }
        // The head's lines are read as they come and its end waited for, not its output's: what it
        // starts holds a copy of that output open past the head's end.
        let me = std::env::current_exe().unwrap();
        let mut head = std::process::Command::new(me)
            .args(["--exact", "container::tests::a_container_s_members_end_with_it_and_what_it_starts_outside_does_not", "--nocapture", "--test-threads", "1"])
            .env("ORIOR_CONTAINER_HEAD", "1")
            .stdin(std::process::Stdio::null())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::null())
            .spawn()
            .unwrap();
        let mut said = Vec::new();
        let mut ids: Vec<u32> = Vec::new();
        let mut lines = std::io::BufRead::lines(std::io::BufReader::new(head.stdout.take().unwrap()));
        for line in lines.by_ref().map_while(Result::ok) {
            // The test runner writes the test's name and its own words on the line before the head's.
            if let Some(at) = line.find("ids ") {
                ids = line[at + 4..].split_whitespace().take(2).map(|id| id.parse().unwrap()).collect();
            }
            said.push(line);
            if !ids.is_empty() || said.iter().any(|one| one.contains("panicked")) {
                break;
            }
        }
        // The rest of what the head writes is read on, that its writing never fails.
        std::thread::spawn(move || lines.for_each(drop));
        let status = head.wait().unwrap();
        assert!(status.success() && ids.len() == 2, "the head failed: {}", said.join("\n"));
        let until = Instant::now() + Duration::from_secs(5);
        while alive(ids[0]) && Instant::now() < until {
            std::thread::sleep(Duration::from_millis(100));
        }
        assert!(!alive(ids[0]), "the member ended with the container");
        assert!(alive(ids[1]), "what was started outside lives on");
        #[cfg(windows)]
        let _ = std::process::Command::new("taskkill").args(["/PID", &ids[1].to_string(), "/F"]).output();
        #[cfg(not(windows))]
        let _ = std::process::Command::new("kill").arg(ids[1].to_string()).output();
    }
}
