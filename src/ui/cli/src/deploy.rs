// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Deployments: a build's files sent to the places a tree names, to several at once. The tree names
//! them in deploy.json at its top folder, each deployment by its name, the folder whose files go,
//! the patterns of the files that stay, the places, and how many releases each place keeps:
//!
//! ```json
//! {"deployments": [{"name": "site", "from": "dist", "exclude": ["*.map"], "to": ["me@web1:/srv/site"], "keep": 5}]}
//! ```
//!
//! A place is an address of a machine as orior reaches one, user@host:folder over ssh with the keys
//! the system's agent holds, docker:container:folder or wsl:distribution:folder, or a folder on this
//! machine. Its files go as a tar stream, which keeps each file's bytes, line ends and all, and its
//! mode, as git's index gives it where the tree is in git.
//!
//! On a machine, each deployment goes to a new folder under releases/, and `current` is turned to it
//! only once every place holds the new release: a place that fails leaves each place on the release
//! it had. Roll back turns each place's `current` to the release before it. A deployment `inPlace`
//! sends its files into the folder itself, as a folder on Windows takes them.
//!
//! Before a file goes, the copy the place holds is compared with the copy last sent there: a copy
//! changed there since is shown with its differences, to keep or to replace. The files a place
//! holds that the tree no longer does are listed, to keep or to delete.

use std::collections::{BTreeMap, HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use serde::Deserialize;

use crate::catalog::{Arg, Job, Param, Program, Step};
use crate::digest::sha256;
use crate::link::{quote, Address};

/// The patterns every deployment leaves out, with those its own file names.
const ALWAYS_OUT: [&str; 1] = [".git/"];

#[derive(Deserialize, Clone, Debug, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct Deployment {
    pub name: String,
    #[serde(default = "the_top")]
    pub from: String,
    #[serde(default)]
    pub exclude: Vec<String>,
    pub to: Vec<String>,
    #[serde(default = "five")]
    pub keep: usize,
    #[serde(default)]
    pub in_place: bool,
}

fn the_top() -> String {
    ".".to_string()
}

fn five() -> usize {
    5
}

#[derive(Deserialize)]
struct Named {
    deployments: Vec<Deployment>,
}

/// The deployments deploy.json at the tree's top folder names; none where it is not there.
pub fn read(root: &Path) -> Result<Vec<Deployment>, String> {
    let path = root.join("deploy.json");
    let Ok(text) = fs::read_to_string(&path) else { return Ok(Vec::new()) };
    serde_json::from_str::<Named>(&text).map(|named| named.deployments).map_err(|error| format!("deploy.json: {error}"))
}

/// A place a deployment sends to: a machine orior reaches, or a folder on this one.
#[derive(Clone, Debug, PartialEq)]
enum Place {
    Machine(Address),
    Here(PathBuf),
}

impl Place {
    fn parse(text: &str, root: &Path) -> Result<Place, String> {
        let text = text.trim();
        let bytes = text.as_bytes();
        let drive = bytes.len() >= 2 && bytes[0].is_ascii_alphabetic() && bytes[1] == b':' && (bytes.len() == 2 || matches!(bytes[2], b'/' | b'\\'));
        if drive || text.starts_with(['/', '.', '\\']) {
            let path = PathBuf::from(text);
            return Ok(Place::Here(if path.is_absolute() { path } else { root.join(path) }));
        }
        Address::parse(text).map(Place::Machine)
    }

    fn name(&self) -> String {
        match self {
            Place::Machine(address) => address.text(),
            Place::Here(path) => path.display().to_string(),
        }
    }

    /// Runs the shell line on the machine, `input` given to it, and gives what it wrote; a machine
    /// that asks for a password is told how a key reaches it in place of one.
    fn ask(&self, line: &str, input: Option<&[u8]>) -> Result<String, String> {
        let Place::Machine(address) = self else { return Err("a folder on this machine takes no shell line".to_string()) };
        address.ask(line, input).map_err(|said| {
            if said.contains("Permission denied") || said.contains("password") {
                format!("{said}; {} asks for a password, which orior does not keep: a key the system's ssh agent holds reaches it, and ssh-copy-id puts one there", address.machine())
            } else {
                said
            }
        })
    }

    fn folder(&self) -> String {
        match self {
            Place::Machine(address) => address.folder.clone(),
            Place::Here(path) => path.display().to_string(),
        }
    }
}

/// A file a deployment sends: its path under the folder sent, its full path here, and its mode.
#[derive(Clone, Debug)]
struct Sent {
    path: String,
    full: PathBuf,
    mode: u32,
}

/// Whether `name` fits the glob `pattern`, `*` taking any run of characters and `?` any one.
fn fits(pattern: &str, name: &str) -> bool {
    let (pattern, name): (Vec<char>, Vec<char>) = (pattern.chars().collect(), name.chars().collect());
    let (mut p, mut n, mut star, mut mark) = (0, 0, None, 0);
    while n < name.len() {
        if p < pattern.len() && (pattern[p] == '?' || pattern[p] == name[n]) {
            p += 1;
            n += 1;
        } else if p < pattern.len() && pattern[p] == '*' {
            star = Some(p);
            mark = n;
            p += 1;
        } else if let Some(at) = star {
            p = at + 1;
            mark += 1;
            n = mark;
        } else {
            return false;
        }
    }
    pattern[p..].iter().all(|char| *char == '*')
}

/// The pattern that leaves out the file at `path`, under the folder sent, where one does: one that
/// ends in `/` leaves out a folder of that name or path and all under it, one with a `/` in it a
/// path, and any other a file or folder of that name anywhere.
fn left_out_by<'a>(patterns: &'a [String], path: &str) -> Option<&'a String> {
    let parts: Vec<&str> = path.split('/').collect();
    patterns.iter().find(|pattern| {
        if let Some(folder) = pattern.strip_suffix('/') {
            if folder.contains('/') {
                return path.starts_with(&format!("{folder}/")) || parts.windows(folder.split('/').count()).any(|run| fits(folder, &run.join("/")));
            }
            return parts[..parts.len() - 1].iter().any(|part| fits(folder, part));
        }
        if pattern.contains('/') {
            return fits(pattern.trim_start_matches('/'), path);
        }
        parts.iter().any(|part| fits(pattern, part))
    })
}

/// The modes git's index gives the tree's files, by their paths in the tree.
fn index_modes(root: &Path) -> HashMap<String, u32> {
    let mut cmd = std::process::Command::new("git");
    cmd.args(["ls-files", "-s"]).current_dir(root);
    crate::runner::quiet(&mut cmd);
    let Ok(out) = cmd.output() else { return HashMap::new() };
    String::from_utf8_lossy(&out.stdout)
        .lines()
        .filter_map(|line| {
            let (head, path) = line.split_once('\t')?;
            let mode = u32::from_str_radix(head.split(' ').next()?, 8).ok()?;
            Some((path.to_string(), if mode & 0o111 != 0 { 0o755 } else { 0o644 }))
        })
        .collect()
}

#[cfg(unix)]
fn mode_here(path: &Path) -> u32 {
    use std::os::unix::fs::PermissionsExt;
    fs::metadata(path).map(|meta| if meta.permissions().mode() & 0o111 != 0 { 0o755 } else { 0o644 }).unwrap_or(0o644)
}

#[cfg(not(unix))]
fn mode_here(_: &Path) -> u32 {
    0o644
}

/// The files a deployment sends, and those it leaves out, each with the pattern that leaves it out.
fn gather(root: &Path, deployment: &Deployment) -> Result<(Vec<Sent>, Vec<(String, String)>), String> {
    let top = root.join(&deployment.from);
    if !top.is_dir() {
        return Err(format!("{} names {}, which is no folder of the tree", deployment.name, deployment.from));
    }
    let patterns: Vec<String> = ALWAYS_OUT.iter().map(|one| one.to_string()).chain(deployment.exclude.iter().cloned()).collect();
    let folder_patterns: Vec<String> = patterns.iter().filter(|pattern| pattern.ends_with('/')).cloned().collect();
    let modes = index_modes(root);
    let (mut sent, mut out) = (Vec::new(), Vec::new());
    let mut folders = vec![top.clone()];
    while let Some(folder) = folders.pop() {
        let Ok(entries) = fs::read_dir(&folder) else { continue };
        for entry in entries.flatten() {
            let full = entry.path();
            let path = full.strip_prefix(&top).map(|inner| inner.to_string_lossy().replace('\\', "/")).unwrap_or_default();
            let Ok(kind) = entry.file_type() else { continue };
            if kind.is_dir() {
                match left_out_by(&folder_patterns, &format!("{path}/x")) {
                    Some(rule) => out.push((format!("{path}/"), rule.clone())),
                    None => folders.push(full),
                }
                continue;
            }
            if let Some(rule) = left_out_by(&patterns, &path) {
                out.push((path, rule.clone()));
                continue;
            }
            let in_tree = full.strip_prefix(root).map(|inner| inner.to_string_lossy().replace('\\', "/")).unwrap_or_default();
            let mode = modes.get(&in_tree).copied().unwrap_or_else(|| mode_here(&full));
            sent.push(Sent { path, full, mode });
        }
    }
    sent.sort_by(|one, other| one.path.cmp(&other.path));
    out.sort();
    Ok((sent, out))
}

/// The fields of a ustar header, its checksum filled in.
fn tar_header(name: &str, size: u64, mode: u32, time: u64, kind: u8) -> [u8; 512] {
    let mut header = [0u8; 512];
    let mut put = |at: usize, width: usize, text: &[u8]| header[at..at + text.len().min(width)].copy_from_slice(&text[..text.len().min(width)]);
    put(0, 100, name.as_bytes());
    put(100, 8, format!("{mode:07o}\0").as_bytes());
    put(108, 8, b"0000000\0");
    put(116, 8, b"0000000\0");
    put(124, 12, format!("{size:011o}\0").as_bytes());
    put(136, 12, format!("{time:011o}\0").as_bytes());
    put(148, 8, b"        ");
    put(156, 1, &[kind]);
    put(257, 6, b"ustar\0");
    put(263, 2, b"00");
    let sum: u32 = header.iter().map(|byte| *byte as u32).sum();
    header[148..156].copy_from_slice(format!("{sum:06o}\0 ").as_bytes());
    header
}

fn tar_pad(out: &mut Vec<u8>) {
    out.resize(out.len().div_ceil(512) * 512, 0);
}

/// A tar stream of `files`, each with its mode and the time given: a path longer than a ustar
/// header holds goes in a PAX header before it.
fn tar(files: &[Sent], time: u64) -> Result<Vec<u8>, String> {
    let mut out = Vec::new();
    for file in files {
        let bytes = fs::read(&file.full).map_err(|error| format!("{}: {error}", file.full.display()))?;
        if file.path.len() > 99 {
            let record = format!(" path={}\n", file.path);
            let mut length = record.len();
            while format!("{}{record}", length).len() != length {
                length = format!("{}{record}", length).len();
            }
            let pax = format!("{length}{record}");
            out.extend_from_slice(&tar_header("PaxHeader", pax.len() as u64, 0o644, time, b'x'));
            out.extend_from_slice(pax.as_bytes());
            tar_pad(&mut out);
        }
        out.extend_from_slice(&tar_header(&file.path, bytes.len() as u64, file.mode, time, b'0'));
        out.extend_from_slice(&bytes);
        tar_pad(&mut out);
    }
    out.extend_from_slice(&[0u8; 1024]);
    Ok(out)
}

/// The days since 1970 as a year, a month and a day, by the proleptic Gregorian calendar.
fn civil(days: i64) -> (i64, u32, u32) {
    let shifted = days + 719_468;
    let era = shifted.div_euclid(146_097);
    let of_era = shifted - era * 146_097;
    let year_of_era = (of_era - of_era / 1460 + of_era / 36_524 - of_era / 146_096) / 365;
    let day_of_year = of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_index = (5 * day_of_year + 2) / 153;
    let day = (day_of_year - (153 * month_index + 2) / 5 + 1) as u32;
    let month = if month_index < 10 { month_index + 3 } else { month_index - 9 } as u32;
    (year_of_era + era * 400 + i64::from(month <= 2), month, day)
}

/// A release's name, the time it is made as UTC, which sorts as the releases were made.
fn release_name(seconds: u64) -> String {
    let (year, month, day) = civil((seconds / 86_400) as i64);
    let rest = seconds % 86_400;
    format!("{year:04}{month:02}{day:02}-{:02}{:02}{:02}", rest / 3600, rest % 3600 / 60, rest % 60)
}

/// Where the hashes of the files last sent to a place are kept, in orior's own folder.
fn record_path(root: &Path, deployment: &str, place: &Place) -> Option<PathBuf> {
    let key = format!("{}\n{deployment}\n{}", root.display(), place.name());
    Some(crate::home::folder()?.join("deployed").join(format!("{}.json", &sha256(key.as_bytes())[..16])))
}

fn read_record(path: Option<&PathBuf>) -> BTreeMap<String, String> {
    path.and_then(|path| fs::read_to_string(path).ok()).and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

fn write_record(path: Option<&PathBuf>, hashes: &BTreeMap<String, String>) {
    let Some(path) = path else { return };
    if let Some(folder) = path.parent() {
        let _ = fs::create_dir_all(folder);
    }
    let _ = fs::write(path, serde_json::to_string_pretty(hashes).unwrap_or_default());
}

/// The hashes of the files under `folder` of a place, by their paths under it.
fn hashes_there(place: &Place, folder: &str) -> Result<BTreeMap<String, String>, String> {
    match place {
        Place::Here(_) => {
            let mut found = BTreeMap::new();
            let top = PathBuf::from(folder);
            let mut folders = vec![top.clone()];
            while let Some(one) = folders.pop() {
                for entry in fs::read_dir(&one).into_iter().flatten().flatten() {
                    let full = entry.path();
                    if full.is_dir() {
                        folders.push(full);
                    } else if let Ok(bytes) = fs::read(&full) {
                        let path = full.strip_prefix(&top).map(|inner| inner.to_string_lossy().replace('\\', "/")).unwrap_or_default();
                        found.insert(path, sha256(&bytes));
                    }
                }
            }
            Ok(found)
        }
        Place::Machine(_) => {
            let said = place.ask(&format!("cd {} 2>/dev/null || exit 0; find . -type f -exec sha256sum {{}} + 2>/dev/null; true", quote(folder)), None)?;
            Ok(said
                .lines()
                .filter_map(|line| {
                    let (hash, path) = line.split_once("  ")?;
                    Some((path.trim_start_matches("./").to_string(), hash.trim_start_matches('\\').to_string()))
                })
                .collect())
        }
    }
}

/// The text of a file a place holds, where it is text of 64 KB at most.
fn text_there(place: &Place, path: &str) -> Option<String> {
    let bytes = match place {
        Place::Here(_) => fs::read(path).ok()?,
        Place::Machine(_) => place.ask(&format!("head -c 65537 {}", quote(path)), None).ok()?.into_bytes(),
    };
    (bytes.len() <= 65_536 && !bytes.contains(&0)).then(|| String::from_utf8_lossy(&bytes).into_owned())
}

/// The lines of `new` that differ from `old`, each marked - for a line taken out and + for a line put
/// in, around the run of lines the two do not share at their heads and tails.
fn difference(old: &str, new: &str) -> Vec<String> {
    let (old, new): (Vec<&str>, Vec<&str>) = (old.lines().collect(), new.lines().collect());
    let head = old.iter().zip(&new).take_while(|(one, other)| one == other).count();
    let tail = old[head..].iter().rev().zip(new[head..].iter().rev()).take_while(|(one, other)| one == other).count();
    let mut lines = vec![format!("@@ line {}", head + 1)];
    lines.extend(old[head..old.len() - tail].iter().map(|line| format!("- {line}")));
    lines.extend(new[head..new.len() - tail].iter().map(|line| format!("+ {line}")));
    lines
}

/// What a place holds against what was last sent there and what goes now: the files changed there
/// since they were sent, and the files there the tree no longer holds, the files the deployment
/// leaves out among neither.
struct Standing {
    changed: Vec<String>,
    gone: Vec<String>,
    there: BTreeMap<String, String>,
}

fn standing(place: &Place, folder: &str, sent: &[Sent], ours: &BTreeMap<String, String>, last: &BTreeMap<String, String>, patterns: &[String]) -> Result<Standing, String> {
    let there = hashes_there(place, folder)?;
    let names: HashSet<&str> = sent.iter().map(|file| file.path.as_str()).collect();
    let changed = there.iter().filter(|(path, hash)| last.get(*path).is_some_and(|sent| sent != *hash) && ours.get(*path) != Some(*hash)).map(|(path, _)| path.clone()).collect();
    let gone = there.keys().filter(|path| !names.contains(path.as_str()) && left_out_by(patterns, path).is_none()).cloned().collect();
    Ok(Standing { changed, gone, there })
}

/// What a deployment does, as the job's `do` says.
#[derive(Clone, Copy, PartialEq)]
pub enum Act {
    Preview,
    Send,
    RollBack,
}

/// Runs the deployment `name` of the tree at `root`: previews it, sends it, or rolls it back. Files
/// changed at a place since they were last sent are kept there unless `replace`, and files there the
/// tree no longer holds are kept unless `delete`. Each step is said as it goes.
pub fn run(root: &Path, name: &str, act: Act, replace: bool, delete: bool, say: &(dyn Fn(String) + Sync)) -> Result<(), String> {
    let deployment = read(root)?.into_iter().find(|one| one.name == name).ok_or_else(|| format!("deploy.json names no deployment {name}"))?;
    let places: Vec<Place> = deployment.to.iter().map(|text| Place::parse(text, root)).collect::<Result<_, _>>()?;
    if places.is_empty() {
        return Err(format!("{name} names no place to send to"));
    }
    // A machine takes a release beside the one it runs; a folder on this machine takes the files in
    // place, as does every place of a deployment sent in place.
    let in_release = |place: &Place| !deployment.in_place && matches!(place, Place::Machine(_));
    if act == Act::RollBack {
        return roll_back(&places, &in_release, say);
    }
    let (sent, out) = gather(root, &deployment)?;
    let patterns: Vec<String> = ALWAYS_OUT.iter().map(|one| one.to_string()).chain(deployment.exclude.iter().cloned()).collect();
    say(format!("{} files of {} go to {} places", sent.len(), deployment.from, places.len()));
    for (path, rule) in &out {
        say(format!("left out: {path}, by {rule}"));
    }
    let ours: BTreeMap<String, String> = sent.iter().map(|file| (file.path.clone(), fs::read(&file.full).map(|bytes| sha256(&bytes)).unwrap_or_default())).collect();
    let mut kept_for: HashMap<String, Vec<String>> = HashMap::new();
    let mut standings: HashMap<String, Standing> = HashMap::new();
    for place in &places {
        let releases = in_release(place);
        let folder = if releases { format!("{}/current", place.folder()) } else { place.folder() };
        if act == Act::Preview {
            say(format!("{} takes {}", place.name(), if releases { "a new release, turned to once every place holds one" } else { "the files into its folder itself, and holds some of them where it fails part way" }));
            for file in &sent {
                let into = if releases { format!("{}/releases/<new>", place.folder()) } else { place.folder() };
                say(format!("{} goes to {into}/{}", file.path, file.path));
            }
        }
        let record = record_path(root, name, place);
        let last = read_record(record.as_ref());
        let standing = match standing(place, &folder, &sent, &ours, &last, &patterns) {
            Ok(standing) => standing,
            Err(error) if act == Act::Preview => {
                say(format!("{} is not reached: {error}", place.name()));
                continue;
            }
            Err(error) => return Err(format!("{} is not reached: {error}", place.name())),
        };
        for path in &standing.changed {
            let how = if replace { "replaced" } else { "kept" };
            say(format!("{path} changed at {} since it was last sent there, and is {how}:", place.name()));
            let theirs = text_there(place, &format!("{folder}/{path}"));
            let mine = sent.iter().find(|file| file.path == *path).and_then(|file| fs::read_to_string(&file.full).ok());
            if let (Some(theirs), Some(mine)) = (theirs, mine) {
                difference(&theirs, &mine).into_iter().for_each(|line| say(format!("  {line}")));
            }
        }
        for path in &standing.gone {
            let how = if !delete { "kept" } else if releases { "left out of the new release" } else { "deleted" };
            say(format!("{path} is at {} and no longer in the tree, and is {how}", place.name()));
        }
        if !replace {
            kept_for.insert(place.name(), standing.changed.clone());
        }
        standings.insert(place.name(), standing);
    }
    if act == Act::Preview {
        return Ok(());
    }
    let time = crate::git::head(root).map(|(_, when, _)| when.max(0) as u64).unwrap_or_else(|| SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_secs()).unwrap_or(0));
    let release = release_name(SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_secs()).unwrap_or(0));
    // Every place takes its files at once: a machine's release goes in beside the one it runs.
    let results: Vec<(usize, Result<(), String>)> = std::thread::scope(|scope| {
        let handles: Vec<_> = places
            .iter()
            .enumerate()
            .map(|(index, place)| {
                let kept = kept_for.get(&place.name()).cloned().unwrap_or_default();
                let keep_gone = if delete { Vec::new() } else { standings.get(&place.name()).map(|standing| standing.gone.clone()).unwrap_or_default() };
                let (release, sent, releases) = (&release, &sent, in_release(place));
                scope.spawn(move || (index, send_to(place, releases, release, sent, time, &kept, &keep_gone)))
            })
            .collect();
        handles.into_iter().map(|handle| handle.join().unwrap_or((usize::MAX, Err("a place's sending stopped".to_string())))).collect()
    });
    let failed: Vec<String> = results.iter().filter_map(|(index, result)| result.as_ref().err().map(|error| format!("{}: {error}", places.get(*index).map(Place::name).unwrap_or_default()))).collect();
    if !failed.is_empty() {
        for (index, result) in &results {
            match places.get(*index) {
                Some(place) if result.is_ok() && in_release(place) => {
                    let _ = place.ask(&format!("rm -rf {}", quote(&format!("{}/releases/{release}", place.folder()))), None);
                }
                _ => {}
            }
        }
        say(format!("no machine was turned to the new release, and each runs the release it had: {}", failed.join("; ")));
        return Err(format!("{} of {} places failed", failed.len(), places.len()));
    }
    for place in &places {
        let releases = in_release(place);
        if releases {
            place.ask(&switch_line(&place.folder(), &release), None).map_err(|error| format!("{}: {error}", place.name()))?;
            prune(place, deployment.keep, say);
            say(format!("{} now runs release {release}", place.name()));
        } else {
            if delete {
                if let Some(standing) = standings.get(&place.name()) {
                    remove_there(place, &place.folder(), &standing.gone)?;
                    standing.gone.iter().for_each(|path| say(format!("deleted {path} at {}", place.name())));
                }
            }
            say(format!("{} holds the files sent", place.name()));
        }
        let mut record: BTreeMap<String, String> = ours.clone();
        if let Some(standing) = standings.get(&place.name()) {
            for path in kept_for.get(&place.name()).into_iter().flatten() {
                if let Some(hash) = standing.there.get(path) {
                    record.insert(path.clone(), hash.clone());
                }
            }
        }
        write_record(record_path(root, name, place).as_ref(), &record);
    }
    Ok(())
}

/// The shell line that turns a machine's `current` to `release`: a link made beside it and moved
/// over it, which a reader of `current` sees as the old release or the new and never as neither.
fn switch_line(folder: &str, release: &str) -> String {
    let at = quote(folder);
    let to = quote(&format!("releases/{release}"));
    format!("cd {at} && ln -sfn {to} current.orior && {{ mv -fT current.orior current 2>/dev/null || {{ rm -f current && mv current.orior current; }}; }}")
}

/// Sends the files to a place, those it keeps left out: on a machine, a tar stream into a new release
/// beside the one it runs, the files it keeps and those the tree no longer holds that it keeps
/// copied into the release from the one it runs, or into its folder itself; to a folder here, each
/// file copied into it.
fn send_to(place: &Place, releases: bool, release: &str, sent: &[Sent], time: u64, kept: &[String], keep_gone: &[String]) -> Result<(), String> {
    let files: Vec<Sent> = sent.iter().filter(|file| !kept.contains(&file.path)).cloned().collect();
    match place {
        Place::Here(folder) => {
            for file in &files {
                let to = folder.join(&file.path);
                if let Some(parent) = to.parent() {
                    fs::create_dir_all(parent).map_err(|error| format!("{}: {error}", parent.display()))?;
                }
                fs::copy(&file.full, &to).map_err(|error| format!("{}: {error}", to.display()))?;
            }
            Ok(())
        }
        Place::Machine(_) => {
            let into = if releases { format!("{}/releases/{release}", place.folder()) } else { place.folder() };
            place.ask(&format!("mkdir -p {0} && tar -x -f - -C {0}", quote(&into)), Some(&tar(&files, time)?))?;
            let carried: Vec<&String> = kept.iter().chain(keep_gone).collect();
            if releases && !carried.is_empty() {
                let copies: Vec<String> = carried
                    .iter()
                    .map(|path| {
                        let to = format!("{into}/{path}");
                        let parent = to.rsplit_once('/').map(|(parent, _)| parent.to_string()).unwrap_or_else(|| into.clone());
                        format!("mkdir -p {} && cp -p {} {}", quote(&parent), quote(&format!("{}/current/{path}", place.folder())), quote(&to))
                    })
                    .collect();
                place.ask(&copies.join(" && "), None)?;
            }
            Ok(())
        }
    }
}

/// Takes away a place's releases past the newest `keep`, the one in use never among them.
fn prune(place: &Place, keep: usize, say: &(dyn Fn(String) + Sync)) {
    let Ok((names, current)) = releases_of(place) else { return };
    let old: Vec<&String> = names.iter().rev().skip(keep.max(1)).filter(|name| current.as_ref() != Some(*name)).collect();
    if old.is_empty() {
        return;
    }
    let words: Vec<String> = old.iter().map(|name| quote(name)).collect();
    if place.ask(&format!("cd {} && rm -rf -- {}", quote(&format!("{}/releases", place.folder())), words.join(" ")), None).is_ok() {
        say(format!("{} let go of {} old releases", place.name(), old.len()));
    }
}

/// A machine's releases, oldest first, and the one `current` names.
fn releases_of(place: &Place) -> Result<(Vec<String>, Option<String>), String> {
    let said = place.ask(&format!("cd {} 2>/dev/null || exit 0; ls -1 releases 2>/dev/null; echo '<current>'; readlink current 2>/dev/null; true", quote(&place.folder())), None)?;
    let (names, current) = said.split_once("<current>").unwrap_or((&said, ""));
    let mut names: Vec<String> = names.lines().map(str::trim).filter(|line| !line.is_empty()).map(str::to_string).collect();
    names.sort();
    let current = current.trim().strip_prefix("releases/").map(|name| name.trim_end_matches('/').to_string());
    Ok((names, current))
}

fn remove_there(place: &Place, folder: &str, paths: &[String]) -> Result<(), String> {
    if paths.is_empty() {
        return Ok(());
    }
    match place {
        Place::Here(top) => {
            for path in paths {
                fs::remove_file(top.join(path)).map_err(|error| format!("{path}: {error}"))?;
            }
            Ok(())
        }
        Place::Machine(_) => {
            let words: Vec<String> = paths.iter().map(|path| quote(path)).collect();
            place.ask(&format!("cd {} && rm -f -- {}", quote(folder), words.join(" ")), None).map(|_| ())
        }
    }
}

/// Turns each place's `current` to the release before the one it names, where every place has one.
fn roll_back(places: &[Place], in_release: &dyn Fn(&Place) -> bool, say: &(dyn Fn(String) + Sync)) -> Result<(), String> {
    let mut back = Vec::new();
    for place in places {
        if !in_release(place) {
            return Err(format!("{} takes the files in place and keeps no release before them to go back to: no place was rolled back", place.name()));
        }
        let (names, current) = releases_of(place).map_err(|error| format!("{}: {error}", place.name()))?;
        let at = current.as_ref().and_then(|current| names.iter().position(|name| name == current));
        match at {
            Some(index) if index > 0 => back.push((place, names[index - 1].clone())),
            _ => return Err(format!("{} holds no release before the one it runs: no place was rolled back", place.name())),
        }
    }
    for (place, release) in back {
        place.ask(&switch_line(&place.folder(), &release), None).map_err(|error| format!("{}: {error}", place.name()))?;
        say(format!("{} now runs release {release}", place.name()));
    }
    Ok(())
}

/// One job for each deployment the tree names, which previews it, sends it, or rolls it back.
pub fn jobs(root: &Path, jobs: &mut Vec<Job>) {
    let Ok(deployments) = read(root) else { return };
    let choice = |key: &str, choices: &[&str]| Param { key: key.to_string(), kind: "choice", choices: choices.iter().map(|one| one.to_string()).collect(), default: choices[0].to_string(), required: true };
    for deployment in deployments {
        let name = deployment.name.clone();
        let places = deployment.to.join(", ");
        jobs.push(Job {
            id: format!("deploy/{name}"),
            group: "deploy",
            title: name.clone(),
            file: "deploy.json".to_string(),
            about: format!("sends the files of {} to {places}, a new release at each turned to once every place holds it.", deployment.from),
            params: vec![choice("do", &["preview", "send", "roll back"]), choice("changed there", &["keep them", "replace them"]), choice("gone there", &["keep them", "delete them"])],
            opens: "",
            steps: vec![Step {
                program: Program::Orior,
                args: vec![Arg::Lit("deploy".into()), Arg::Lit(name), Arg::Value("do".into()), Arg::Value("changed there".into()), Arg::Value("gone there".into())],
            }],
        });
    }
}

/// The act a job's `do` names.
pub fn act_of(word: &str) -> Option<Act> {
    match word {
        "preview" => Some(Act::Preview),
        "send" => Some(Act::Send),
        "roll back" => Some(Act::RollBack),
        _ => None,
    }
}

#[cfg(test)]
mod deploying {
    use super::*;

    #[test]
    fn patterns_leave_out_names_paths_and_folders() {
        let patterns: Vec<String> = [".git/", "*.map", "drafts/", "secret/key.txt"].iter().map(|one| one.to_string()).collect();
        assert_eq!(left_out_by(&patterns, "app.js.map").map(String::as_str), Some("*.map"));
        assert_eq!(left_out_by(&patterns, "deep/app.js.map").map(String::as_str), Some("*.map"));
        assert_eq!(left_out_by(&patterns, "drafts/one.md").map(String::as_str), Some("drafts/"));
        assert_eq!(left_out_by(&patterns, "site/drafts/one.md").map(String::as_str), Some("drafts/"));
        assert_eq!(left_out_by(&patterns, "secret/key.txt").map(String::as_str), Some("secret/key.txt"));
        assert_eq!(left_out_by(&patterns, ".git/HEAD").map(String::as_str), Some(".git/"));
        assert_eq!(left_out_by(&patterns, "index.html"), None);
        assert_eq!(left_out_by(&patterns, "drafts.html"), None);
    }

    #[test]
    fn a_release_is_named_by_its_utc_time() {
        assert_eq!(release_name(0), "19700101-000000");
        assert_eq!(release_name(1_791_672_293), "20261010-224453");
        assert_eq!(civil(-1), (1969, 12, 31));
    }

    #[test]
    fn a_place_is_a_machine_or_a_folder_here() {
        let root = Path::new("C:/tree");
        assert!(matches!(Place::parse("me@web1:/srv/site", root), Ok(Place::Machine(_))));
        assert!(matches!(Place::parse("docker:web:/srv", root), Ok(Place::Machine(_))));
        assert!(matches!(Place::parse("C:/www/site", root), Ok(Place::Here(_))));
        assert!(matches!(Place::parse("./out", root), Ok(Place::Here(_))));
    }

    #[test]
    fn a_tar_stream_holds_each_file_with_its_mode_and_a_long_name_in_a_pax_header() {
        let dir = std::env::temp_dir().join(format!("orior_deploy_tar_{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        fs::write(dir.join("run.sh"), "#!/bin/sh\r\necho hi\r\n").unwrap();
        let long = format!("{}/x.txt", "folder".repeat(20));
        let files = vec![Sent { path: "run.sh".into(), full: dir.join("run.sh"), mode: 0o755 }, Sent { path: long.clone(), full: dir.join("run.sh"), mode: 0o644 }];
        let stream = tar(&files, 1_000).unwrap();
        assert_eq!(&stream[0..6], b"run.sh");
        assert_eq!(&stream[100..107], b"0000755");
        assert_eq!(&stream[512..532], b"#!/bin/sh\r\necho hi\r\n");
        assert_eq!(stream[1024 + 156], b'x');
        assert!(String::from_utf8_lossy(&stream[1536..2048]).contains(&format!("path={long}")));
        assert_eq!(stream.len() % 512, 0);
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_difference_shows_the_lines_taken_out_and_put_in() {
        assert_eq!(difference("a\nb\nc\n", "a\nB\nc\n"), vec!["@@ line 2", "- b", "+ B"]);
    }
}
