// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The bridge between languages as the editor reads it: Lstar.klq, each language's .klm, and every
//! ruleset, all in src/cu/transpiler/lstar/protocol/table.
//!
//! A language's name goes through its .klm to a key of the bridge, the key's pairs and their
//! verdicts are in Lstar.klq, and each ruleset's entry of the key's name is the form it writes.
//! The forms of each line are in src/cu/types/file_defs/klq and klm.
//!
//! Only the line shapes are held to: `key`, `pair`, the verdict under a pair (`open` or `closed`),
//! and a .klm's `key`, `kind` and `breaks`. Every other line under a pair is kept as a note, its
//! first word and the rest, and a line the query adds later shows without a change here. A .klm's
//! name and its key are read as two words even where they are equal.

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use serde::Serialize;

use crate::root::relative;

pub const DIR: &str = "src/cu/transpiler/lstar/protocol/table";

#[derive(Serialize)]
pub struct Note {
    pub word: String,
    pub rest: String,
    pub line: usize,
}

#[derive(Serialize)]
pub struct Pair {
    pub first: String,
    pub second: String,
    pub line: usize,
    /// The verdict line: `open <count>` or `closed <address> <case>-><answer>`.
    pub verdict: Option<Note>,
    pub notes: Vec<Note>,
}

#[derive(Serialize, Default)]
pub struct Key {
    pub line: usize,
    pub pairs: Vec<Pair>,
}

#[derive(Serialize)]
pub struct Klq {
    pub path: String,
    pub keys: BTreeMap<String, Key>,
    /// The `text_identity` blocks at the end of the file, each with its verdict.
    pub identities: Vec<Note>,
}

#[derive(Serialize)]
pub struct Name {
    pub key: String,
    pub line: usize,
    pub kind: Option<Note>,
    pub breaks: Vec<Note>,
}

#[derive(Serialize)]
pub struct Klm {
    pub path: String,
    pub language: String,
    pub names: BTreeMap<String, Name>,
}

#[derive(Serialize)]
pub struct Entry {
    pub kind: String,
    pub line: usize,
}

#[derive(Serialize)]
pub struct Ruleset {
    pub path: String,
    pub entries: BTreeMap<String, Vec<Entry>>,
}

#[derive(Serialize)]
pub struct Bridge {
    pub dir: String,
    pub klq: Option<Klq>,
    pub maps: Vec<Klm>,
    pub rulesets: Vec<Ruleset>,
}

/// The lines of a file that say something: each with its number from 0, its first word and the rest.
fn said(text: &str) -> impl Iterator<Item = (usize, &str, &str)> {
    text.lines().enumerate().filter_map(|(line, raw)| {
        let trimmed = raw.trim();
        if trimmed.is_empty() || trimmed.starts_with('#') {
            return None;
        }
        let (word, rest) = trimmed.split_once(char::is_whitespace).unwrap_or((trimmed, ""));
        Some((line, word, rest.trim()))
    })
}

fn note(word: &str, rest: &str, line: usize) -> Note {
    Note { word: word.to_string(), rest: rest.to_string(), line }
}

pub fn klq(path: &str, text: &str) -> Klq {
    let mut keys: BTreeMap<String, Key> = BTreeMap::new();
    let mut identities = Vec::new();
    let mut key: Option<String> = None;
    // Where the lines under a pair go: the pair's own index in its key, or an identity block.
    let mut under_pair = false;
    let mut under_identity = false;
    for (line, word, rest) in said(text) {
        match word {
            "klq" => {}
            "key" => {
                let name = rest.split_whitespace().next().unwrap_or("").to_string();
                keys.entry(name.clone()).or_insert_with(|| Key { line, pairs: Vec::new() }).line = line;
                key = Some(name);
                under_pair = false;
                under_identity = false;
            }
            "pair" => {
                let mut words = rest.split_whitespace();
                let first = words.next().unwrap_or("").to_string();
                let second = words.next().unwrap_or("").to_string();
                let owner = key.clone().unwrap_or_else(|| first.clone());
                keys.entry(owner).or_default().pairs.push(Pair { first, second, line, verdict: None, notes: Vec::new() });
                under_pair = true;
                under_identity = false;
            }
            "text_identity" => {
                identities.push(note(word, rest, line));
                under_pair = false;
                under_identity = true;
            }
            _ => {
                let verdict = word == "open" || word == "closed";
                if under_identity && verdict {
                    if let Some(last) = identities.last_mut() {
                        last.rest = format!("{}  {word} {rest}", last.rest);
                    }
                    under_identity = false;
                    continue;
                }
                if !under_pair {
                    continue;
                }
                let Some(pair) = key.as_ref().and_then(|k| keys.get_mut(k)).and_then(|k| k.pairs.last_mut()) else { continue };
                if verdict && pair.verdict.is_none() {
                    pair.verdict = Some(note(word, rest, line));
                } else {
                    pair.notes.push(note(word, rest, line));
                }
            }
        }
    }
    Klq { path: path.to_string(), keys, identities }
}

pub fn klm(path: &str, text: &str) -> Klm {
    let mut language = String::new();
    let mut names: BTreeMap<String, Name> = BTreeMap::new();
    let blank = |line| Name { key: String::new(), line, kind: None, breaks: Vec::new() };
    for (line, word, rest) in said(text) {
        let mut words = rest.split_whitespace();
        let name = words.next().unwrap_or("").to_string();
        let value = words.collect::<Vec<_>>().join(" ");
        match word {
            "klm" => language = name,
            "key" => {
                let entry = names.entry(name).or_insert_with(|| blank(line));
                entry.key = value;
                entry.line = line;
            }
            "kind" => names.entry(name).or_insert_with(|| blank(line)).kind = Some(note(word, &value, line)),
            "breaks" => names.entry(name).or_insert_with(|| blank(line)).breaks.push(note(word, &value, line)),
            _ => {}
        }
    }
    Klm { path: path.to_string(), language, names }
}

/// A ruleset's entries by name: every line whose first word is a kind in lower case and whose
/// second is a name, `form lane_open = ...`, `nop state_loop`, `header lane_open`.
pub fn ruleset(path: &str, text: &str) -> Ruleset {
    let mut entries: BTreeMap<String, Vec<Entry>> = BTreeMap::new();
    let is_kind = |word: &str| !word.is_empty() && word.chars().all(|c| c.is_ascii_lowercase() || c == '_');
    let is_name = |word: &str| word.chars().next().is_some_and(|c| c.is_ascii_alphabetic() || c == '_') && word.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    for (line, word, rest) in said(text) {
        let name = rest.split_whitespace().next().unwrap_or("");
        if is_kind(word) && is_name(name) {
            entries.entry(name.to_string()).or_default().push(Entry { kind: word.to_string(), line });
        }
    }
    Ruleset { path: path.to_string(), entries }
}

pub fn read(root: &Path) -> Bridge {
    let dir = root.join(DIR);
    let mut files: Vec<_> = fs::read_dir(&dir).into_iter().flatten().flatten().map(|entry| entry.path()).filter(|p| p.is_file()).collect();
    files.sort();
    let mut bridge = Bridge { dir: DIR.to_string(), klq: None, maps: Vec::new(), rulesets: Vec::new() };
    for file in files {
        let ext = file.extension().map(|e| e.to_string_lossy().into_owned()).unwrap_or_default();
        if !matches!(ext.as_str(), "klq" | "klm" | "krs" | "kdm") {
            continue;
        }
        let Ok(text) = fs::read_to_string(&file) else { continue };
        let path = relative(root, &file);
        match ext.as_str() {
            "klq" => bridge.klq = Some(klq(&path, &text)),
            "klm" => bridge.maps.push(klm(&path, &text)),
            _ => bridge.rulesets.push(ruleset(&path, &text)),
        }
    }
    bridge
}

#[cfg(test)]
mod reading {
    #[test]
    fn a_pair_keeps_its_verdict_and_every_line_under_it() {
        let text = "klq L*\nkey word_copy\npair word_copy thread_idx_x\nopen 3\nqualifier_coherence\nconcept_coherence 00ff\nkey word_set\ntext_identity a = b\nclosed @x 1->0\n";
        let read = super::klq("Lstar.klq", text);
        let pair = &read.keys["word_copy"].pairs[0];
        assert_eq!((pair.first.as_str(), pair.second.as_str()), ("word_copy", "thread_idx_x"));
        assert_eq!(pair.verdict.as_ref().map(|v| (v.word.as_str(), v.rest.as_str())), Some(("open", "3")));
        let words: Vec<_> = pair.notes.iter().map(|n| n.word.as_str()).collect();
        assert_eq!(words, ["qualifier_coherence", "concept_coherence"]);
        assert!(read.keys["word_set"].pairs.is_empty());
        assert!(read.identities[0].rest.ends_with("closed @x 1->0"));
    }

    #[test]
    fn a_map_reads_a_name_and_its_key_apart() {
        let read = super::klm("cu.klm", "klm cu\nkey lane_body lane_core\nkind lane_body nop\nbreaks a b\n");
        assert_eq!(read.language, "cu");
        assert_eq!(read.names["lane_body"].key, "lane_core");
        assert_eq!(read.names["lane_body"].kind.as_ref().unwrap().rest, "nop");
        assert_eq!(read.names["a"].breaks[0].rest, "b");
    }

    #[test]
    fn the_tree_gives_the_bridge() {
        let Some(root) = crate::root::find() else { return };
        let bridge = super::read(&root);
        let klq = bridge.klq.expect("Lstar.klq");
        assert!(klq.keys.contains_key("lane_open"));
        assert!(bridge.maps.iter().any(|m| m.language == "cu" && m.names.contains_key("lane_open")));
        assert!(bridge.rulesets.iter().any(|r| r.path.ends_with("sass.krs") && r.entries.contains_key("lane_open")));
    }
}
