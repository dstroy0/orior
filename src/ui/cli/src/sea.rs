// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Node's single executable: a script built into a copy of node. From Node 25.5, `node --build-sea`
//! makes it. Before that, orior takes Node's documented steps itself, with no package fetched to
//! take them: `node --experimental-sea-config` makes the script's blob, node is copied, the copy's
//! signature is taken off, the blob is added to it as the resource NODE_SEA_BLOB on Windows or as
//! the note of that name in a segment of its own on Linux, and the fuse is turned from 0 to 1.
//!
//! The executable loads Node's own modules alone: a script that requires another file or
//! a package is told so before the build, since only a bundler such as Bun or Deno takes those in.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

/// The fuse Node reads to know a blob was added, as Node's documentation gives it.
const FUSE: &[u8] = b"NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0";

/// The name the blob is added under.
const RESOURCE: &str = "NODE_SEA_BLOB";

/// The modules Node holds itself, which a single executable can require.
const BUILTIN: [&str; 51] = [
    "assert", "async_hooks", "buffer", "child_process", "cluster", "console", "constants", "crypto", "dgram", "diagnostics_channel", "dns", "domain", "events", "fs",
    "http", "http2", "https", "inspector", "module", "net", "os", "path", "perf_hooks", "process", "punycode", "querystring", "readline", "repl", "stream",
    "string_decoder", "sys", "timers", "tls", "trace_events", "tty", "url", "util", "v8", "vm", "wasi", "worker_threads", "zlib", "fs/promises", "path/posix",
    "path/win32", "stream/promises", "stream/web", "timers/promises", "util/types", "dns/promises", "readline/promises",
];

/// The modules a script asks for that Node does not hold itself: each named in a require(), an
/// import or an import(), other than a `node:` name or a module of BUILTIN.
pub fn outside(text: &str) -> Vec<String> {
    let mut found = Vec::new();
    for mark in ["require(", "import(", "from ", "import "] {
        let mut rest = text;
        while let Some(at) = rest.find(mark) {
            rest = &rest[at + mark.len()..];
            let trimmed = rest.trim_start();
            let Some(quote) = trimmed.chars().next().filter(|char| matches!(char, '"' | '\'' | '`')) else { continue };
            let Some(name) = trimmed[1..].split(quote).next() else { continue };
            let bare = name.strip_prefix("node:").is_some() || BUILTIN.contains(&name);
            if !bare && !name.is_empty() && !found.iter().any(|one: &String| one == name) {
                found.push(name.to_string());
            }
        }
    }
    found
}

/// The version node says it is, as its three numbers.
fn version(node: &Path) -> Result<(u32, u32, u32), String> {
    let mut cmd = Command::new(node);
    cmd.arg("--version");
    crate::runner::quiet(&mut cmd);
    let out = cmd.output().map_err(|error| format!("{}: {error}", node.display()))?;
    let text = String::from_utf8_lossy(&out.stdout);
    let numbers: Vec<u32> = text.trim().trim_start_matches('v').split('.').filter_map(|part| part.parse().ok()).collect();
    match numbers.as_slice() {
        [major, minor, patch, ..] => Ok((*major, *minor, *patch)),
        _ => Err(format!("{} says no version: {}", node.display(), text.trim())),
    }
}

/// Runs node with `args`, each line it writes said as it ends, and says why where it fails.
fn node_run(node: &Path, args: &[&str], say: &dyn Fn(&str)) -> Result<(), String> {
    say(&format!("$ node {}", args.join(" ")));
    let mut cmd = Command::new(node);
    cmd.args(args);
    crate::runner::quiet(&mut cmd);
    let out = cmd.output().map_err(|error| format!("node: {error}"))?;
    for line in String::from_utf8_lossy(&out.stdout).lines().chain(String::from_utf8_lossy(&out.stderr).lines()) {
        say(line);
    }
    if out.status.success() { Ok(()) } else { Err(format!("node {} ended with {}", args.first().unwrap_or(&""), out.status)) }
}

fn json_text(text: &str) -> String {
    serde_json::to_string(text).unwrap_or_default()
}

/// Builds the script at `main` into the single executable `output`, with `node`. `module` says the
/// script is an ES module. Each step is said as it goes.
pub fn build(node: &Path, main: &Path, output: &Path, module: bool, say: &dyn Fn(&str)) -> Result<PathBuf, String> {
    let text = fs::read_to_string(main).map_err(|error| format!("{}: {error}", main.display()))?;
    let wanted = outside(&text);
    if !wanted.is_empty() {
        return Err(format!(
            "{} asks for {}, which a single executable of Node's does not hold: it loads Node's own modules alone; Bun or Deno bundles them in",
            main.display(),
            wanted.join(", ")
        ));
    }
    let output = if cfg!(windows) && output.extension().is_none() { output.with_extension("exe") } else { output.to_path_buf() };
    if let Some(folder) = output.parent() {
        fs::create_dir_all(folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    }
    let (major, minor, _) = version(node)?;
    let format = if module { "module" } else { "commonjs" };
    let config = output.with_extension("sea.json");
    let slash = |path: &Path| path.display().to_string().replace('\\', "/");
    if (major, minor) >= (25, 5) {
        let text = format!(
            "{{\n  \"main\": {},\n  \"output\": {},\n  \"mainFormat\": \"{format}\",\n  \"disableExperimentalSEAWarning\": true\n}}\n",
            json_text(&slash(main)),
            json_text(&slash(&output))
        );
        fs::write(&config, text).map_err(|error| format!("{}: {error}", config.display()))?;
        node_run(node, &["--build-sea", &slash(&config)], say)?;
        return Ok(output);
    }
    if major < 20 {
        return Err(format!("Node {major}.{minor} makes no single executable: Node 20 is the first that does"));
    }
    if cfg!(target_os = "macos") {
        return Err(format!("on macOS, Node {major}.{minor} needs postject to add the blob; Node 25.5 builds it itself, with --build-sea"));
    }
    let blob = output.with_extension("blob");
    let module_line = if module { format!(",\n  \"mainFormat\": \"{format}\"") } else { String::new() };
    let text = format!(
        "{{\n  \"main\": {},\n  \"output\": {}{module_line},\n  \"disableExperimentalSEAWarning\": true\n}}\n",
        json_text(&slash(main)),
        json_text(&slash(&blob))
    );
    fs::write(&config, text).map_err(|error| format!("{}: {error}", config.display()))?;
    node_run(node, &["--experimental-sea-config", &slash(&config)], say)?;
    let data = fs::read(&blob).map_err(|error| format!("{}: {error}", blob.display()))?;
    say(&format!("copy {} to {}", node.display(), output.display()));
    let mut program = fs::read(node).map_err(|error| format!("{}: {error}", node.display()))?;
    if cfg!(windows) {
        let cut = unsigned(&mut program)?;
        if cut {
            say("took off node's signature");
        }
        fs::write(&output, &program).map_err(|error| format!("{}: {error}", output.display()))?;
        add_resource(&output, &data)?;
        say(&format!("added {} bytes as the resource {RESOURCE}", data.len()));
        let mut program = fs::read(&output).map_err(|error| format!("{}: {error}", output.display()))?;
        fuse(&mut program)?;
        fs::write(&output, &program).map_err(|error| format!("{}: {error}", output.display()))?;
    } else {
        let mut built = with_note(&program, &data)?;
        say(&format!("added {} bytes as the note {RESOURCE}", data.len()));
        fuse(&mut built)?;
        fs::write(&output, &built).map_err(|error| format!("{}: {error}", output.display()))?;
        set_runnable(&output)?;
    }
    say("turned the fuse to 1");
    Ok(output)
}

#[cfg(unix)]
fn set_runnable(path: &Path) -> Result<(), String> {
    use std::os::unix::fs::PermissionsExt;
    fs::set_permissions(path, fs::Permissions::from_mode(0o755)).map_err(|error| format!("{}: {error}", path.display()))
}

#[cfg(not(unix))]
fn set_runnable(_: &Path) -> Result<(), String> {
    Ok(())
}

/// Turns the fuse's 0 to 1 in a program's bytes; a program with no fuse, or with it turned, is said.
fn fuse(program: &mut [u8]) -> Result<(), String> {
    let at = program.windows(FUSE.len()).position(|window| window == FUSE).ok_or("node holds no single executable's fuse, or holds it turned already")?;
    program[at + FUSE.len() - 1] = b'1';
    Ok(())
}

fn read_u16(bytes: &[u8], at: usize) -> Result<u16, String> {
    bytes.get(at..at + 2).map(|two| u16::from_le_bytes([two[0], two[1]])).ok_or_else(|| "the program ends inside its own headers".to_string())
}

fn read_u32(bytes: &[u8], at: usize) -> Result<u32, String> {
    bytes.get(at..at + 4).map(|four| u32::from_le_bytes([four[0], four[1], four[2], four[3]])).ok_or_else(|| "the program ends inside its own headers".to_string())
}

fn read_u64(bytes: &[u8], at: usize) -> Result<u64, String> {
    bytes.get(at..at + 8).map(|eight| u64::from_le_bytes(eight.try_into().unwrap_or_default())).ok_or_else(|| "the program ends inside its own headers".to_string())
}

/// Takes a Windows program's signature off: the certificate table's entry is emptied and the
/// certificates, at the file's end, cut away. Says whether there was one.
fn unsigned(program: &mut Vec<u8>) -> Result<bool, String> {
    if program.get(0..2) != Some(b"MZ") {
        return Err("node is no Windows program".to_string());
    }
    let pe = read_u32(program, 0x3C)? as usize;
    if program.get(pe..pe + 4) != Some(b"PE\0\0") {
        return Err("node is no Windows program".to_string());
    }
    let optional = pe + 24;
    let directories = match read_u16(program, optional)? {
        0x20b => optional + 112,
        0x10b => optional + 96,
        other => return Err(format!("node's optional header is of the unknown kind {other:#x}")),
    };
    let entry = directories + 4 * 8;
    let (offset, size) = (read_u32(program, entry)? as usize, read_u32(program, entry + 4)? as usize);
    if offset == 0 || size == 0 {
        return Ok(false);
    }
    program[entry..entry + 8].fill(0);
    if offset + size == program.len() {
        program.truncate(offset);
    }
    Ok(true)
}

/// Adds `data` to the Windows program at `path` as the RCDATA resource RESOURCE.
#[cfg(windows)]
fn add_resource(path: &Path, data: &[u8]) -> Result<(), String> {
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::System::LibraryLoader::{BeginUpdateResourceW, EndUpdateResourceW, UpdateResourceW};
    // The RCDATA type, 10, given in place of a name as Windows takes a number for one.
    const RT_RCDATA: *const u16 = 10 as *const u16;
    let wide: Vec<u16> = path.as_os_str().encode_wide().chain(std::iter::once(0)).collect();
    let name: Vec<u16> = RESOURCE.encode_utf16().chain(std::iter::once(0)).collect();
    let size = u32::try_from(data.len()).map_err(|_| "the blob is past 4 GB".to_string())?;
    // SAFETY: each pointer is to a buffer this function holds, ended by a 0 where Windows reads it
    // as a string, and the handle is ended on every path.
    unsafe {
        let update = BeginUpdateResourceW(wide.as_ptr(), 0);
        if update.is_null() {
            return Err(format!("{}: {}", path.display(), std::io::Error::last_os_error()));
        }
        if UpdateResourceW(update, RT_RCDATA, name.as_ptr(), 0, data.as_ptr().cast(), size) == 0 {
            let error = std::io::Error::last_os_error();
            EndUpdateResourceW(update, 1);
            return Err(format!("{}: {error}", path.display()));
        }
        if EndUpdateResourceW(update, 0) == 0 {
            return Err(format!("{}: {}", path.display(), std::io::Error::last_os_error()));
        }
    }
    Ok(())
}

#[cfg(not(windows))]
fn add_resource(path: &Path, _: &[u8]) -> Result<(), String> {
    Err(format!("{} is a Windows program, which only Windows adds a resource to", path.display()))
}

fn put_u32(bytes: &mut Vec<u8>, value: u32) {
    bytes.extend_from_slice(&value.to_le_bytes());
}

fn put_u64(bytes: &mut Vec<u8>, value: u64) {
    bytes.extend_from_slice(&value.to_le_bytes());
}

/// A Linux program's bytes with `data` added as the note RESOURCE, where node finds it: in a
/// segment of its own that is loaded, past every segment the program loads. The program headers
/// move to the head of that segment, with two more, the segment's own and the note's, and the
/// header that names the program headers is set to their new place. The segment's address keeps
/// the file's offset as far from it as the first loaded segment does: the program headers'
/// address reads the same whichever way the system works it out.
fn with_note(program: &[u8], data: &[u8]) -> Result<Vec<u8>, String> {
    const PT_LOAD: u32 = 1;
    const PT_NOTE: u32 = 4;
    const PT_PHDR: u32 = 6;
    const PAGE: u64 = 0x1000;
    if program.get(0..4) != Some(b"\x7fELF") || program.get(4) != Some(&2) || program.get(5) != Some(&1) {
        return Err("node is no 64-bit little-endian Linux program".to_string());
    }
    let phoff = read_u64(program, 0x20)? as usize;
    let entsize = read_u16(program, 0x36)? as usize;
    let count = read_u16(program, 0x38)? as usize;
    if entsize != 56 {
        return Err(format!("node's program headers are {entsize} bytes each, not 56"));
    }
    let mut headers: Vec<[u8; 56]> = (0..count)
        .map(|index| program.get(phoff + index * 56..phoff + (index + 1) * 56).and_then(|one| one.try_into().ok()).ok_or("node ends inside its program headers"))
        .collect::<Result<_, _>>()?;
    let kind = |header: &[u8; 56]| u32::from_le_bytes(header[0..4].try_into().unwrap_or_default());
    let field = |header: &[u8; 56], at: usize| u64::from_le_bytes(header[at..at + 8].try_into().unwrap_or_default());
    let loads: Vec<&[u8; 56]> = headers.iter().filter(|header| kind(header) == PT_LOAD).collect();
    let first = loads.first().ok_or("node loads no segment")?;
    let bias = field(first, 16).wrapping_sub(field(first, 8));
    let loaded_end = loads.iter().map(|header| field(header, 16) + field(header, 40)).max().unwrap_or(0).wrapping_sub(bias);
    let region = (program.len() as u64).max(loaded_end).div_ceil(PAGE) * PAGE;
    let table = (count as u64 + 2) * 56;
    let note_at = region + table.div_ceil(4) * 4;
    let name = format!("{RESOURCE}\0");
    let mut note = Vec::new();
    put_u32(&mut note, name.len() as u32);
    put_u32(&mut note, u32::try_from(data.len()).map_err(|_| "the blob is past 4 GB".to_string())?);
    put_u32(&mut note, 0);
    note.extend_from_slice(name.as_bytes());
    note.resize(note.len().div_ceil(4) * 4, 0);
    note.extend_from_slice(data);
    note.resize(note.len().div_ceil(4) * 4, 0);
    let size = note_at - region + note.len() as u64;
    let header = |kind: u32, offset: u64, length: u64, align: u64| {
        let mut one = Vec::with_capacity(56);
        put_u32(&mut one, kind);
        put_u32(&mut one, 4);
        put_u64(&mut one, offset);
        put_u64(&mut one, bias + offset);
        put_u64(&mut one, bias + offset);
        put_u64(&mut one, length);
        put_u64(&mut one, length);
        put_u64(&mut one, align);
        <[u8; 56]>::try_from(one.as_slice()).unwrap_or([0; 56])
    };
    for one in headers.iter_mut().filter(|one| kind(one) == PT_PHDR) {
        *one = header(PT_PHDR, region, table, 8);
    }
    let last_load = headers.iter().rposition(|one| kind(one) == PT_LOAD).unwrap_or(headers.len() - 1);
    headers.insert(last_load + 1, header(PT_LOAD, region, size, PAGE));
    headers.push(header(PT_NOTE, note_at, note.len() as u64, 4));
    let mut out = program.to_vec();
    out.resize(region as usize, 0);
    for one in &headers {
        out.extend_from_slice(one);
    }
    out.resize(note_at as usize, 0);
    out.extend_from_slice(&note);
    out[0x20..0x28].copy_from_slice(&region.to_le_bytes());
    out[0x38..0x3A].copy_from_slice(&((count + 2) as u16).to_le_bytes());
    Ok(out)
}

#[cfg(test)]
mod building {
    use super::*;

    #[test]
    fn a_script_s_modules_from_outside_node_are_named() {
        let text = "const fs = require('fs');\nconst p = require(\"node:path\");\nconst x = require('./helper');\nimport y from 'left-pad';\nimport { readFile } from \"fs/promises\";\nconst z = await import(`chalk`);\n";
        assert_eq!(outside(text), vec!["./helper", "chalk", "left-pad"]);
        assert!(outside("console.log(require('os').cpus().length);").is_empty());
    }

    #[test]
    fn the_fuse_turns_once() {
        let mut program = b"head NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0 tail".to_vec();
        fuse(&mut program).unwrap();
        assert!(program.windows(FUSE.len()).all(|window| window != FUSE));
        assert!(String::from_utf8_lossy(&program).contains("b2:1 tail"));
        assert!(fuse(&mut program).is_err());
    }

    #[test]
    fn a_signature_at_the_end_is_cut_away() {
        let mut program = vec![0u8; 0x400];
        program[0..2].copy_from_slice(b"MZ");
        program[0x3C..0x40].copy_from_slice(&0x80u32.to_le_bytes());
        program[0x80..0x84].copy_from_slice(b"PE\0\0");
        program[0x98..0x9A].copy_from_slice(&0x20bu16.to_le_bytes());
        let entry = 0x98 + 112 + 32;
        program[entry..entry + 4].copy_from_slice(&0x300u32.to_le_bytes());
        program[entry + 4..entry + 8].copy_from_slice(&0x100u32.to_le_bytes());
        assert!(unsigned(&mut program).unwrap());
        assert_eq!(program.len(), 0x300);
        assert_eq!(&program[entry..entry + 8], &[0; 8]);
        assert!(!unsigned(&mut program).unwrap());
    }

    #[test]
    fn a_note_goes_in_a_loaded_segment_past_the_others() {
        let mut program = vec![0u8; 0x2000];
        program[0..6].copy_from_slice(b"\x7fELF\x02\x01");
        program[0x20..0x28].copy_from_slice(&0x40u64.to_le_bytes());
        program[0x36..0x38].copy_from_slice(&56u16.to_le_bytes());
        program[0x38..0x3A].copy_from_slice(&2u16.to_le_bytes());
        let mut put = |at: usize, kind: u32, offset: u64, vaddr: u64, filesz: u64, memsz: u64| {
            program[at..at + 4].copy_from_slice(&kind.to_le_bytes());
            program[at + 8..at + 16].copy_from_slice(&offset.to_le_bytes());
            program[at + 16..at + 24].copy_from_slice(&vaddr.to_le_bytes());
            program[at + 32..at + 40].copy_from_slice(&filesz.to_le_bytes());
            program[at + 40..at + 48].copy_from_slice(&memsz.to_le_bytes());
        };
        put(0x40, 6, 0x40, 0x400040, 112, 112);
        put(0x78, 1, 0, 0x400000, 0x2000, 0x5000);
        let built = with_note(&program, b"blob!").unwrap();
        assert_eq!(read_u64(&built, 0x20).unwrap(), 0x5000);
        assert_eq!(read_u16(&built, 0x38).unwrap(), 4);
        let at = |index: usize| 0x5000 + index * 56;
        assert_eq!(read_u32(&built, at(0)).unwrap(), 6);
        assert_eq!(read_u64(&built, at(0) + 16).unwrap(), 0x405000);
        assert_eq!(read_u32(&built, at(2)).unwrap(), 1);
        assert_eq!(read_u64(&built, at(2) + 8).unwrap(), 0x5000);
        assert_eq!(read_u32(&built, at(3)).unwrap(), 4);
        let note = read_u64(&built, at(3) + 8).unwrap() as usize;
        assert_eq!(read_u32(&built, note).unwrap(), 14);
        assert_eq!(read_u32(&built, note + 4).unwrap(), 5);
        assert_eq!(&built[note + 12..note + 26], b"NODE_SEA_BLOB\0");
        assert_eq!(&built[note + 28..note + 33], b"blob!");
    }
}
