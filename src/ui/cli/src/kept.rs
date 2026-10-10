// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What the window keeps between runs, as files in orior's own folder, laid out as the README's
//! "orior's own folder" says:
//!
//! - `settings.json`, the reader's settings, those SETTINGS names, each under its key;
//! - `themes/`, each theme of the reader's in a file of its own, named for the theme, the themes
//!   read back in the order of their files' names;
//! - `state.json`, what the window keeps of itself: its panes, its sessions, the files opened last,
//!   the places left in them, bookmarks, breakpoints and watches;
//! - `backups/`, the text of each tab with changes not saved, a file each.
//!
//! The page keeps each entry as text under a key starting `orior.`; here the key goes without it. An
//! entry whose text is JSON other than a string is written as that JSON, and any other as a string,
//! and each reads back as the text it was. A key the reader takes out of a file is gone at the next
//! start, and the window's default stands for it.

use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};

use serde_json::{Map, Value};

/// The settings among the window's entries: a key named here, or one starting with a name here that
/// ends in `.`.
const SETTINGS: [&str; 19] = [
    "scheme",
    "zoom",
    "opacity.",
    "fonts",
    "theme.",
    "crumbs",
    "column",
    "sticky",
    "brackets",
    "autosave",
    "trim",
    "final-newline",
    "format-on-save",
    "panes.auto",
    "patterns.",
    "search.sets",
    "plugins.off",
    "macros",
    "vim",
];

/// The entry that holds every theme of the reader's, and the start of each change not saved.
const THEMES: &str = "themes";
const BACKUP: &str = "backup.";

static KEEPING: std::sync::Mutex<()> = std::sync::Mutex::new(());

/// Where an entry is kept.
#[derive(Debug, PartialEq)]
enum Place {
    Settings,
    State,
    Themes,
    Backup,
}

fn place_of(key: &str) -> Place {
    if key == THEMES {
        Place::Themes
    } else if key.starts_with(BACKUP) {
        Place::Backup
    } else if SETTINGS.iter().any(|name| key == *name || (name.ends_with('.') && key.starts_with(name))) {
        Place::Settings
    } else {
        Place::State
    }
}

/// An entry's text as it is written: its JSON where it is JSON other than a string that writes back
/// the same, else the text itself.
fn written(text: &str) -> Value {
    match serde_json::from_str::<Value>(text) {
        Ok(value) if !value.is_string() && serde_json::to_string(&value).ok().as_deref() == Some(text) => value,
        _ => Value::String(text.to_string()),
    }
}

/// An entry as the page keeps it, from what was written.
fn read_back(value: &Value) -> String {
    match value {
        Value::String(text) => text.clone(),
        other => serde_json::to_string(other).unwrap_or_default(),
    }
}

fn read_map(path: &Path) -> Map<String, Value> {
    fs::read_to_string(path).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

/// Writes beside the file and moves it over, which leaves the file whole where the write fails.
fn write_whole(path: &Path, text: &str) -> Result<(), String> {
    let said = |error: std::io::Error| format!("{}: {error}", path.display());
    if let Some(folder) = path.parent() {
        fs::create_dir_all(folder).map_err(said)?;
    }
    let beside = path.with_extension("saving");
    fs::write(&beside, text).map_err(said)?;
    fs::rename(&beside, path).map_err(said)
}

fn write_map(path: &Path, map: &Map<String, Value>) -> Result<(), String> {
    let sorted: BTreeMap<&String, &Value> = map.iter().collect();
    write_whole(path, &(serde_json::to_string_pretty(&sorted).map_err(|error| error.to_string())? + "\n"))
}

/// A theme's file name: its name with each letter a file name cannot hold, or should not, made `_`.
fn theme_file(name: &str) -> String {
    let safe: String = name.chars().map(|letter| if letter.is_alphanumeric() || " -_.".contains(letter) { letter } else { '_' }).collect();
    let safe = safe.trim_matches(|letter| letter == '.' || letter == ' ');
    format!("{}.json", if safe.is_empty() { "theme" } else { safe })
}

/// A change not saved's file name: a hash of its key, the same for the same key.
fn backup_file(key: &str) -> String {
    let hash = key.bytes().fold(0xcbf2_9ce4_8422_2325u64, |hash, byte| (hash ^ u64::from(byte)).wrapping_mul(0x0100_0000_01b3));
    format!("{hash:016x}.json")
}

fn json_files(folder: &Path) -> Vec<PathBuf> {
    let mut found: Vec<PathBuf> = fs::read_dir(folder).into_iter().flatten().flatten().map(|entry| entry.path()).filter(|path| path.extension().is_some_and(|end| end == "json")).collect();
    found.sort();
    found
}

/// Every entry kept in `folder`, by its key without `orior.`, or None where the window has kept
/// nothing there yet, which is while there is no `state.json`.
pub fn read(folder: &Path) -> Option<BTreeMap<String, String>> {
    if !folder.join("state.json").is_file() {
        return None;
    }
    let mut entries = BTreeMap::new();
    for file in ["settings.json", "state.json"] {
        for (key, value) in read_map(&folder.join(file)) {
            entries.insert(key, read_back(&value));
        }
    }
    let themes: Vec<Value> = json_files(&folder.join("themes")).iter().filter_map(|path| fs::read_to_string(path).ok()).filter_map(|text| serde_json::from_str(&text).ok()).collect();
    if !themes.is_empty() {
        entries.insert(THEMES.to_string(), serde_json::to_string(&themes).unwrap_or_default());
    }
    for path in json_files(&folder.join("backups")) {
        let backup = read_map(&path);
        if let (Some(Value::String(key)), Some(Value::String(text))) = (backup.get("key"), backup.get("text")) {
            entries.insert(key.clone(), text.clone());
        }
    }
    Some(entries)
}

/// Keeps the changes in `folder`: each key without `orior.`, with its new text, or None where it is
/// gone. `state.json` is made where it is not there yet.
pub fn keep(folder: &Path, changes: &BTreeMap<String, Option<String>>) -> Result<(), String> {
    // One keeping at a time, each reading the files the one before wrote.
    let _held = KEEPING.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    let (mut settings, mut state) = (None, None);
    for (key, text) in changes {
        match place_of(key) {
            Place::Settings | Place::State => {
                let (held, file) = if place_of(key) == Place::Settings { (&mut settings, "settings.json") } else { (&mut state, "state.json") };
                let map = held.get_or_insert_with(|| read_map(&folder.join(file)));
                match text {
                    Some(text) => map.insert(key.clone(), written(text)),
                    None => map.remove(key),
                };
            }
            Place::Themes => keep_themes(&folder.join("themes"), text.as_deref())?,
            Place::Backup => {
                let path = folder.join("backups").join(backup_file(key));
                match text {
                    Some(text) => {
                        let backup: Map<String, Value> = [("key".to_string(), Value::String(key.clone())), ("text".to_string(), Value::String(text.clone()))].into_iter().collect();
                        write_map(&path, &backup)?;
                    }
                    None => {
                        let _ = fs::remove_file(&path);
                    }
                }
            }
        }
    }
    if let Some(settings) = settings {
        write_map(&folder.join("settings.json"), &settings)?;
    }
    match state {
        Some(state) => write_map(&folder.join("state.json"), &state)?,
        None if !folder.join("state.json").is_file() => write_map(&folder.join("state.json"), &Map::new())?,
        None => {}
    }
    Ok(())
}

/// Writes each theme in the list to a file of its own and takes out the files of themes no longer in
/// it.
fn keep_themes(folder: &Path, text: Option<&str>) -> Result<(), String> {
    let themes: Vec<Value> = text.and_then(|text| serde_json::from_str(text).ok()).unwrap_or_default();
    let mut names = std::collections::BTreeSet::new();
    for theme in &themes {
        let base = theme_file(theme.get("name").and_then(Value::as_str).unwrap_or("theme"));
        let mut name = base.clone();
        let mut count = 2;
        while names.contains(&name) {
            name = format!("{} {count}.json", base.trim_end_matches(".json"));
            count += 1;
        }
        write_whole(&folder.join(&name), &(serde_json::to_string_pretty(theme).map_err(|error| error.to_string())? + "\n"))?;
        names.insert(name);
    }
    for path in json_files(folder) {
        if !path.file_name().is_some_and(|name| names.contains(&*name.to_string_lossy())) {
            let _ = fs::remove_file(path);
        }
    }
    Ok(())
}

#[cfg(test)]
mod keeping {
    use super::{keep, read};
    use std::collections::BTreeMap;

    #[test]
    fn what_the_window_keeps_reads_back_as_it_was() {
        let folder = std::env::temp_dir().join(format!("orior_ui_kept_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&folder);
        std::fs::create_dir_all(&folder).unwrap();
        std::fs::write(folder.join("settings.json"), "{\"auto_report\": true}").unwrap();
        assert_eq!(read(&folder), None);
        let entries: BTreeMap<String, Option<String>> = [
            ("scheme", "dark"),
            ("zoom", "1.1"),
            ("fonts", "{\"--code\":\"Fira Code\"}"),
            ("theme.dark", "Night"),
            ("patterns.explorer", "target\n*.log"),
            ("panes", "{\"folder\":true}"),
            ("session.D:/tree", "[\"a.rs\"]"),
            ("search.set", "\"quoted\""),
            ("themes", "[{\"name\":\"A/B\",\"base\":\"light\",\"colors\":{}},{\"name\":\"Night\",\"base\":\"dark\",\"colors\":{\"--bg\":\"#000\"}}]"),
            ("backup.D:/tree\nsrc/a.rs", "unsaved text"),
        ]
        .into_iter()
        .map(|(key, text)| (key.to_string(), Some(text.to_string())))
        .collect();
        keep(&folder, &entries).unwrap();
        let read_back = read(&folder).unwrap();
        for (key, text) in &entries {
            assert_eq!(read_back.get(key), text.as_ref(), "{key}");
        }
        assert_eq!(read_back.get("auto_report").map(String::as_str), Some("true"));
        let settings = std::fs::read_to_string(folder.join("settings.json")).unwrap();
        assert!(settings.contains("\"zoom\": 1.1") && settings.contains("\"theme.dark\": \"Night\"") && !settings.contains("panes"), "{settings}");
        assert!(folder.join("themes/Night.json").is_file() && folder.join("themes/A_B.json").is_file());
        let gone: BTreeMap<String, Option<String>> = [("zoom".to_string(), None), ("themes".to_string(), Some("[{\"name\":\"Night\",\"base\":\"dark\",\"colors\":{}}]".to_string())), ("backup.D:/tree\nsrc/a.rs".to_string(), None)].into_iter().collect();
        keep(&folder, &gone).unwrap();
        let read_back = read(&folder).unwrap();
        assert!(!read_back.contains_key("zoom") && !read_back.keys().any(|key| key.starts_with("backup.")));
        assert!(!folder.join("themes/A_B.json").exists());
        std::fs::remove_dir_all(&folder).unwrap();
    }
}
