// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The toolchains orior and the tree use, as toolchains.json lists them: what each is for, the
//! programs that name it, the page to install it from on each system, and the folders it is
//! usually installed in. orior installs none of them; it finds them, opens their install pages, and
//! puts their folders on the reader's PATH when asked.
//!
//! A tool is found in this order: the program its variable names, such as ORIOR_PYTHON; a folder
//! the reader gave orior for it; the PATH; then the folders it is usually installed in. On Windows
//! the PATH read is what the registry holds now, the system's and the reader's, ahead of the PATH
//! orior started with: a folder added since shows without starting orior again. A program in
//! a folder `notIn` names is passed over there: the bash in System32 is WSL's, and the python in
//! WindowsApps is the Store's stand-in.
//!
//! The folders the reader gave are kept in toolchains.json in orior's own folder, by tool, and every
//! job and terminal orior starts has them at the front of its PATH.
//!
//! The reader adds toolchains and groups of their own, kept in user_toolchains.json in orior's own
//! folder as { "groups": [...], "tools": [...] }, each tool written as toolchains.json writes one.
//! They come after orior's own in every list, each group after the groups orior's tools are in, and
//! a group may stand empty until a tool is added to it. Only what the reader added can be taken out.

use std::collections::{BTreeMap, HashMap, HashSet};
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

const TEXT: &str = include_str!("../toolchains.json");

/// How long a program may take to say its version before it is stopped.
const PATIENCE: Duration = Duration::from_secs(6);

#[derive(Deserialize, Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct Tool {
    pub id: String,
    pub name: String,
    pub group: String,
    #[serde(rename = "for")]
    pub uses: String,
    pub programs: Vec<String>,
    /// The words that make the first program found say its version; none where running it to ask
    /// opens a window or takes long.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub version: Option<Vec<String>>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub env: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub not_in: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub formats: Vec<String>,
    /// The shell line that runs a file, by its language, or by its language and extension, as
    /// run_file.rs reads it.
    #[serde(default, skip_serializing_if = "HashMap::is_empty")]
    pub runs: HashMap<String, String>,
    /// The language server the tool has for the editor, as servers.rs starts it, where it has one.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub server: Option<crate::servers::ServerSpec>,
    /// The debug adapter the tool has, as debug.rs starts it, where it has one.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub debugger: Option<crate::debug::DebuggerSpec>,
    /// The words that build a file for debugging, by its language or by its language and extension
    /// as `runs` keys them, as debug.rs reads them.
    #[serde(default, skip_serializing_if = "HashMap::is_empty")]
    pub builds: HashMap<String, Vec<String>>,
    /// How the tool formats a text of a language `formats` names, where it does.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub format: Option<Formatter>,
    /// The one system the tool is for, where it is for one.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub only: Option<String>,
    #[serde(default, skip_serializing_if = "HashMap::is_empty")]
    pub install: HashMap<String, String>,
    /// The shell line that installs it on each system, where its makers give one to run.
    #[serde(default, skip_serializing_if = "HashMap::is_empty")]
    pub setup: HashMap<String, String>,
    #[serde(default, skip_serializing_if = "HashMap::is_empty")]
    pub places: HashMap<String, Vec<String>>,
    /// Whether the tree's jobs cannot run without it, which the window checks for as a tree opens.
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub needed: bool,
}

/// A formatter's program, which is one of the tool's or beside them, and its words: it reads the
/// text on its input and writes it formatted on its output, and {file} in a word is the file's path,
/// by which it finds the project's own settings.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct Formatter {
    pub program: String,
    pub args: Vec<String>,
}

#[derive(Deserialize)]
struct Manifest {
    tools: Vec<Tool>,
}

/// A tool as orior finds it. `state` is "env", "chosen", "path", "found" or "missing": named by its
/// variable, in the folder the reader gave, on the PATH, installed in one of its usual folders and
/// not on the PATH, or not found. `program` is the program found and `folder` its folder.
#[derive(Serialize, Clone, Debug)]
pub struct Found {
    pub id: String,
    pub name: String,
    pub group: String,
    #[serde(rename = "for")]
    pub uses: String,
    pub programs: Vec<String>,
    pub formats: Vec<String>,
    pub state: &'static str,
    pub program: Option<String>,
    pub folder: Option<String>,
    pub chosen: Option<String>,
    pub install: Option<String>,
    pub setup: bool,
    pub versioned: bool,
    pub needed: bool,
    /// Whether the reader added it, which only such a tool can be taken out for.
    pub added: bool,
}

/// orior itself: its own folder, and whether that folder is on the PATH.
#[derive(Serialize, Clone, Debug)]
pub struct Own {
    pub folder: String,
    pub on_path: bool,
}

/// The toolchains and groups the reader added.
#[derive(Deserialize, Serialize, Default, Debug)]
pub struct Added {
    #[serde(default)]
    pub groups: Vec<String>,
    #[serde(default)]
    pub tools: Vec<Tool>,
}

fn bundled() -> Vec<Tool> {
    let read: Manifest = serde_json::from_str(TEXT).expect("toolchains.json is part of the program");
    read.tools
}

/// orior's own toolchains, then the reader's, those for another system left out.
pub fn manifest() -> Vec<Tool> {
    bundled().into_iter().chain(added().tools).filter(|tool| tool.only.as_deref().is_none_or(|only| only == system())).collect()
}

fn added_file() -> Option<PathBuf> {
    crate::home::folder().map(|folder| folder.join("user_toolchains.json"))
}

/// What the reader added, or nothing where they have added nothing or the file does not read.
pub fn added() -> Added {
    added_file().map(|path| added_in(&path)).unwrap_or_default()
}

fn added_in(path: &Path) -> Added {
    std::fs::read_to_string(path).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

fn keep_added(path: &Path, added: &Added) -> Result<(), String> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
    }
    let text = serde_json::to_string_pretty(added).map_err(|error| error.to_string())?;
    std::fs::write(path, text + "\n").map_err(|error| format!("{}: {error}", path.display()))
}

fn added_path() -> Result<PathBuf, String> {
    added_file().ok_or_else(|| "orior has no folder of its own to keep toolchains in".to_string())
}

/// Every group, in the order the sheet and the command line list them: those of orior's tools as
/// they first come, then those the reader added.
pub fn groups() -> Vec<String> {
    groups_of(&bundled(), &added())
}

fn groups_of(bundled: &[Tool], added: &Added) -> Vec<String> {
    let mut found: Vec<String> = Vec::new();
    for group in bundled.iter().chain(&added.tools).map(|tool| &tool.group).chain(&added.groups) {
        if !found.contains(group) {
            found.push(group.clone());
        }
    }
    found
}

/// An id made from a name: its letters and digits in lower case, each run of anything else a dash.
fn id_of(name: &str) -> String {
    let mut id = String::new();
    for char in name.trim().to_lowercase().chars() {
        if char.is_alphanumeric() {
            id.push(char);
        } else if !id.ends_with('-') && !id.is_empty() {
            id.push('-');
        }
    }
    id.trim_end_matches('-').to_string()
}

/// Adds the reader's toolchain `tool`, its id made from its name where it has none, and its group
/// with it where the group is new. Gives the id.
pub fn add(tool: Tool) -> Result<String, String> {
    add_in(&added_path()?, tool)
}

fn add_in(path: &Path, mut tool: Tool) -> Result<String, String> {
    tool.name = tool.name.trim().to_string();
    tool.group = tool.group.trim().to_string();
    tool.uses = tool.uses.trim().to_string();
    tool.programs = tool.programs.iter().map(|program| program.trim().to_string()).filter(|program| !program.is_empty()).collect();
    if tool.id.trim().is_empty() {
        tool.id = id_of(&tool.name);
    }
    tool.id = tool.id.trim().to_string();
    if tool.name.is_empty() || tool.id.is_empty() {
        return Err("a toolchain needs a name".into());
    }
    if tool.group.is_empty() {
        return Err(format!("{} needs a group", tool.name));
    }
    if tool.programs.is_empty() {
        return Err(format!("{} needs the name of at least one program to look for", tool.name));
    }
    let mut added = added_in(path);
    if bundled().iter().chain(&added.tools).any(|one| one.id == tool.id) {
        return Err(format!("there is a toolchain {} already", tool.id));
    }
    if !groups_of(&bundled(), &added).contains(&tool.group) {
        added.groups.push(tool.group.clone());
    }
    let id = tool.id.clone();
    added.tools.push(tool);
    keep_added(path, &added)?;
    Ok(id)
}

/// Adds an empty group of the reader's.
pub fn add_group(name: &str) -> Result<(), String> {
    add_group_in(&added_path()?, name)
}

fn add_group_in(path: &Path, name: &str) -> Result<(), String> {
    let name = name.trim();
    if name.is_empty() {
        return Err("a group needs a name".into());
    }
    let mut added = added_in(path);
    if groups_of(&bundled(), &added).iter().any(|group| group == name) {
        return Err(format!("there is a group {name} already"));
    }
    added.groups.push(name.to_string());
    keep_added(path, &added)
}

/// Takes out a toolchain the reader added, and the folder they gave for it.
pub fn remove(id: &str) -> Result<(), String> {
    remove_in(&added_path()?, id)?;
    forget(id)
}

fn remove_in(path: &Path, id: &str) -> Result<(), String> {
    let mut added = added_in(path);
    let before = added.tools.len();
    added.tools.retain(|tool| tool.id != id);
    if added.tools.len() == before {
        return Err(if bundled().iter().any(|tool| tool.id == id) { format!("{id} comes with orior: only a toolchain you added can be taken out") } else { format!("there is no toolchain {id}") });
    }
    keep_added(path, &added)
}

/// Takes out an empty group the reader added.
pub fn remove_group(name: &str) -> Result<(), String> {
    remove_group_in(&added_path()?, name)
}

fn remove_group_in(path: &Path, name: &str) -> Result<(), String> {
    let mut added = added_in(path);
    if bundled().iter().chain(&added.tools).any(|tool| tool.group == name) {
        return Err(format!("{name} holds toolchains: take them out first, or it is one of orior's own"));
    }
    let before = added.groups.len();
    added.groups.retain(|group| group != name);
    if added.groups.len() == before {
        return Err(format!("there is no group {name}"));
    }
    keep_added(path, &added)
}

fn tool(id: &str) -> Result<Tool, String> {
    manifest().into_iter().find(|tool| tool.id == id).ok_or_else(|| format!("orior knows no toolchain {id}: orior file toolchains lists them"))
}

/// "windows", "macos" or "linux": the key toolchains.json gives each system's pages and folders under.
pub fn system() -> &'static str {
    if cfg!(windows) {
        "windows"
    } else if cfg!(target_os = "macos") {
        "macos"
    } else {
        "linux"
    }
}

fn for_system<T>(map: &HashMap<String, T>) -> Option<&T> {
    map.get(system()).or_else(|| map.get("any"))
}

/// The names a program `name` has as a file here: name.exe, name.cmd and name.bat on Windows.
fn files_of(name: &str) -> Vec<String> {
    if cfg!(windows) {
        ["exe", "cmd", "bat"].iter().map(|ext| format!("{name}.{ext}")).collect()
    } else {
        vec![name.to_string()]
    }
}

/// The first of `names` in `dir`, as a file there.
pub fn program_in(dir: &Path, names: &[String]) -> Option<PathBuf> {
    names.iter().flat_map(|name| files_of(name)).map(|file| dir.join(file)).find(|path| path.is_file())
}

fn passed_over(dir: &Path, not_in: &[String]) -> bool {
    dir.components().any(|part| not_in.iter().any(|name| part.as_os_str().to_string_lossy().eq_ignore_ascii_case(name)))
}

/// A folder as written in toolchains.json, its variables filled in: %NAME% on Windows, and ~ and
/// $NAME elsewhere. A variable that is not set leaves the folder out.
fn expand(text: &str) -> Option<String> {
    let mut out = String::new();
    let mut rest = text;
    if let Some(after) = rest.strip_prefix('~') {
        out.push_str(&std::env::var("HOME").or_else(|_| std::env::var("USERPROFILE")).ok()?);
        rest = after;
    }
    while let Some(at) = rest.find(['%', '$']) {
        out.push_str(&rest[..at]);
        let mark = rest.as_bytes()[at];
        let after = &rest[at + 1..];
        let (name, next) = if mark == b'%' {
            let end = after.find('%')?;
            (&after[..end], &after[end + 1..])
        } else {
            let end = after.find(|c: char| !(c.is_ascii_alphanumeric() || c == '_')).unwrap_or(after.len());
            (&after[..end], &after[end..])
        };
        out.push_str(&std::env::var(name).ok()?);
        rest = next;
    }
    out.push_str(rest);
    Some(out)
}

/// Whether `name` matches `pattern`, whose * matches any run of characters, case aside on Windows.
fn matches(pattern: &str, name: &str) -> bool {
    let (pattern, name) = if cfg!(windows) { (pattern.to_lowercase(), name.to_lowercase()) } else { (pattern.to_string(), name.to_string()) };
    let parts: Vec<&str> = pattern.split('*').collect();
    if parts.len() == 1 {
        return pattern == name;
    }
    let mut at = 0;
    for (index, part) in parts.iter().enumerate() {
        if index == 0 {
            if !name.starts_with(part) {
                return false;
            }
            at = part.len();
        } else if index == parts.len() - 1 {
            return name.len() >= at + part.len() && name.ends_with(part);
        } else if let Some(found) = name[at..].find(part) {
            at += found + part.len();
        } else {
            return false;
        }
    }
    true
}

/// Orders names as their numbers read, so that Python314 comes after Python39.
fn natural(a: &str, b: &str) -> std::cmp::Ordering {
    let pieces = |text: &str| {
        let mut out: Vec<(bool, String)> = Vec::new();
        for c in text.chars() {
            let digit = c.is_ascii_digit();
            match out.last_mut() {
                Some((was, piece)) if *was == digit => piece.push(c),
                _ => out.push((digit, c.to_string())),
            }
        }
        out
    };
    for (x, y) in pieces(a).iter().zip(pieces(b).iter()) {
        let order = if x.0 && y.0 {
            x.1.trim_start_matches('0').len().cmp(&y.1.trim_start_matches('0').len()).then_with(|| x.1.trim_start_matches('0').cmp(y.1.trim_start_matches('0')))
        } else {
            x.1.to_lowercase().cmp(&y.1.to_lowercase())
        };
        if order != std::cmp::Ordering::Equal {
            return order;
        }
    }
    a.len().cmp(&b.len())
}

/// The folders a pattern of toolchains.json names, newest first where a * stands for a version.
fn folders_of(pattern: &str) -> Vec<PathBuf> {
    let Some(full) = expand(pattern) else {
        return Vec::new();
    };
    let path = PathBuf::from(&full);
    let mut found = vec![PathBuf::new()];
    for part in path.components() {
        let text = part.as_os_str().to_string_lossy().to_string();
        if !text.contains('*') {
            found.iter_mut().for_each(|dir| dir.push(part));
            continue;
        }
        let mut next = Vec::new();
        for dir in &found {
            let Ok(entries) = std::fs::read_dir(dir) else {
                continue;
            };
            let mut names: Vec<String> = entries.flatten().filter(|entry| entry.path().is_dir()).map(|entry| entry.file_name().to_string_lossy().to_string()).filter(|name| matches(&text, name)).collect();
            names.sort_by(|a, b| natural(b, a));
            next.extend(names.into_iter().map(|name| dir.join(name)));
        }
        found = next;
    }
    found.into_iter().filter(|dir| dir.is_dir()).collect()
}

/// The PATH's folders, in order and each once: on Windows the system's and the reader's as the
/// registry holds them now, then the PATH orior started with.
pub fn path_folders() -> Vec<PathBuf> {
    let mut all: Vec<PathBuf> = Vec::new();
    #[cfg(windows)]
    {
        for text in [registry::machine_path(), registry::user_path()].into_iter().flatten() {
            all.extend(std::env::split_paths(&OsString::from(registry::expand(&text))));
        }
    }
    if let Some(path) = std::env::var_os("PATH") {
        all.extend(std::env::split_paths(&path));
    }
    let mut seen = HashSet::new();
    all.into_iter().filter(|dir| !dir.as_os_str().is_empty() && seen.insert(key_of(dir))).collect()
}

/// A folder as PATH entries are compared: without a trailing separator, and case aside on Windows.
fn key_of(dir: &Path) -> String {
    let text = dir.to_string_lossy();
    let text = text.trim_end_matches(['\\', '/']);
    if cfg!(windows) {
        text.to_lowercase()
    } else {
        text.to_string()
    }
}

fn chosen_file() -> Option<PathBuf> {
    crate::home::folder().map(|folder| folder.join("toolchains.json"))
}

/// The folders the reader gave orior, by tool.
pub fn chosen() -> BTreeMap<String, String> {
    chosen_file().and_then(|path| std::fs::read_to_string(path).ok()).and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

fn keep_chosen(all: &BTreeMap<String, String>) -> Result<(), String> {
    let path = chosen_file().ok_or("orior has no folder of its own to keep toolchains in")?;
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
    }
    let text = serde_json::to_string_pretty(all).map_err(|error| error.to_string())?;
    std::fs::write(&path, text + "\n").map_err(|error| format!("{}: {error}", path.display()))
}

/// Has orior use `folder` for tool `id`, which must hold one of the tool's programs. Gives the
/// program found there.
pub fn choose(id: &str, folder: &str) -> Result<String, String> {
    let tool = tool(id)?;
    let dir = PathBuf::from(folder.trim());
    let program = program_in(&dir, &tool.programs).ok_or_else(|| format!("{} holds none of {}", dir.display(), tool.programs.join(", ")))?;
    let mut all = chosen();
    all.insert(tool.id.clone(), dir.display().to_string());
    keep_chosen(&all)?;
    Ok(program.display().to_string())
}

/// Has orior forget the folder the reader gave for tool `id`.
pub fn forget(id: &str) -> Result<(), String> {
    let mut all = chosen();
    if all.remove(id).is_none() {
        return Ok(());
    }
    keep_chosen(&all)
}

/// Finds one tool, as the module's opening says.
pub fn find(tool: &Tool, path: &[PathBuf], kept: &BTreeMap<String, String>) -> Found {
    let mut found = Found {
        id: tool.id.clone(),
        name: tool.name.clone(),
        group: tool.group.clone(),
        uses: tool.uses.clone(),
        programs: tool.programs.clone(),
        formats: tool.formats.clone(),
        state: "missing",
        program: None,
        folder: None,
        chosen: kept.get(&tool.id).cloned(),
        install: for_system(&tool.install).cloned(),
        setup: for_system(&tool.setup).is_some(),
        versioned: tool.version.is_some(),
        needed: tool.needed,
        added: false,
    };
    let mut set = |state: &'static str, program: PathBuf| {
        found.state = state;
        found.folder = program.parent().map(|dir| dir.display().to_string());
        found.program = Some(program.display().to_string());
    };
    if let Some(named) = tool.env.as_ref().and_then(std::env::var_os) {
        set("env", PathBuf::from(named));
        return found;
    }
    if let Some(program) = found.chosen.as_ref().and_then(|dir| program_in(Path::new(dir), &tool.programs)) {
        set("chosen", program);
        return found;
    }
    for name in &tool.programs {
        if let Some(program) = path.iter().filter(|dir| !passed_over(dir, &tool.not_in)).find_map(|dir| program_in(dir, std::slice::from_ref(name))) {
            set("path", program);
            return found;
        }
    }
    let places = for_system(&tool.places).cloned().unwrap_or_default();
    if let Some(program) = places.iter().flat_map(|pattern| folders_of(pattern)).filter(|dir| !passed_over(dir, &tool.not_in)).find_map(|dir| program_in(&dir, &tool.programs)) {
        set("found", program);
    }
    found
}

/// Every toolchain, as orior finds it now.
pub fn check() -> Vec<Found> {
    let path = path_folders();
    let kept = chosen();
    let mine: HashSet<String> = added().tools.into_iter().map(|tool| tool.id).collect();
    manifest()
        .iter()
        .map(|tool| Found { added: mine.contains(&tool.id), ..find(tool, &path, &kept) })
        .collect()
}

/// The program orior runs for tool `id` where the reader gave a folder for it.
pub fn chosen_program(id: &str) -> Option<PathBuf> {
    let tool = manifest().into_iter().find(|tool| tool.id == id)?;
    chosen().get(id).and_then(|dir| program_in(Path::new(dir), &tool.programs))
}

/// What tool `id` says its version is: the first line it writes with a dotted number in it, such as
/// 13.3, which a copyright's years are not.
pub fn version(id: &str) -> Result<String, String> {
    let tool = tool(id)?;
    let words = tool.version.clone().ok_or_else(|| format!("{} is not asked its version: running it opens a window or takes long", tool.name))?;
    let found = find(&tool, &path_folders(), &chosen());
    let program = found.program.ok_or_else(|| format!("{} is not found", tool.name))?;
    let mut command = Command::new(&program);
    command.args(&words).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| format!("{program}: {error}"))?;
    let began = Instant::now();
    loop {
        match child.try_wait() {
            Ok(Some(_)) => break,
            Ok(None) if began.elapsed() < PATIENCE => std::thread::sleep(Duration::from_millis(40)),
            _ => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(format!("{} did not say its version in {} s", tool.name, PATIENCE.as_secs()));
            }
        }
    }
    let output = child.wait_with_output().map_err(|error| error.to_string())?;
    let text = format!("{}\n{}", String::from_utf8_lossy(&output.stdout), String::from_utf8_lossy(&output.stderr));
    let numbered = |line: &&str| line.as_bytes().windows(3).any(|three| three[0].is_ascii_digit() && three[1] == b'.' && three[2].is_ascii_digit());
    let line = text.lines().map(str::trim).filter(|line| !line.is_empty()).find(numbered).or_else(|| text.lines().map(str::trim).find(|line| !line.is_empty()));
    line.map(|line| line.chars().take(120).collect()).ok_or_else(|| format!("{} said nothing", tool.name))
}

/// The versions of every tool found that is asked its version, each asked at once.
pub fn versions(found: &[Found]) -> HashMap<String, String> {
    let asks: Vec<_> = found.iter().filter(|one| one.program.is_some() && one.versioned).map(|one| one.id.clone()).collect();
    let handles: Vec<_> = asks.into_iter().map(|id| std::thread::spawn(move || (id.clone(), version(&id)))).collect();
    handles.into_iter().filter_map(|handle| handle.join().ok()).filter_map(|(id, said)| said.ok().map(|said| (id, said))).collect()
}

/// orior's own folder, and whether it is on the PATH, so that `orior` works in any terminal.
pub fn own() -> Result<Own, String> {
    let exe = std::env::current_exe().map_err(|error| error.to_string())?;
    let folder = dunce::canonicalize(&exe).unwrap_or(exe).parent().map(Path::to_path_buf).ok_or("orior's program has no folder")?;
    let key = key_of(&folder);
    let on_path = path_folders().iter().any(|dir| key_of(dir) == key);
    Ok(Own { folder: folder.display().to_string(), on_path })
}

/// The shell line that installs tool `id` on this system, as its makers give it.
pub fn setup_line(id: &str) -> Result<String, String> {
    let tool = tool(id)?;
    for_system(&tool.setup).cloned().ok_or_else(|| format!("orior has no way to install {} on {}: its install page has the steps", tool.name, system()))
}

/// Opens the install page of tool `id` for this system in the browser.
pub fn open_install(id: &str) -> Result<String, String> {
    let tool = tool(id)?;
    let url = for_system(&tool.install).ok_or_else(|| format!("{} has no install page for {}", tool.name, system()))?.clone();
    if !url.starts_with("https://") {
        return Err(format!("{url} is not an https page"));
    }
    crate::report::open_url(&url)?;
    Ok(url)
}

/// `path` with `folder` added at its end, where it is not there already.
pub fn joined(path: &str, folder: &str) -> Option<String> {
    let separator = if cfg!(windows) { ';' } else { ':' };
    let key = key_of(Path::new(folder));
    if path.split(separator).any(|dir| !dir.is_empty() && key_of(Path::new(dir)) == key) {
        return None;
    }
    let path = path.trim_end_matches(separator);
    Some(if path.is_empty() { folder.to_string() } else { format!("{path}{separator}{folder}") })
}

/// Adds a folder to the reader's own PATH, for every terminal and program started after: "orior"
/// adds orior's own folder, and a tool's id the folder its program was found in. Gives the folder.
/// On Windows that is the reader's Path in the registry, kept as text whose variables expand, and
/// every open program is told it changed; elsewhere it is a line at the end of ~/.profile, and of
/// ~/.zprofile on macOS, which a new login shell reads.
pub fn add_to_path(what: &str) -> Result<String, String> {
    let folder = if what == "orior" {
        own()?.folder
    } else {
        let tool = tool(what)?;
        let found = find(&tool, &path_folders(), &chosen());
        found.folder.ok_or_else(|| format!("{} is not found: install it or give orior its folder first", tool.name))?
    };
    add_folder("Path", &folder)?;
    Ok(folder)
}

#[cfg(windows)]
fn add_folder(variable: &str, folder: &str) -> Result<(), String> {
    let now = registry::user_value(variable).unwrap_or_default();
    if let Some(next) = joined(&now, folder) {
        registry::set_user_value(variable, &next)?;
        registry::announce();
    }
    Ok(())
}

#[cfg(not(windows))]
fn add_folder(_variable: &str, folder: &str) -> Result<(), String> {
    let home = std::env::var("HOME").map_err(|_| "HOME is not set".to_string())?;
    let mut files = vec![PathBuf::from(&home).join(".profile")];
    if cfg!(target_os = "macos") {
        files.push(PathBuf::from(&home).join(".zprofile"));
    }
    for name in [".bash_profile", ".bash_login"] {
        let file = PathBuf::from(&home).join(name);
        if file.is_file() {
            files.push(file);
        }
    }
    let line = format!("export PATH=\"$PATH:{}\"", folder.replace('"', "\\\""));
    for file in files {
        let text = std::fs::read_to_string(&file).unwrap_or_default();
        if text.lines().any(|kept| kept.trim() == line) {
            continue;
        }
        let gap = if text.is_empty() || text.ends_with('\n') { "" } else { "\n" };
        std::fs::write(&file, format!("{text}{gap}{line}\n")).map_err(|error| format!("{}: {error}", file.display()))?;
    }
    Ok(())
}

/// The PATH a job or a terminal orior starts gets: the folders the reader gave, then the PATH as
/// path_folders reads it.
/// The folders that exist of those written by system as `places` writes them, each pattern's in
/// order.
pub fn folders_for(by_system: &HashMap<String, Vec<String>>) -> Vec<PathBuf> {
    for_system(by_system).cloned().unwrap_or_default().iter().flat_map(|pattern| folders_of(pattern)).filter(|dir| dir.is_dir()).collect()
}

/// The PATH jobs run with, with `ahead` in front of it.
pub fn run_path_with(ahead: &[PathBuf]) -> OsString {
    let mut dirs: Vec<PathBuf> = ahead.to_vec();
    dirs.extend(std::env::split_paths(&run_path()));
    std::env::join_paths(dirs).unwrap_or_else(|_| run_path())
}

pub fn run_path() -> OsString {
    let kept = chosen();
    let mut dirs: Vec<PathBuf> = kept.values().map(PathBuf::from).filter(|dir| dir.is_dir()).collect();
    dirs.extend(path_folders());
    let mut seen = HashSet::new();
    let dirs: Vec<PathBuf> = dirs.into_iter().filter(|dir| seen.insert(key_of(dir))).collect();
    std::env::join_paths(dirs).unwrap_or_else(|_| std::env::var_os("PATH").unwrap_or_default())
}

#[cfg(windows)]
mod registry {
    use std::ptr::{null, null_mut};

    use windows_sys::Win32::Foundation::ERROR_SUCCESS;
    use windows_sys::Win32::System::Environment::ExpandEnvironmentStringsW;
    use windows_sys::Win32::System::Registry::{
        RegCloseKey, RegOpenKeyExW, RegQueryValueExW, RegSetValueExW, HKEY, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, KEY_READ, KEY_WRITE, REG_EXPAND_SZ, REG_SZ, REG_VALUE_TYPE,
    };
    use windows_sys::Win32::UI::WindowsAndMessaging::{SendMessageTimeoutW, HWND_BROADCAST, SMTO_ABORTIFHUNG, WM_SETTINGCHANGE};

    fn wide(text: &str) -> Vec<u16> {
        text.encode_utf16().chain(std::iter::once(0)).collect()
    }

    fn read(root: HKEY, key: &str, name: &str) -> Option<String> {
        let mut opened: HKEY = null_mut();
        let key = wide(key);
        let name = wide(name);
        unsafe {
            if RegOpenKeyExW(root, key.as_ptr(), 0, KEY_READ, &mut opened) != ERROR_SUCCESS {
                return None;
            }
            let mut size = 0u32;
            let mut kind: REG_VALUE_TYPE = 0;
            let mut said = RegQueryValueExW(opened, name.as_ptr(), null(), &mut kind, null_mut(), &mut size);
            let mut data = vec![0u16; (size as usize).div_ceil(2) + 1];
            if said == ERROR_SUCCESS {
                said = RegQueryValueExW(opened, name.as_ptr(), null(), &mut kind, data.as_mut_ptr().cast(), &mut size);
            }
            RegCloseKey(opened);
            if said != ERROR_SUCCESS || (kind != REG_SZ && kind != REG_EXPAND_SZ) {
                return None;
            }
            let end = data.iter().position(|&unit| unit == 0).unwrap_or(data.len());
            Some(String::from_utf16_lossy(&data[..end]))
        }
    }

    pub fn machine_path() -> Option<String> {
        read(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", "Path")
    }

    pub fn user_path() -> Option<String> {
        user_value("Path")
    }

    pub fn user_value(name: &str) -> Option<String> {
        read(HKEY_CURRENT_USER, "Environment", name)
    }

    /// Sets a variable of the reader's own as text whose %NAME% parts expand, as Windows keeps Path.
    pub fn set_user_value(name: &str, value: &str) -> Result<(), String> {
        let mut opened: HKEY = null_mut();
        let key = wide("Environment");
        let wide_name = wide(name);
        let data = wide(value);
        unsafe {
            let said = RegOpenKeyExW(HKEY_CURRENT_USER, key.as_ptr(), 0, KEY_READ | KEY_WRITE, &mut opened);
            if said != ERROR_SUCCESS {
                return Err(format!("the reader's environment did not open: error {said}"));
            }
            let said = RegSetValueExW(opened, wide_name.as_ptr(), 0, REG_EXPAND_SZ, data.as_ptr().cast(), (data.len() * 2) as u32);
            RegCloseKey(opened);
            if said != ERROR_SUCCESS {
                return Err(format!("{name} was not written: error {said}"));
            }
        }
        Ok(())
    }

    #[cfg(test)]
    pub fn remove_user_value(name: &str) {
        use windows_sys::Win32::System::Registry::RegDeleteKeyValueW;
        let key = wide("Environment");
        let name = wide(name);
        unsafe {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, key.as_ptr(), name.as_ptr());
        }
    }

    /// Tells every open program that the environment changed, as Windows' own settings do, so that
    /// Explorer starts new terminals with it.
    pub fn announce() {
        let what = wide("Environment");
        let mut result = 0usize;
        unsafe {
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, what.as_ptr() as isize, SMTO_ABORTIFHUNG, 3000, &mut result);
        }
    }

    pub fn expand(text: &str) -> String {
        let source = wide(text);
        unsafe {
            let size = ExpandEnvironmentStringsW(source.as_ptr(), null_mut(), 0);
            if size == 0 {
                return text.to_string();
            }
            let mut out = vec![0u16; size as usize];
            let written = ExpandEnvironmentStringsW(source.as_ptr(), out.as_mut_ptr(), size);
            if written == 0 {
                return text.to_string();
            }
            let end = out.iter().position(|&unit| unit == 0).unwrap_or(out.len());
            String::from_utf16_lossy(&out[..end])
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_jobs_need_git_bash_and_python() {
        let needed: Vec<String> = manifest().into_iter().filter(|tool| tool.needed).map(|tool| tool.id).collect();
        assert_eq!(needed, ["git", "bash", "python"]);
    }

    #[test]
    fn the_manifest_reads_and_each_id_is_one_tool() {
        let all: Vec<Tool> = serde_json::from_str::<Manifest>(TEXT).unwrap().tools;
        let mut seen = HashSet::new();
        for tool in &all {
            assert!(seen.insert(tool.id.clone()), "{} twice", tool.id);
            assert!(!tool.programs.is_empty(), "{} names no program", tool.id);
            assert!(tool.install.values().all(|url| url.starts_with("https://")), "{} has a page that is not https", tool.id);
            assert!(!tool.install.is_empty(), "{} has no install page", tool.id);
        }
    }

    #[test]
    fn a_reader_adds_and_takes_out_toolchains_and_groups() {
        let path = std::env::temp_dir().join(format!("orior-user-toolchains-{}.json", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let tool = |name: &str, group: &str| Tool { name: name.into(), group: group.into(), programs: vec!["zig".into()], ..serde_json::from_str(r#"{"id":"","name":"","group":"","for":"","programs":[]}"#).unwrap() };
        assert_eq!(add_in(&path, tool("Zig 0.13", "Systems")).unwrap(), "zig-0-13");
        assert!(add_in(&path, tool("Zig 0.13", "Systems")).unwrap_err().contains("already"));
        assert!(add_in(&path, tool("Git", "Systems")).is_err(), "an id orior has is refused");
        add_group_in(&path, "Proof assistants").unwrap();
        let read = added_in(&path);
        assert_eq!(read.groups, ["Systems", "Proof assistants"]);
        assert!(groups_of(&bundled(), &read).ends_with(&["Systems".to_string(), "Proof assistants".to_string()]));
        assert!(remove_group_in(&path, "Systems").is_err(), "a group holding a tool stays");
        assert!(remove_in(&path, "git").unwrap_err().contains("comes with orior"));
        remove_in(&path, "zig-0-13").unwrap();
        remove_group_in(&path, "Systems").unwrap();
        assert_eq!(added_in(&path).groups, ["Proof assistants"]);
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn a_star_matches_any_run_and_versions_sort_as_numbers() {
        assert!(matches("Python3*", "Python314"));
        assert!(matches("R-*", "R-4.6.1"));
        assert!(!matches("v*x", "v13.3"));
        let mut names = vec!["Python39", "Python314", "Python312"];
        names.sort_by(|a, b| natural(b, a));
        assert_eq!(names, vec!["Python314", "Python312", "Python39"]);
    }

    #[test]
    fn a_folder_already_on_the_path_is_not_added_twice() {
        let separator = if cfg!(windows) { ";" } else { ":" };
        let path = format!("a{separator}b{separator}");
        assert_eq!(joined(&path, "c"), Some(format!("a{separator}b{separator}c")));
        assert_eq!(joined(&format!("a{separator}c"), "c"), None);
        assert_eq!(joined("", "c"), Some("c".to_string()));
        if cfg!(windows) {
            assert_eq!(joined("C:\\Tools\\", "c:\\tools"), None);
        }
    }

    #[test]
    fn a_tool_in_a_chosen_folder_is_found_there_first() {
        let dir = std::env::temp_dir().join(format!("orior-toolchains-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let file = dir.join(&files_of("ghdl")[0]);
        std::fs::write(&file, "").unwrap();
        let tool = manifest().into_iter().find(|tool| tool.id == "ghdl").unwrap();
        let kept: BTreeMap<String, String> = [("ghdl".to_string(), dir.display().to_string())].into_iter().collect();
        let found = find(&tool, &[], &kept);
        assert_eq!(found.state, "chosen");
        assert_eq!(found.program.as_deref(), Some(file.display().to_string().as_str()));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[cfg(windows)]
    #[test]
    #[ignore = "writes a variable of the reader's own to the registry, then removes it"]
    fn a_folder_reaches_the_reader_s_environment() {
        let name = "ORIOR_PATH_CHECK";
        registry::remove_user_value(name);
        add_folder(name, "C:\\one").unwrap();
        add_folder(name, "%USERPROFILE%\\two").unwrap();
        add_folder(name, "c:\\one\\").unwrap();
        let kept = registry::user_value(name);
        registry::remove_user_value(name);
        assert_eq!(kept.as_deref(), Some("C:\\one;%USERPROFILE%\\two"));
    }
}
