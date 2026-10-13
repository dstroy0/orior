// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Settings by file, each part of a file standing over the parts before it: the files the reader's
//! settings.json takes, settings.json itself, the files a tree's `.orior/settings.json` takes, and
//! that file. A settings file is JSON, with comments and trailing commas as VS Code's files hold
//! them, each key a setting as settings.json keeps it (kept.rs), and these keys of its own:
//!
//! - `takes`: the path of another settings file, from the file's own folder, whose settings this one
//!   takes, its own standing over them;
//! - `windows`, `linux` and `macos`: settings for that system alone, standing over the file's others;
//! - `machine`, in the reader's own settings.json: settings for this machine alone, standing over the
//!   file's others and its system's, and never carried to another machine (carry.rs);
//! - `[kind]`, as `[markdown]`: the theme of each scheme, `theme.dark` and `theme.light`, the editor
//!   shows the files of the language `kind` in.
//!
//! Text in a value names another setting's value with `${setting:name}`, a variable of the system's
//! environment with `${env:NAME}`, the reader's home folder with `${home}`, the tree's folder with
//! `${tree}`, and the system with `${system}`. A tree's file does not set the settings of TREE_HELD.

use std::collections::{BTreeMap, BTreeSet};
use std::path::{Component, Path, PathBuf};

use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

use crate::kept::read_back;

/// The tree's settings file, from its top folder.
pub const TREE_FILE: &str = ".orior/settings.json";

const TAKES: &str = "takes";
const MACHINE: &str = "machine";
const SYSTEMS: [&str; 3] = ["windows", "linux", "macos"];

/// The settings a tree's file does not set, each a key or the start of keys where it ends in `.`:
/// how the programs a build makes are signed, the environment a tree's runs take, the checkers run
/// as files change, the window's title bar, whether orior is held off the network, what each
/// plugin may read, write and reach, and the cores kept for the reader. Each runs a program, names the
/// reader's own, lets orior or a plugin reach out or takes the reader's machine, and a tree cloned from
/// anywhere would set it.
const TREE_HELD: [&str; 7] = ["signing.", "environment.", "checkers", "titlebar", "network", "plugins.allowed", "cores-kept"];

/// The settings a kind of file sets.
const KIND_KEYS: [&str; 2] = ["theme.dark", "theme.light"];

/// The most files one chain of `takes` reads, and the most settings one `${setting:name}` reads
/// through.
const DEEPEST: usize = 16;
const NAMES_DEEP: usize = 8;

/// A settings file as read: the path it is shown by and every key it holds.
#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct File {
    pub file: String,
    pub values: Map<String, Value>,
}

/// The files of one chain, the first taken by the second and so on to the last, and what could not
/// be read.
#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct Chain {
    pub files: Vec<File>,
    pub problems: Vec<String>,
}

/// A part of a file that sets a setting, the system the part is for where it is one, and the text it
/// sets the setting to.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Source {
    pub file: String,
    pub part: Option<String>,
    pub text: String,
}

/// The settings in force, as the page reads them over its own entries, each by its key without
/// `orior.`:
///
/// - `over`, the text of each a file stands over the reader's own settings.json with, and of each
///   settings.json sets through a variable, those in `own`;
/// - `under`, the text of each only the files settings.json takes set;
/// - `from`, each part of a file that sets a setting, the one in force first, a kind's settings
///   under `[kind] key`;
/// - `kinds`, the settings of each kind of file;
/// - `own_file`, the reader's own settings.json as `from` names it, where it is there.
#[derive(Serialize, Debug, Default)]
pub struct Layers {
    pub own_file: Option<String>,
    pub over: BTreeMap<String, String>,
    pub under: BTreeMap<String, String>,
    pub own: Vec<String>,
    pub from: BTreeMap<String, Vec<Source>>,
    pub kinds: BTreeMap<String, BTreeMap<String, String>>,
    pub files: Vec<String>,
    pub problems: Vec<String>,
    pub system: String,
}

/// The system orior runs on, as a settings file names it.
pub fn system() -> &'static str {
    if cfg!(windows) {
        "windows"
    } else if cfg!(target_os = "macos") {
        "macos"
    } else {
        "linux"
    }
}

/// Whether `key` is one of a settings file's own keys and no setting: `takes`, a system's, or a
/// kind's.
pub fn is_file_key(key: &str) -> bool {
    key == TAKES || key == MACHINE || SYSTEMS.contains(&key) || kind_of(key).is_some()
}

fn kind_of(key: &str) -> Option<&str> {
    key.strip_prefix('[').and_then(|rest| rest.strip_suffix(']')).filter(|kind| !kind.is_empty())
}

fn held(key: &str) -> bool {
    TREE_HELD.iter().any(|name| key == *name || (name.ends_with('.') && key.starts_with(name)))
}

/// `path` with each `.` taken out and each `..` taking out the folder before it.
fn tidy(path: &Path) -> PathBuf {
    let mut out = PathBuf::new();
    for part in path.components() {
        match part {
            Component::CurDir => {}
            Component::ParentDir if matches!(out.components().next_back(), Some(Component::Normal(_))) => {
                out.pop();
            }
            other => out.push(other),
        }
    }
    out
}

fn home() -> String {
    std::env::var(if cfg!(windows) { "USERPROFILE" } else { "HOME" }).unwrap_or_default()
}

/// `text` with each `${name}` that `look` reads put in its place; one it does not read stays as it is.
fn expand(text: &str, look: &mut dyn FnMut(&str) -> Option<String>) -> String {
    let mut out = String::with_capacity(text.len());
    let mut rest = text;
    while let Some(open) = rest.find("${") {
        out.push_str(&rest[..open]);
        let Some(close) = rest[open..].find('}') else {
            out.push_str(&rest[open..]);
            return out;
        };
        match look(&rest[open + 2..open + close]) {
            Some(value) => out.push_str(&value),
            None => out.push_str(&rest[open..=open + close]),
        }
        rest = &rest[open + close + 1..];
    }
    out.push_str(rest);
    out
}

/// The file at `path` and each it takes, the one taken first, each shown as `shown` names it. A file
/// not there gives none; a file taken that is not there, or that a chain takes twice, is a problem.
/// The path a file takes may name the reader's home folder and the system's variables.
pub fn chain(path: &Path, shown: &dyn Fn(&Path) -> String) -> Chain {
    let mut chain = Chain::default();
    let mut seen = BTreeSet::new();
    let mut next = Some(tidy(path));
    let mut first = true;
    while let Some(path) = next.take() {
        let named = shown(&path);
        if chain.files.len() >= DEEPEST {
            chain.problems.push(format!("{named}: more than {DEEPEST} files take one another, and those past them are not read"));
            break;
        }
        if !seen.insert(std::fs::canonicalize(&path).unwrap_or_else(|_| path.clone())) {
            chain.problems.push(format!("{named} is taken twice in one chain, and is read once"));
            break;
        }
        let text = match std::fs::read_to_string(&path) {
            Ok(text) => text,
            Err(error) => {
                if !first || error.kind() != std::io::ErrorKind::NotFound {
                    chain.problems.push(format!("{named}: {error}"));
                }
                break;
            }
        };
        first = false;
        let values = match serde_json::from_str::<Value>(&crate::devcontainer::plain_json(&text)) {
            Ok(Value::Object(values)) => values,
            Ok(_) => {
                chain.problems.push(format!("{named} is not an object of settings"));
                break;
            }
            Err(error) => {
                chain.problems.push(format!("{named}: {error}"));
                break;
            }
        };
        next = match values.get(TAKES) {
            Some(Value::String(taken)) => {
                let taken = expand(taken, &mut |name| match name {
                    "home" => Some(home()).filter(|home| !home.is_empty()),
                    _ => name.strip_prefix("env:").and_then(|variable| std::env::var(variable).ok()),
                });
                Some(tidy(&path.parent().map(|folder| folder.join(&taken)).unwrap_or_else(|| PathBuf::from(&taken))))
            }
            Some(_) => {
                chain.problems.push(format!("{named}: `takes` is the path of a file"));
                None
            }
            None => None,
        };
        chain.files.insert(0, File { file: named, values });
    }
    chain
}

/// The tree's settings: its file, `.orior/settings.json`, and each it takes, shown from the tree's
/// top folder where they are in it.
pub fn tree(root: &Path) -> Chain {
    let root = tidy(root);
    chain(&root.join(TREE_FILE), &|path| match path.strip_prefix(&root) {
        Ok(inside) => inside.to_string_lossy().replace('\\', "/"),
        Err(_) => path.display().to_string(),
    })
}

/// The reader's settings: settings.json in orior's own folder, and each file it takes.
pub fn reader() -> Chain {
    match crate::home::folder() {
        Some(folder) => chain(&folder.join("settings.json"), &|path| path.display().to_string()),
        None => Chain::default(),
    }
}

/// Where a part stands: under the reader's own settings.json, as settings.json itself, or over it.
#[derive(Clone, Copy, PartialEq)]
enum Rank {
    Under,
    Own,
    Over,
}

struct Part<'a> {
    file: &'a str,
    part: Option<&'static str>,
    values: Vec<(&'a String, &'a Value)>,
    rank: Rank,
}

/// What `${name}` reads: the settings in force by key, the tree's folder, the reader's home folder and
/// the system.
struct Names<'a> {
    raw: BTreeMap<&'a str, &'a Value>,
    root: &'a str,
    home: String,
    system: &'static str,
}

impl Names<'_> {
    fn value(&self, value: &Value, depth: usize, missing: &mut Vec<String>) -> Value {
        match value {
            Value::String(text) => Value::String(expand(text, &mut |name| self.named(name, depth, missing))),
            Value::Array(items) => Value::Array(items.iter().map(|item| self.value(item, depth, missing)).collect()),
            Value::Object(set) => Value::Object(set.iter().map(|(key, item)| (key.clone(), self.value(item, depth, missing))).collect()),
            other => other.clone(),
        }
    }

    fn named(&self, name: &str, depth: usize, missing: &mut Vec<String>) -> Option<String> {
        let found = match name {
            "home" => Some(self.home.clone()).filter(|home| !home.is_empty()),
            "tree" => Some(self.root.to_string()).filter(|root| !root.is_empty()),
            "system" => Some(self.system.to_string()),
            _ => match (name.strip_prefix("env:"), name.strip_prefix("setting:")) {
                (Some(variable), _) => std::env::var(variable).ok(),
                (_, Some(setting)) if depth < NAMES_DEEP => self.raw.get(setting).map(|value| read_back(&self.value(value, depth + 1, missing))),
                _ => None,
            },
        };
        if found.is_none() {
            missing.push(name.to_string());
        }
        found
    }
}

/// Says each name of `missing` that the setting `key` of `file` names and nothing gives.
fn said(file: &str, key: &str, missing: Vec<String>, problems: &mut Vec<String>) {
    for name in missing {
        problems.push(format!("{file}: {key} names ${{{name}}}, which nothing gives"));
    }
}

/// The settings in force from the reader's chain and the tree's, for the tree at `root`.
pub fn layers(reader: &Chain, tree: &Chain, root: &str) -> Layers {
    let system = system();
    let mut layers = Layers { system: system.to_string(), own_file: reader.files.last().map(|file| file.file.clone()), ..Layers::default() };
    let mut problems: Vec<String> = reader.problems.iter().chain(&tree.problems).cloned().collect();
    let mut parts: Vec<Part> = Vec::new();
    let mut kinds: Vec<(&str, &str, &Map<String, Value>)> = Vec::new();
    let last = reader.files.len().saturating_sub(1);
    let files = reader.files.iter().enumerate().map(|(at, file)| (file, false, at == last)).chain(tree.files.iter().map(|file| (file, true, false)));
    for (file, of_tree, own) in files {
        layers.files.push(file.file.clone());
        let mut top = Vec::new();
        for (key, value) in &file.values {
            if let Some(kind) = kind_of(key) {
                match value {
                    Value::Object(set) => kinds.push((file.file.as_str(), kind, set)),
                    _ => problems.push(format!("{}: {key} is an object of settings", file.file)),
                }
            } else if of_tree && held(key) {
                problems.push(format!("{}: {key} is the reader's own to set, and a tree's file does not set it", file.file));
            } else if !is_file_key(key) {
                top.push((key, value));
            }
        }
        let rank = if of_tree { Rank::Over } else if own { Rank::Own } else { Rank::Under };
        parts.push(Part { file: &file.file, part: None, values: top, rank });
        match file.values.get(system) {
            Some(Value::Object(set)) => {
                let mut values = Vec::new();
                for (key, value) in set {
                    if is_file_key(key) {
                        problems.push(format!("{}: {system} sets {key}, which a file sets outside a system's part", file.file));
                    } else if of_tree && held(key) {
                        problems.push(format!("{}: {key} is the reader's own to set, and a tree's file does not set it", file.file));
                    } else {
                        values.push((key, value));
                    }
                }
                parts.push(Part { file: &file.file, part: Some(system), values, rank: if rank == Rank::Under { Rank::Under } else { Rank::Over } });
            }
            Some(_) => problems.push(format!("{}: {system} is an object of settings", file.file)),
            None => {}
        }
        if let Some(Value::Object(set)) = file.values.get(MACHINE).filter(|_| own) {
            let values = set.iter().filter(|(key, _)| !is_file_key(key)).collect();
            parts.push(Part { file: &file.file, part: Some(MACHINE), values, rank: Rank::Over });
        }
    }
    let mut raw = BTreeMap::new();
    for part in &parts {
        for (key, value) in &part.values {
            raw.insert(key.as_str(), *value);
        }
    }
    let names = Names { raw, root, home: home(), system };
    for part in parts.iter().rev() {
        for (key, value) in &part.values {
            let mut missing = Vec::new();
            let text = read_back(&names.value(value, 0, &mut missing));
            said(part.file, key, missing, &mut problems);
            let sources = layers.from.entry(key.to_string()).or_default();
            if sources.is_empty() {
                match part.rank {
                    Rank::Over => {
                        layers.over.insert(key.to_string(), text.clone());
                    }
                    Rank::Own if read_back(value).contains("${") => {
                        layers.over.insert(key.to_string(), text.clone());
                        layers.own.push(key.to_string());
                    }
                    Rank::Own => {}
                    Rank::Under => {
                        layers.under.insert(key.to_string(), text.clone());
                    }
                }
            }
            sources.push(Source { file: part.file.to_string(), part: part.part.map(String::from), text });
        }
    }
    for (file, kind, set) in kinds.iter().rev() {
        for (key, value) in set.iter() {
            if !KIND_KEYS.contains(&key.as_str()) {
                problems.push(format!("{file}: [{kind}] sets {key}, and a kind of file sets its theme alone: {}", KIND_KEYS.join(" and ")));
                continue;
            }
            let mut missing = Vec::new();
            let text = read_back(&names.value(value, 0, &mut missing));
            said(file, key, missing, &mut problems);
            layers.kinds.entry(kind.to_string()).or_default().entry(key.clone()).or_insert_with(|| text.clone());
            layers.from.entry(format!("[{kind}] {key}")).or_default().push(Source { file: file.to_string(), part: None, text });
        }
    }
    let mut seen = BTreeSet::new();
    layers.problems = problems.into_iter().filter(|problem| seen.insert(problem.clone())).collect();
    layers
}

#[cfg(test)]
mod reading {
    use super::{chain, layers, system, tree, Chain, Source, TREE_FILE};
    use std::fs;

    #[test]
    fn a_file_takes_another_and_a_chain_that_takes_a_file_twice_stops() {
        let root = std::env::temp_dir().join(format!("orior_ui_settings_chain_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("tree/.orior")).unwrap();
        fs::write(root.join("tree").join(TREE_FILE), "{\n  // the shared settings, changed\n  \"takes\": \"../../shared.json\",\n  \"zoom\": 1.25,\n}\n").unwrap();
        fs::write(root.join("shared.json"), "{ \"takes\": \"base.json\", \"zoom\": 1.1, \"vim\": true }").unwrap();
        fs::write(root.join("base.json"), "{ \"sticky\": false }").unwrap();
        let read = tree(&root.join("tree"));
        assert!(read.problems.is_empty(), "{:?}", read.problems);
        let named: Vec<&str> = read.files.iter().map(|file| file.file.as_str()).collect();
        assert_eq!(named.len(), 3);
        assert!(named[0].ends_with("base.json") && named[1].ends_with("shared.json") && named[2] == TREE_FILE, "{named:?}");
        fs::write(root.join("base.json"), "{ \"takes\": \"shared.json\" }").unwrap();
        let read = tree(&root.join("tree"));
        assert_eq!(read.files.len(), 3);
        assert!(read.problems.iter().any(|problem| problem.contains("taken twice")), "{:?}", read.problems);
        fs::write(root.join("base.json"), "{ \"takes\": \"gone.json\" }").unwrap();
        assert!(tree(&root.join("tree")).problems.iter().any(|problem| problem.contains("gone.json")));
        assert!(chain(&root.join("none.json"), &|path| path.display().to_string()).problems.is_empty(), "a file not there is no problem");
        fs::remove_dir_all(&root).unwrap();
    }

    fn file(name: &str, text: &str) -> super::File {
        let values = match serde_json::from_str(text).unwrap() {
            serde_json::Value::Object(values) => values,
            _ => unreachable!(),
        };
        super::File { file: name.to_string(), values }
    }

    #[test]
    fn a_tree_stands_over_the_reader_a_system_over_its_file_and_values_name_others() {
        let other = if system() == "windows" { "linux" } else { "windows" };
        let reader = Chain {
            files: vec![
                file("base.json", "{\"vim\": \"true\", \"margin\": \"true\"}"),
                file("settings.json", &format!("{{\"takes\": \"base.json\", \"zoom\": 1.1, \"margin\": \"false\", \"{}\": {{\"sticky\": \"false\"}}, \"{other}\": {{\"zoom\": 3}}, \"fonts\": {{\"--code\": \"${{setting:code}}\"}}, \"code\": \"Fira Code\", \"machine\": {{\"brackets\": \"false\"}}}}", system())),
            ],
            problems: Vec::new(),
        };
        let tree = Chain {
            files: vec![file(TREE_FILE, "{\"zoom\": 1.25, \"signing.windows\": \"mine\", \"crumbs\": \"${system}\", \"path\": \"${env:PATH}\", \"lost\": \"${nothing}\", \"[markdown]\": {\"theme.dark\": \"Paper\", \"vim\": true}}")],
            problems: Vec::new(),
        };
        let read = layers(&reader, &tree, "D:/tree");
        assert_eq!(read.over.get("zoom").map(String::as_str), Some("1.25"), "the tree's stands over the reader's");
        assert_eq!(read.from["zoom"], vec![Source { file: TREE_FILE.into(), part: None, text: "1.25".into() }, Source { file: "settings.json".into(), part: None, text: "1.1".into() }]);
        assert_eq!(read.over.get("sticky").map(String::as_str), Some("false"), "the system's part of the reader's own stands over its others");
        assert_eq!(read.from["sticky"][0].part.as_deref(), Some(system()));
        assert!(!read.over.contains_key("margin") && !read.under.contains_key("margin"), "the reader's own settings.json is the page's own");
        assert_eq!(read.under.get("vim").map(String::as_str), Some("true"), "a file the reader's takes stands under it");
        assert_eq!(read.over.get("fonts").map(String::as_str), Some("{\"--code\":\"Fira Code\"}"), "a value names another's");
        assert!(read.own.contains(&"fonts".to_string()));
        assert_eq!(read.over.get("crumbs").map(String::as_str), Some(system()));
        assert_eq!(read.over.get("path").cloned(), std::env::var("PATH").ok());
        assert!(!read.over.contains_key("signing.windows") && read.problems.iter().any(|problem| problem.contains("signing.windows")), "{:?}", read.problems);
        assert!(read.problems.iter().any(|problem| problem.contains("${nothing}")), "{:?}", read.problems);
        assert_eq!(read.kinds["markdown"]["theme.dark"], "Paper");
        assert!(!read.kinds["markdown"].contains_key("vim") && read.problems.iter().any(|problem| problem.contains("[markdown] sets vim")));
        assert_eq!(read.from["[markdown] theme.dark"][0].file, TREE_FILE);
        assert_eq!(read.files, vec!["base.json", "settings.json", TREE_FILE]);
        assert_eq!(read.own_file.as_deref(), Some("settings.json"));
        assert_eq!(read.over.get("brackets").map(String::as_str), Some("false"), "the part for this machine alone stands over the rest");
        assert_eq!(read.from["brackets"][0].part.as_deref(), Some("machine"));
    }
}
