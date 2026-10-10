// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What a built program loads as it starts, read from the file itself: a Windows program's
//! imports and its delay-loaded imports, and a Linux program's needed libraries, its run paths and
//! the glibc versions it asks for. Each library is looked for as the system's loader looks for it
//! where the program is opened on its own, and the libraries those load in turn, past the system's
//! own. A library found only through the folders orior puts on its jobs' PATH, one found nowhere,
//! one found by a path of the machine that built the program, and Visual Studio's debug runtime are
//! each told, as the things that start a program here and keep it from starting elsewhere.

use std::collections::{HashSet, VecDeque};
use std::fs;
use std::path::{Path, PathBuf};

/// Where a library a program loads is found.
#[derive(Clone, Debug, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Found {
    /// In the program's own folder, or by a run path from it.
    Beside,
    /// In the system's own folders, or one Windows resolves itself.
    System,
    /// In the system's folders, put there by the Visual C++ Redistributable, which a machine
    /// without it lacks.
    Redistributable,
    /// Visual Studio's debug runtime, which only a machine with Visual Studio has.
    DebugRuntime,
    /// Through the PATH the system gives a program opened on its own.
    SystemPath,
    /// Only through the folders orior puts on its jobs' PATH.
    OriorPath,
    /// By a run path that names a folder of the machine that built the program.
    BuildPath,
    /// Nowhere.
    Missing,
}

#[derive(Clone, Debug, serde::Serialize)]
pub struct Library {
    pub name: String,
    pub found: Found,
    pub at: Option<PathBuf>,
    /// The file that loads it: the program, or a library the program loads.
    pub by: String,
}

#[derive(Clone, Debug, Default, serde::Serialize)]
pub struct Report {
    pub libraries: Vec<Library>,
    /// The run paths of a Linux program that name folders of the machine that built it.
    pub build_paths: Vec<String>,
    /// The newest glibc version a Linux program asks for, as GLIBC_2.38.
    pub glibc: Option<String>,
}

/// The Visual C++ runtime's libraries, which its Redistributable installs.
const REDISTRIBUTABLE: [&str; 9] = ["vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll", "concrt140.dll", "vccorlib140.dll", "vcomp140.dll"];

/// The debug forms of the runtime, which only Visual Studio installs.
const DEBUG_RUNTIME: [&str; 7] = ["vcruntime140d.dll", "vcruntime140_1d.dll", "msvcp140d.dll", "msvcp140_1d.dll", "msvcp140_2d.dll", "ucrtbased.dll", "concrt140d.dll"];

fn u16_at(bytes: &[u8], at: usize) -> Option<u16> {
    bytes.get(at..at + 2).map(|two| u16::from_le_bytes([two[0], two[1]]))
}

fn u32_at(bytes: &[u8], at: usize) -> Option<u32> {
    bytes.get(at..at + 4).map(|four| u32::from_le_bytes([four[0], four[1], four[2], four[3]]))
}

fn u64_at(bytes: &[u8], at: usize) -> Option<u64> {
    bytes.get(at..at + 8).and_then(|eight| eight.try_into().ok()).map(u64::from_le_bytes)
}

/// The text ended by a 0 at `at`.
fn text_at(bytes: &[u8], at: usize) -> Option<String> {
    let rest = bytes.get(at..)?;
    let end = rest.iter().position(|byte| *byte == 0)?;
    Some(String::from_utf8_lossy(&rest[..end]).to_string())
}

/// A Windows program's machine and the DLLs it imports, the delay-loaded ones among them.
fn pe_imports(bytes: &[u8]) -> Result<(u16, Vec<String>), String> {
    let pe = u32_at(bytes, 0x3C).ok_or("the file ends inside its headers")? as usize;
    if bytes.get(pe..pe + 4) != Some(b"PE\0\0") {
        return Err("the file is no Windows program".to_string());
    }
    let machine = u16_at(bytes, pe + 4).unwrap_or(0);
    let sections = u16_at(bytes, pe + 6).unwrap_or(0) as usize;
    let optional_size = u16_at(bytes, pe + 20).unwrap_or(0) as usize;
    let optional = pe + 24;
    let directories = match u16_at(bytes, optional) {
        Some(0x20b) => optional + 112,
        Some(0x10b) => optional + 96,
        _ => return Err("the file's optional header is of no kind orior reads".to_string()),
    };
    let table = optional + optional_size;
    let to_offset = |rva: u32| -> Option<usize> {
        (0..sections).find_map(|index| {
            let at = table + index * 40;
            let size = u32_at(bytes, at + 8)?.max(u32_at(bytes, at + 16)?);
            let start = u32_at(bytes, at + 12)?;
            let raw = u32_at(bytes, at + 20)?;
            (rva >= start && rva < start + size).then_some((rva - start + raw) as usize)
        })
    };
    let mut names = Vec::new();
    let mut read = |index: usize, step: usize, name_at: usize| {
        let Some(rva) = u32_at(bytes, directories + index * 8).filter(|rva| *rva != 0) else { return };
        let Some(mut at) = to_offset(rva) else { return };
        while let Some(name_rva) = u32_at(bytes, at + name_at) {
            if bytes.get(at..at + step).is_none_or(|entry| entry.iter().all(|byte| *byte == 0)) || name_rva == 0 {
                break;
            }
            if let Some(name) = to_offset(name_rva).and_then(|offset| text_at(bytes, offset)) {
                if !names.iter().any(|one: &String| one.eq_ignore_ascii_case(&name)) {
                    names.push(name);
                }
            }
            at += step;
        }
    };
    read(1, 20, 12);
    read(13, 32, 4);
    Ok((machine, names))
}

/// What a Linux program's dynamic section gives: its needed libraries, its run paths and its
/// RPATH, and the version names it asks for of each library.
#[derive(Default, Debug, PartialEq)]
struct Dynamic {
    needed: Vec<String>,
    rpath: Vec<String>,
    runpath: Vec<String>,
    versions: Vec<(String, String)>,
}

fn elf_dynamic(bytes: &[u8]) -> Result<Dynamic, String> {
    if bytes.get(4) != Some(&2) || bytes.get(5) != Some(&1) {
        return Err("orior reads 64-bit little-endian Linux programs".to_string());
    }
    let phoff = u64_at(bytes, 0x20).ok_or("the file ends inside its headers")? as usize;
    let count = u16_at(bytes, 0x38).unwrap_or(0) as usize;
    let header = |index: usize| phoff + index * 56;
    let loads: Vec<(u64, u64, u64)> = (0..count)
        .filter(|index| u32_at(bytes, header(*index)) == Some(1))
        .filter_map(|index| Some((u64_at(bytes, header(index) + 8)?, u64_at(bytes, header(index) + 16)?, u64_at(bytes, header(index) + 32)?)))
        .collect();
    let to_offset = |vaddr: u64| loads.iter().find(|(_, start, size)| vaddr >= *start && vaddr < start + size).map(|(offset, start, _)| (vaddr - start + offset) as usize);
    let Some(dynamic) = (0..count).find(|index| u32_at(bytes, header(*index)) == Some(2)) else { return Ok(Dynamic::default()) };
    let (start, size) = (u64_at(bytes, header(dynamic) + 8).unwrap_or(0) as usize, u64_at(bytes, header(dynamic) + 32).unwrap_or(0) as usize);
    let entries: Vec<(u64, u64)> = (0..size / 16).filter_map(|index| Some((u64_at(bytes, start + index * 16)?, u64_at(bytes, start + index * 16 + 8)?))).take_while(|(tag, _)| *tag != 0).collect();
    let strings = entries.iter().find(|(tag, _)| *tag == 5).and_then(|(_, value)| to_offset(*value)).ok_or("the program names no string table")?;
    let text = |offset: u64| text_at(bytes, strings + offset as usize).unwrap_or_default();
    let mut found = Dynamic::default();
    for (tag, value) in &entries {
        match tag {
            1 => found.needed.push(text(*value)),
            15 => found.rpath.extend(text(*value).split(':').filter(|one| !one.is_empty()).map(str::to_string)),
            29 => found.runpath.extend(text(*value).split(':').filter(|one| !one.is_empty()).map(str::to_string)),
            _ => {}
        }
    }
    let verneed = entries.iter().find(|(tag, _)| *tag == 0x6fff_fffe).and_then(|(_, value)| to_offset(*value));
    let verneed_count = entries.iter().find(|(tag, _)| *tag == 0x6fff_ffff).map_or(0, |(_, value)| *value);
    if let Some(mut at) = verneed {
        for _ in 0..verneed_count {
            let file = text(u32_at(bytes, at + 4).unwrap_or(0) as u64);
            let mut aux = at + u32_at(bytes, at + 8).unwrap_or(0) as usize;
            for _ in 0..u16_at(bytes, at + 2).unwrap_or(0) {
                found.versions.push((file.clone(), text(u32_at(bytes, aux + 8).unwrap_or(0) as u64)));
                let next = u32_at(bytes, aux + 12).unwrap_or(0) as usize;
                if next == 0 {
                    break;
                }
                aux += next;
            }
            let next = u32_at(bytes, at + 12).unwrap_or(0) as usize;
            if next == 0 {
                break;
            }
            at += next;
        }
    }
    Ok(found)
}

/// The folders a file of the loader's settings names, those of the files it includes among them,
/// four includes deep at most.
fn ld_conf(path: &Path, folders: &mut Vec<PathBuf>, depth: u8) {
    let Ok(text) = fs::read_to_string(path) else { return };
    for line in text.lines().map(|line| line.split('#').next().unwrap_or_default().trim()).filter(|line| !line.is_empty()) {
        let Some(pattern) = line.strip_prefix("include").map(str::trim) else {
            folders.push(PathBuf::from(line));
            continue;
        };
        let pattern = Path::new(pattern);
        let (Some(folder), Some(name)) = (pattern.parent(), pattern.file_name().map(|name| name.to_string_lossy().to_string())) else { continue };
        let (head, tail) = name.split_once('*').unwrap_or((&name, ""));
        let mut files: Vec<PathBuf> = fs::read_dir(folder)
            .into_iter()
            .flatten()
            .flatten()
            .map(|entry| entry.path())
            .filter(|file| file.file_name().is_some_and(|one| one.to_string_lossy().starts_with(head) && one.to_string_lossy().ends_with(tail)))
            .collect();
        files.sort();
        if depth < 4 {
            files.iter().for_each(|file| ld_conf(file, folders, depth + 1));
        }
    }
}

/// The folders the Linux loader looks in after a program's own run paths: those /etc/ld.so.conf
/// names and the files it includes, then the system's own library folders.
fn linux_folders() -> Vec<PathBuf> {
    let mut folders = Vec::new();
    ld_conf(Path::new("/etc/ld.so.conf"), &mut folders, 0);
    folders.extend(["/lib64", "/usr/lib64", "/lib", "/usr/lib", "/lib/x86_64-linux-gnu", "/usr/lib/x86_64-linux-gnu", "/lib/aarch64-linux-gnu", "/usr/lib/aarch64-linux-gnu"].map(PathBuf::from));
    folders
}

/// The folders Windows itself loads a program's DLLs from: System32, or SysWOW64 for a 32-bit
/// program on a 64-bit system, the old 16-bit System folder, and the Windows folder.
fn windows_folders(machine: u16) -> Vec<PathBuf> {
    let windows = std::env::var_os("SystemRoot").map(PathBuf::from).unwrap_or_else(|| PathBuf::from(r"C:\Windows"));
    let system = if machine == 0x14c && windows.join("SysWOW64").is_dir() { "SysWOW64" } else { "System32" };
    vec![windows.join(system), windows.join("System"), windows]
}

/// Reads what the program at `path` loads, each library looked for as the system's loader looks
/// for it where the program is opened on its own, and then, where it is not found so, through
/// `orior_path`, the folders orior's jobs have on their PATH.
pub fn read(path: &Path, orior_path: &[PathBuf]) -> Result<Report, String> {
    let bytes = fs::read(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let folder = path.parent().unwrap_or(Path::new(".")).to_path_buf();
    let program = path.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
    let mut report = Report::default();
    if bytes.starts_with(b"MZ") {
        let system_path = crate::toolchains::system_path();
        let (machine, imports) = pe_imports(&bytes)?;
        let system = windows_folders(machine);
        let mut seen: HashSet<String> = HashSet::new();
        let mut queue: VecDeque<(String, String)> = imports.into_iter().map(|name| (name, program.clone())).collect();
        while let Some((name, by)) = queue.pop_front() {
            let lower = name.to_lowercase();
            if !seen.insert(lower.clone()) || lower.starts_with("api-ms-") || lower.starts_with("ext-ms-") {
                continue;
            }
            let in_folder = |dir: &Path| Some(dir.join(&name)).filter(|file| file.is_file());
            let (found, at) = if let Some(at) = in_folder(&folder) {
                (Found::Beside, Some(at))
            } else if let Some(at) = system.iter().find_map(|dir| in_folder(dir)) {
                let found = if DEBUG_RUNTIME.contains(&lower.as_str()) {
                    Found::DebugRuntime
                } else if REDISTRIBUTABLE.contains(&lower.as_str()) {
                    Found::Redistributable
                } else {
                    Found::System
                };
                (found, Some(at))
            } else if let Some(at) = system_path.iter().find_map(|dir| in_folder(dir)) {
                (Found::SystemPath, Some(at))
            } else if let Some(at) = orior_path.iter().find_map(|dir| in_folder(dir)) {
                (Found::OriorPath, Some(at))
            } else if DEBUG_RUNTIME.contains(&lower.as_str()) {
                (Found::DebugRuntime, None)
            } else {
                (Found::Missing, None)
            };
            if let Some(at) = at.as_ref().filter(|_| matches!(found, Found::Beside | Found::SystemPath | Found::OriorPath)) {
                if let Ok(more) = fs::read(at).ok().as_deref().map(pe_imports).transpose() {
                    queue.extend(more.into_iter().flat_map(|(_, names)| names).map(|more| (more, name.clone())));
                }
            }
            report.libraries.push(Library { name, found, at, by });
        }
    } else if bytes.starts_with(b"\x7fELF") {
        let system = linux_folders();
        let dynamic = elf_dynamic(&bytes)?;
        report.glibc = newest_glibc(&dynamic.versions);
        let mut seen: HashSet<String> = HashSet::new();
        let mut queue: VecDeque<(String, String, PathBuf, Dynamic)> = dynamic.needed.iter().map(|name| (name.clone(), program.clone(), folder.clone(), clone_paths(&dynamic))).collect();
        for entry in dynamic.rpath.iter().chain(&dynamic.runpath).filter(|entry| !entry.starts_with("$ORIGIN") && !entry.starts_with("${ORIGIN}")) {
            report.build_paths.push(entry.clone());
        }
        while let Some((name, by, origin, paths)) = queue.pop_front() {
            if !seen.insert(name.clone()) {
                continue;
            }
            let expand = |entry: &String| PathBuf::from(entry.replace("${ORIGIN}", &origin.to_string_lossy()).replace("$ORIGIN", &origin.to_string_lossy()));
            let own: Vec<(PathBuf, bool)> = if paths.runpath.is_empty() { &paths.rpath } else { &paths.runpath }.iter().map(|entry| (expand(entry), entry.contains("ORIGIN"))).collect();
            let in_folder = |dir: &Path| Some(dir.join(&name)).filter(|file| file.is_file());
            let (found, at) = if let Some((at, beside)) = own.iter().find_map(|(dir, beside)| in_folder(dir).map(|at| (at, *beside))) {
                (if beside { Found::Beside } else { Found::BuildPath }, Some(at))
            } else if let Some(at) = system.iter().find_map(|dir| in_folder(dir)) {
                (Found::System, Some(at))
            } else {
                (Found::Missing, None)
            };
            if let Some(at) = at.as_ref().filter(|_| matches!(found, Found::Beside | Found::BuildPath)) {
                if let Ok(more) = fs::read(at).map_err(|error| error.to_string()).and_then(|bytes| elf_dynamic(&bytes)) {
                    if let Some(newer) = newest_glibc(&more.versions) {
                        if report.glibc.as_ref().is_none_or(|old| glibc_key(&newer) > glibc_key(old)) {
                            report.glibc = Some(newer);
                        }
                    }
                    let origin = at.parent().unwrap_or(Path::new(".")).to_path_buf();
                    queue.extend(more.needed.iter().map(|one| (one.clone(), name.clone(), origin.clone(), clone_paths(&more))));
                }
            }
            report.libraries.push(Library { name, found, at, by });
        }
    } else if bytes.starts_with(&[0xcf, 0xfa, 0xed, 0xfe]) || bytes.starts_with(&[0xca, 0xfe, 0xba, 0xbe]) {
        return Err("orior reads the libraries of Windows and Linux programs; a macOS program's are read with otool -L".to_string());
    } else {
        return Err(format!("{} is no program orior reads", path.display()));
    }
    Ok(report)
}

fn clone_paths(dynamic: &Dynamic) -> Dynamic {
    Dynamic { needed: Vec::new(), rpath: dynamic.rpath.clone(), runpath: dynamic.runpath.clone(), versions: Vec::new() }
}

/// A glibc version's numbers, to set versions in order.
fn glibc_key(version: &str) -> Vec<u32> {
    version.trim_start_matches("GLIBC_").split('.').filter_map(|part| part.parse().ok()).collect()
}

/// The newest of the glibc versions among `versions`.
fn newest_glibc(versions: &[(String, String)]) -> Option<String> {
    versions.iter().map(|(_, version)| version).filter(|version| version.starts_with("GLIBC_2")).max_by_key(|version| glibc_key(version)).cloned()
}

/// What a report tells of the program: each library found other than beside it or in the system's
/// own folders, each run path of the machine that built it, and the newest glibc it asks for.
pub fn said(report: &Report) -> Vec<String> {
    let mut lines = Vec::new();
    for one in &report.libraries {
        let at = one.at.as_ref().map(|at| at.display().to_string()).unwrap_or_default();
        let by = &one.by;
        lines.push(match one.found {
            Found::OriorPath => format!("{}, which {by} loads, is found through the folders orior puts on its PATH, at {at}, and not where the program is opened on its own: Copy Libraries Beside It puts it beside the program", one.name),
            Found::SystemPath => format!("{}, which {by} loads, is found through this machine's PATH, at {at}, which another machine's PATH may not hold: Copy Libraries Beside It puts it beside the program", one.name),
            Found::Missing => format!("{}, which {by} loads, is found nowhere: the program does not start without it", one.name),
            Found::DebugRuntime => format!("{}, which {by} loads, is Visual Studio's debug runtime, which only a machine with Visual Studio has: a release build loads the runtime any machine can install", one.name),
            Found::Redistributable => format!("{}, which {by} loads, comes with the Visual C++ Redistributable, which a machine without it lacks", one.name),
            Found::BuildPath => format!("{}, which {by} loads, is found by a run path of the machine that built it, at {at}", one.name),
            Found::Beside | Found::System => continue,
        });
    }
    for path in &report.build_paths {
        lines.push(format!("the program looks for its libraries in {path}, a folder of the machine that built it; a run path from $ORIGIN finds them beside the program on any machine"));
    }
    if let Some(glibc) = &report.glibc {
        lines.push(format!("the program asks for glibc {} or newer, and does not start on a system with an older one", glibc.trim_start_matches("GLIBC_")));
    }
    lines
}

/// Copies each library the program loads through a PATH, this machine's or orior's, beside it, and
/// gives the names copied.
pub fn copy_beside(path: &Path, report: &Report) -> Result<Vec<String>, String> {
    let folder = path.parent().ok_or("the program has no folder")?;
    let mut copied = Vec::new();
    for one in report.libraries.iter().filter(|one| matches!(one.found, Found::OriorPath | Found::SystemPath)) {
        let Some(from) = &one.at else { continue };
        fs::copy(from, folder.join(&one.name)).map_err(|error| format!("{}: {error}", from.display()))?;
        copied.push(one.name.clone());
    }
    Ok(copied)
}

#[cfg(test)]
mod reading {
    use super::*;

    /// A Windows program of one section that imports `names`, the last of them delay-loaded.
    fn pe_with(names: &[&str]) -> Vec<u8> {
        let mut bytes = vec![0u8; 0x400];
        bytes[0..2].copy_from_slice(b"MZ");
        bytes[0x3C..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        bytes[0x80..0x84].copy_from_slice(b"PE\0\0");
        bytes[0x84..0x86].copy_from_slice(&0x8664u16.to_le_bytes());
        bytes[0x86..0x88].copy_from_slice(&1u16.to_le_bytes());
        bytes[0x94..0x96].copy_from_slice(&240u16.to_le_bytes());
        bytes[0x98..0x9A].copy_from_slice(&0x20bu16.to_le_bytes());
        let section = 0x98 + 240;
        bytes[section + 8..section + 12].copy_from_slice(&0x1000u32.to_le_bytes());
        bytes[section + 12..section + 16].copy_from_slice(&0x1000u32.to_le_bytes());
        bytes[section + 16..section + 20].copy_from_slice(&0x1000u32.to_le_bytes());
        bytes[section + 20..section + 24].copy_from_slice(&0x400u32.to_le_bytes());
        bytes.resize(0x1400, 0);
        let (plain, delayed) = names.split_at(names.len() - 1);
        let directories = 0x98 + 112;
        bytes[directories + 8..directories + 12].copy_from_slice(&0x1000u32.to_le_bytes());
        bytes[directories + 13 * 8..directories + 13 * 8 + 4].copy_from_slice(&0x1200u32.to_le_bytes());
        let mut name_rva = 0x1300u32;
        for (index, name) in plain.iter().enumerate() {
            let at = 0x400 + index * 20;
            bytes[at + 12..at + 16].copy_from_slice(&name_rva.to_le_bytes());
            let offset = (name_rva - 0x1000 + 0x400) as usize;
            bytes[offset..offset + name.len()].copy_from_slice(name.as_bytes());
            name_rva += name.len() as u32 + 1;
        }
        let at = 0x400 + 0x200;
        bytes[at..at + 4].copy_from_slice(&1u32.to_le_bytes());
        bytes[at + 4..at + 8].copy_from_slice(&name_rva.to_le_bytes());
        let offset = (name_rva - 0x1000 + 0x400) as usize;
        bytes[offset..offset + delayed[0].len()].copy_from_slice(delayed[0].as_bytes());
        bytes
    }

    #[test]
    fn a_windows_program_gives_its_imports_and_delayed_imports() {
        let bytes = pe_with(&["KERNEL32.dll", "libstdc++-6.dll", "late.dll"]);
        assert_eq!(pe_imports(&bytes).unwrap(), (0x8664, vec!["KERNEL32.dll".to_string(), "libstdc++-6.dll".to_string(), "late.dll".to_string()]));
    }

    #[test]
    fn a_library_only_orior_finds_is_told_and_copied_beside() {
        let dir = std::env::temp_dir().join(format!("orior_binary_{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("app")).unwrap();
        fs::create_dir_all(dir.join("tools")).unwrap();
        fs::write(dir.join("app").join("main.exe"), pe_with(&["kernel32.dll", "helper.dll", "nowhere.dll"])).unwrap();
        fs::write(dir.join("tools").join("helper.dll"), pe_with(&["kernel32.dll", "deeper.dll"])).unwrap();
        if !cfg!(windows) {
            return;
        }
        let report = read(&dir.join("app").join("main.exe"), &[dir.join("tools")]).unwrap();
        let by_name = |name: &str| report.libraries.iter().find(|one| one.name == name).map(|one| (one.found.clone(), one.by.clone()));
        assert_eq!(by_name("kernel32.dll"), Some((Found::System, "main.exe".to_string())));
        assert_eq!(by_name("helper.dll"), Some((Found::OriorPath, "main.exe".to_string())));
        assert_eq!(by_name("deeper.dll"), Some((Found::Missing, "helper.dll".to_string())));
        assert_eq!(by_name("nowhere.dll").map(|(found, _)| found), Some(Found::Missing));
        let lines = said(&report);
        assert!(lines.iter().any(|line| line.starts_with("helper.dll, which main.exe loads, is found through the folders orior puts on its PATH")), "{lines:?}");
        assert_eq!(copy_beside(&dir.join("app").join("main.exe"), &report).unwrap(), vec!["helper.dll"]);
        assert!(dir.join("app").join("helper.dll").is_file());
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_linux_program_gives_its_needs_run_paths_and_glibc() {
        let mut bytes = vec![0u8; 0x1000];
        bytes[0..6].copy_from_slice(b"\x7fELF\x02\x01");
        bytes[0x20..0x28].copy_from_slice(&0x40u64.to_le_bytes());
        bytes[0x38..0x3A].copy_from_slice(&2u16.to_le_bytes());
        let put = |bytes: &mut Vec<u8>, at: usize, kind: u32, offset: u64, size: u64| {
            bytes[at..at + 4].copy_from_slice(&kind.to_le_bytes());
            bytes[at + 8..at + 16].copy_from_slice(&offset.to_le_bytes());
            bytes[at + 16..at + 24].copy_from_slice(&offset.to_le_bytes());
            bytes[at + 32..at + 40].copy_from_slice(&size.to_le_bytes());
        };
        put(&mut bytes, 0x40, 1, 0, 0x1000);
        put(&mut bytes, 0x78, 2, 0x200, 0x80);
        let strings = b"\0libc.so.6\0libfoo.so\0$ORIGIN/../lib:/home/me/build/lib\0GLIBC_2.17\0GLIBC_2.34\0";
        bytes[0x300..0x300 + strings.len()].copy_from_slice(strings);
        let entries: [(u64, u64); 6] = [(1, 1), (1, 11), (29, 21), (5, 0x300), (0x6fff_fffe, 0x380), (0x6fff_ffff, 1)];
        for (index, (tag, value)) in entries.iter().enumerate() {
            bytes[0x200 + index * 16..0x208 + index * 16].copy_from_slice(&tag.to_le_bytes());
            bytes[0x208 + index * 16..0x210 + index * 16].copy_from_slice(&value.to_le_bytes());
        }
        bytes[0x380..0x382].copy_from_slice(&1u16.to_le_bytes());
        bytes[0x382..0x384].copy_from_slice(&2u16.to_le_bytes());
        bytes[0x384..0x388].copy_from_slice(&1u32.to_le_bytes());
        bytes[0x388..0x38C].copy_from_slice(&16u32.to_le_bytes());
        let first = 0x390;
        bytes[first + 8..first + 12].copy_from_slice(&55u32.to_le_bytes());
        bytes[first + 12..first + 16].copy_from_slice(&16u32.to_le_bytes());
        bytes[first + 16 + 8..first + 16 + 12].copy_from_slice(&66u32.to_le_bytes());
        let dynamic = elf_dynamic(&bytes).unwrap();
        assert_eq!(dynamic.needed, vec!["libc.so.6", "libfoo.so"]);
        assert_eq!(dynamic.runpath, vec!["$ORIGIN/../lib", "/home/me/build/lib"]);
        assert_eq!(newest_glibc(&dynamic.versions).as_deref(), Some("GLIBC_2.34"));
    }
}
