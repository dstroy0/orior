// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The reader's settings carried to another machine: settings.json, without its part for this
//! machine alone and the settings that name this machine's own (MACHINE); the themes; the reader's
//! plugins, and each plugin's own settings; the menus and toolbar, menus.json; and the stylesheet,
//! user.css. They go as one file, a JSON object of each file's text by its place in orior's own
//! folder, or into a folder of the reader's laid out as orior's own folder lays them, which, where it
//! is a repository, is committed and pushed, and pulled before it is read.
//!
//! Taking them in writes nothing until the reader has seen the differences and chosen what to take:
//! each file chosen, and in settings.json each setting chosen, the settings of this machine alone
//! kept as they are. Each file a taking writes over is kept as it was in `carried/`, a folder for each
//! taking, and undoing the taking writes them back and takes out the files it made.

use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

/// The files that travel, each a file or a folder of orior's own folder, by its place there.
const CARRIED: [&str; 6] = ["settings.json", "menus.json", "user.css", "themes/", "plugins/", "plugin_settings/"];

/// The settings that name this machine's own and stay on it, each a key or the start of keys that
/// ends in `.`: the part of settings.json for this machine alone, how programs are signed here, the
/// environments of the trees here, the memory the window may take here, and the file or folder the
/// settings are carried through.
const MACHINE: [&str; 5] = ["machine", "signing.", "environment.", "memory-budget", "carry.place"];

/// What a carried file says it is.
const MARK: &str = "orior settings";

/// The most a file of the reader's that travels holds.
const LARGEST: u64 = 16 << 20;

fn machine(key: &str) -> bool {
    MACHINE.iter().any(|name| key == *name || (name.ends_with('.') && key.starts_with(name)))
}

fn said(path: &Path) -> impl Fn(std::io::Error) -> String + '_ {
    move |error| format!("{}: {error}", path.display())
}

fn settings_of(text: &str) -> Map<String, Value> {
    match serde_json::from_str::<Value>(&crate::devcontainer::plain_json(text)) {
        Ok(Value::Object(map)) => map,
        _ => Map::new(),
    }
}

fn settings_text(map: &Map<String, Value>) -> String {
    let sorted: BTreeMap<&String, &Value> = map.iter().collect();
    serde_json::to_string_pretty(&sorted).unwrap_or_default() + "\n"
}

/// Each file of `folder` under it, by its place with `/` between folders, the place starting `under`.
fn files_under(folder: &Path, under: &str, into: &mut BTreeMap<String, PathBuf>) {
    let Ok(entries) = fs::read_dir(folder) else { return };
    for entry in entries.flatten() {
        let path = entry.path();
        let name = entry.file_name().to_string_lossy().into_owned();
        if name.starts_with('.') {
            continue;
        }
        if path.is_dir() {
            files_under(&path, &format!("{under}{name}/"), into);
        } else {
            into.insert(format!("{under}{name}"), path);
        }
    }
}

/// The files of `folder` that travel, by place.
fn carried_files(folder: &Path) -> BTreeMap<String, PathBuf> {
    let mut found = BTreeMap::new();
    for place in CARRIED {
        match place.strip_suffix('/') {
            Some(dir) => files_under(&folder.join(dir), place, &mut found),
            None if folder.join(place).is_file() => {
                found.insert(place.to_string(), folder.join(place));
            }
            None => {}
        }
    }
    found
}

/// What travels from orior's own folder `home`: each file's text by its place, settings.json without
/// the settings of this machine alone.
pub fn pack(home: &Path) -> Result<BTreeMap<String, String>, String> {
    let mut packed = BTreeMap::new();
    for (place, path) in carried_files(home) {
        if fs::metadata(&path).map_err(said(&path))?.len() > LARGEST {
            return Err(format!("{} is over {} MB, the most a file that travels holds", path.display(), LARGEST >> 20));
        }
        let Ok(text) = fs::read_to_string(&path) else { continue };
        if place == "settings.json" {
            let kept: Map<String, Value> = settings_of(&text).into_iter().filter(|(key, _)| !machine(key)).collect();
            packed.insert(place, settings_text(&kept));
        } else {
            packed.insert(place, text);
        }
    }
    Ok(packed)
}

/// Writes what travels from `home` to `to`: as one file where `to` is not a folder, or laid out in the
/// folder `to`, the files it no longer carries taken out of it, committed and pushed where it is a
/// repository. Says what it did.
pub fn send(home: &Path, to: &Path) -> Result<String, String> {
    let packed = pack(home)?;
    if !to.is_dir() {
        let file: Map<String, Value> = [("orior".to_string(), Value::String(MARK.into())), ("files".to_string(), serde_json::to_value(&packed).unwrap_or_default())].into_iter().collect();
        fs::write(to, serde_json::to_string_pretty(&file).unwrap_or_default() + "\n").map_err(said(to))?;
        return Ok(format!("{} files written to {}", packed.len(), to.display()));
    }
    let repository = to.join(".git").exists();
    if repository && crate::git::run(to, &["remote"]).is_ok_and(|remotes| !remotes.is_empty()) {
        crate::git::pull(to)?;
    }
    for (place, path) in carried_files(to) {
        if !packed.contains_key(&place) {
            fs::remove_file(&path).map_err(said(&path))?;
        }
    }
    for (place, text) in &packed {
        let path = to.join(place);
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(said(parent))?;
        }
        fs::write(&path, text).map_err(said(&path))?;
    }
    if !repository {
        return Ok(format!("{} files written to {}", packed.len(), to.display()));
    }
    crate::git::run(to, &["add", "-A", "--", "."])?;
    if crate::git::run(to, &["diff", "--cached", "--quiet"]).is_ok() {
        return Ok(format!("{} holds these settings already", to.display()));
    }
    let name = std::env::var(if cfg!(windows) { "COMPUTERNAME" } else { "HOSTNAME" }).unwrap_or_else(|_| "a machine".into());
    crate::git::run(to, &["commit", "-m", &format!("orior's settings from {name}")])?;
    if crate::git::run(to, &["remote"]).is_ok_and(|remotes| !remotes.is_empty()) {
        crate::git::push(to)?;
        return Ok(format!("{} files committed to {} and pushed", packed.len(), to.display()));
    }
    Ok(format!("{} files committed to {}", packed.len(), to.display()))
}

/// What the file or the folder `from` carries, by place, a repository with a remote pulled first.
pub fn read(from: &Path) -> Result<BTreeMap<String, String>, String> {
    if from.is_dir() {
        if from.join(".git").exists() && crate::git::run(from, &["remote"]).is_ok_and(|remotes| !remotes.is_empty()) {
            crate::git::pull(from)?;
        }
        let mut found = BTreeMap::new();
        for (place, path) in carried_files(from) {
            if let Ok(text) = fs::read_to_string(&path) {
                found.insert(place, text);
            }
        }
        return Ok(found);
    }
    let text = fs::read_to_string(from).map_err(said(from))?;
    let file: Value = serde_json::from_str(&text).map_err(|error| format!("{}: {error}", from.display()))?;
    if file.get("orior").and_then(Value::as_str) != Some(MARK) {
        return Err(format!("{} holds no settings orior carried", from.display()));
    }
    let files: BTreeMap<String, String> = serde_json::from_value(file.get("files").cloned().unwrap_or_default()).map_err(|error| format!("{}: {error}", from.display()))?;
    Ok(files.into_iter().filter(|(place, _)| allowed(place)).collect())
}

/// Whether `place` is one a carried file may write: under one of CARRIED, and nowhere above it.
fn allowed(place: &str) -> bool {
    !place.split('/').any(|part| part.is_empty() || part == "." || part == "..") && CARRIED.iter().any(|carried| place == *carried || (carried.ends_with('/') && place.starts_with(carried)))
}

/// A setting that differs between here and there, each side's value as JSON, or none where that side
/// does not set it.
#[derive(Serialize, Debug, PartialEq)]
pub struct Setting {
    pub key: String,
    pub here: Option<String>,
    pub there: Option<String>,
}

/// A file that differs between here and there: its place, whether it is only there, only here, or
/// changed, and for settings.json the settings that differ.
#[derive(Serialize, Debug)]
pub struct Difference {
    pub place: String,
    pub how: &'static str,
    pub settings: Vec<Setting>,
}

/// How what `there` carries differs from orior's own folder `home`; the files alike are left out.
pub fn differences(home: &Path, there: &BTreeMap<String, String>) -> Vec<Difference> {
    let here = pack(home).unwrap_or_default();
    let mut found = Vec::new();
    let places: BTreeSet<&String> = here.keys().chain(there.keys()).collect();
    for place in places {
        let (mine, theirs) = (here.get(place), there.get(place));
        if place == "settings.json" {
            let (mine, theirs) = (settings_of(mine.map_or("", String::as_str)), settings_of(theirs.map_or("", String::as_str)));
            let keys: BTreeSet<&String> = mine.keys().chain(theirs.keys()).filter(|key| !machine(key)).collect();
            let settings: Vec<Setting> = keys
                .into_iter()
                .filter(|key| mine.get(*key) != theirs.get(*key))
                .map(|key| Setting { key: key.clone(), here: mine.get(key).map(Value::to_string), there: theirs.get(key).map(Value::to_string) })
                .collect();
            if !settings.is_empty() {
                found.push(Difference { place: place.clone(), how: "changed", settings });
            }
            continue;
        }
        let how = match (mine, theirs) {
            (Some(mine), Some(theirs)) if mine == theirs => continue,
            (Some(_), Some(_)) => "changed",
            (None, Some(_)) => "there",
            (Some(_), None) => "here",
            (None, None) => continue,
        };
        found.push(Difference { place: place.clone(), how, settings: Vec::new() });
    }
    found
}

/// One taking: when, from where, and each file it wrote with whether one stood there before.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Taking {
    pub id: String,
    pub at: u64,
    pub from: String,
    pub files: Vec<(String, bool)>,
}

fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|since| since.as_millis() as u64).unwrap_or(0)
}

/// Takes in what `there`, read from `from`, carries: each file of `places`, and of settings.json the
/// settings of `keys` alone, as there, a setting there does not set taken out here, and the settings
/// of this machine alone kept. Each file it writes over is kept first in `carried/`.
pub fn take(home: &Path, from: &str, there: &BTreeMap<String, String>, places: &[String], keys: &[String]) -> Result<Taking, String> {
    let at = now();
    let id = format!("{at}");
    let kept = home.join("carried").join(&id);
    let mut taking = Taking { id, at, from: from.to_string(), files: Vec::new() };
    for place in places.iter().filter(|place| allowed(place)) {
        let path = home.join(place);
        let before = fs::read_to_string(&path).ok();
        let text = if place == "settings.json" {
            let mut mine = settings_of(before.as_deref().unwrap_or(""));
            let theirs = settings_of(there.get(place).map_or("", String::as_str));
            for key in keys.iter().filter(|key| !machine(key)) {
                match theirs.get(key) {
                    Some(value) => mine.insert(key.clone(), value.clone()),
                    None => mine.remove(key),
                };
            }
            settings_text(&mine)
        } else {
            match there.get(place) {
                Some(text) => text.clone(),
                None => continue,
            }
        };
        if let Some(before) = &before {
            let saved = kept.join(place);
            fs::create_dir_all(saved.parent().unwrap_or(&kept)).map_err(said(&kept))?;
            fs::write(&saved, before).map_err(said(&saved))?;
        }
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(said(parent))?;
        }
        fs::write(&path, text).map_err(said(&path))?;
        taking.files.push((place.clone(), before.is_some()));
    }
    fs::create_dir_all(&kept).map_err(said(&kept))?;
    fs::write(kept.join("taking.json"), serde_json::to_string_pretty(&taking).unwrap_or_default()).map_err(said(&kept))?;
    Ok(taking)
}

/// Every taking kept, the last first.
pub fn takings(home: &Path) -> Vec<Taking> {
    let mut found: Vec<Taking> = fs::read_dir(home.join("carried"))
        .into_iter()
        .flatten()
        .flatten()
        .filter_map(|entry| fs::read_to_string(entry.path().join("taking.json")).ok())
        .filter_map(|text| serde_json::from_str(&text).ok())
        .collect();
    found.sort_by(|a, b| b.at.cmp(&a.at));
    found
}

/// Undoes the taking `id`: each file it wrote over written back as it was, each it made taken out,
/// and the taking forgotten.
pub fn undo(home: &Path, id: &str) -> Result<Taking, String> {
    let taking = takings(home).into_iter().find(|taking| taking.id == id).ok_or_else(|| format!("no taking {id} is kept"))?;
    let kept = home.join("carried").join(&taking.id);
    for (place, stood) in &taking.files {
        let path = home.join(place);
        if *stood {
            let saved = kept.join(place);
            fs::write(&path, fs::read(&saved).map_err(said(&saved))?).map_err(said(&path))?;
        } else if path.is_file() {
            fs::remove_file(&path).map_err(said(&path))?;
        }
    }
    fs::remove_dir_all(&kept).map_err(said(&kept))?;
    Ok(taking)
}

#[cfg(test)]
mod carrying {
    use super::{differences, pack, read, send, take, takings, undo};
    use std::fs;

    #[test]
    fn settings_go_by_a_file_differ_before_they_are_taken_and_a_taking_is_undone() {
        let root = std::env::temp_dir().join(format!("orior_ui_carry_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let (one, two) = (root.join("one"), root.join("two"));
        fs::create_dir_all(one.join("themes")).unwrap();
        fs::create_dir_all(one.join("plugins/marks")).unwrap();
        fs::create_dir_all(one.join("plugin_settings")).unwrap();
        fs::create_dir_all(&two).unwrap();
        fs::write(one.join("settings.json"), "{\"zoom\": 1.25, \"vim\": true, \"memory-budget\": 900, \"machine\": {\"fonts\": {}}}").unwrap();
        fs::write(one.join("themes/Night.json"), "{\"name\": \"Night\"}").unwrap();
        fs::write(one.join("plugins/marks/plugin.json"), "{\"id\": \"marks\"}").unwrap();
        fs::write(one.join("plugin_settings/marks.json"), "{\"every\": 3}").unwrap();
        fs::write(one.join("state.json"), "{\"panes\": {}}").unwrap();
        fs::write(two.join("settings.json"), "{\"zoom\": 1.0, \"sticky\": false, \"memory-budget\": 400, \"machine\": {\"zoom\": 2}}").unwrap();
        let packed = pack(&one).unwrap();
        assert_eq!(packed.keys().map(String::as_str).collect::<Vec<_>>(), ["plugin_settings/marks.json", "plugins/marks/plugin.json", "settings.json", "themes/Night.json"]);
        assert!(!packed["settings.json"].contains("memory-budget") && !packed["settings.json"].contains("machine"), "{}", packed["settings.json"]);
        let file = root.join("carried.json");
        send(&one, &file).unwrap();
        let there = read(&file).unwrap();
        assert_eq!(there, packed);
        let found = differences(&two, &there);
        let settings = &found.iter().find(|one| one.place == "settings.json").unwrap().settings;
        assert_eq!(settings.iter().map(|one| one.key.as_str()).collect::<Vec<_>>(), ["sticky", "vim", "zoom"], "this machine's own differ not");
        assert_eq!(found.iter().find(|one| one.place == "themes/Night.json").unwrap().how, "there");
        let taking = take(&two, "carried.json", &there, &["settings.json".into(), "themes/Night.json".into(), "../outside.json".into()], &["zoom".into(), "sticky".into()]).unwrap();
        let now: serde_json::Value = serde_json::from_str(&fs::read_to_string(two.join("settings.json")).unwrap()).unwrap();
        assert_eq!(now["zoom"], 1.25);
        assert!(now.get("sticky").is_none(), "a setting there does not set is taken out where chosen");
        assert!(now.get("vim").is_none(), "one not chosen is not taken");
        assert_eq!(now["memory-budget"], 400, "this machine's own stays");
        assert_eq!(now["machine"]["zoom"], 2);
        assert!(two.join("themes/Night.json").is_file() && !root.join("outside.json").exists());
        assert_eq!(takings(&two).len(), 1);
        undo(&two, &taking.id).unwrap();
        assert!(fs::read_to_string(two.join("settings.json")).unwrap().contains("\"sticky\": false"));
        assert!(!two.join("themes/Night.json").exists(), "a file the taking made is taken out");
        assert!(takings(&two).is_empty());
        fs::write(root.join("other.json"), "{\"files\": {}}").unwrap();
        assert!(read(&root.join("other.json")).unwrap_err().contains("no settings orior carried"));
        fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn settings_go_by_a_folder_that_is_a_repository_and_are_committed() {
        let root = std::env::temp_dir().join(format!("orior_ui_carry_repo_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let (home, repo) = (root.join("home"), root.join("repo"));
        fs::create_dir_all(home.join("themes")).unwrap();
        fs::create_dir_all(&repo).unwrap();
        fs::write(home.join("settings.json"), "{\"zoom\": 1.1}").unwrap();
        fs::write(home.join("themes/Day.json"), "{\"name\": \"Day\"}").unwrap();
        let git = |args: &[&str]| crate::git::run(&repo, args).unwrap();
        git(&["init", "-q"]);
        git(&["config", "user.email", "orior@example.com"]);
        git(&["config", "user.name", "orior"]);
        fs::create_dir_all(repo.join("themes")).unwrap();
        fs::write(repo.join("themes/Gone.json"), "{}").unwrap();
        let said = send(&home, &repo).unwrap();
        assert!(said.contains("committed"), "{said}");
        assert!(repo.join("themes/Day.json").is_file() && !repo.join("themes/Gone.json").exists(), "the folder holds what travels and no more");
        assert_eq!(git(&["log", "--format=%s"]).lines().count(), 1);
        assert!(send(&home, &repo).unwrap().contains("already"));
        assert_eq!(read(&repo).unwrap()["themes/Day.json"], "{\"name\": \"Day\"}");
        fs::remove_dir_all(&root).ok();
    }
}
