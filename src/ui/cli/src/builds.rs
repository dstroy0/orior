// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Build files: Gradle's build and settings scripts, in Groovy or Kotlin, and its version catalogs;
//! Maven's `pom.xml`; and SCons's SConstruct and SConscript. Each is checked and completed as it is
//! written, by its own text and what the tree around it holds: a catalog's aliases where a
//! script names them, a POM's properties and its parent's, the SConscripts a script reads and the
//! names they export.
//!
//! A dependency whose version is a snapshot is noted, and its note offers the command that fetches
//! the snapshot's newest build, run in the terminal; `snapshots_line` gives the one for a whole
//! build. `maven` also reads a POM's compiler settings and Maven's own, from `settings.xml`, and
//! writes them again as they are set.

use std::path::{Path, PathBuf};

use serde_json::{json, Value};

use crate::servers::{Item, Place, TextEdit};

mod catalog;
mod gradle;
pub mod maven;
mod scons;

/// The kinds of build file read here.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Kind {
    Gradle,
    Catalog,
    Maven,
    Scons,
}

/// The kind of build file at `path`, by its name.
pub fn kind_of(path: &Path) -> Option<Kind> {
    let name = path.file_name()?.to_string_lossy().to_lowercase();
    if name.ends_with(".gradle") || name.ends_with(".gradle.kts") {
        Some(Kind::Gradle)
    } else if name.ends_with(".versions.toml") {
        Some(Kind::Catalog)
    } else if name == "pom.xml" {
        Some(Kind::Maven)
    } else if name == "sconstruct" || name == "sconscript" {
        Some(Kind::Scons)
    } else {
        None
    }
}

/// The diagnostics of the build file at `path` of the tree at `root`, its text `text`, in the
/// protocol's form.
pub fn check(root: &Path, path: &Path, text: &str) -> Vec<Value> {
    let found = match kind_of(path) {
        Some(Kind::Gradle) => gradle::check(root, path, text),
        Some(Kind::Catalog) => catalog::check(root, path, text),
        Some(Kind::Maven) => maven::check(root, path, text),
        Some(Kind::Scons) => scons::check(root, path, text),
        None => Vec::new(),
    };
    found.iter().map(Finding::value).collect()
}

/// What completes the word before `line`, `col` of the build file at `path`, its text `text`.
pub fn complete(root: &Path, path: &Path, text: &str, line: u32, col: u32) -> Vec<Item> {
    let at = byte_at(text, line, col);
    let mut items = match kind_of(path) {
        Some(Kind::Gradle) => gradle::complete(root, path, text, at),
        Some(Kind::Catalog) => catalog::complete(text, at),
        Some(Kind::Maven) => maven::complete(root, path, text, at),
        Some(Kind::Scons) => scons::complete(text, at),
        None => Vec::new(),
    };
    let mut seen = std::collections::HashSet::new();
    items.retain(|item| seen.insert(item.label.clone()));
    items
}

/// The names SCons gives the SConstruct or SConscript at `path` without its importing them: its
/// globals and the names its `Import` calls take. Empty for any other file.
pub fn given_names(path: &Path, text: &str) -> std::collections::HashSet<String> {
    if kind_of(path) == Some(Kind::Scons) {
        scons::given(text)
    } else {
        std::collections::HashSet::new()
    }
}

/// The folder, in the tree, and the command line that update every snapshot dependency of the build
/// `path` belongs to: Maven's where it is a POM or a POM stands beside it, Gradle's where it is a
/// Gradle file or one stands beside it, and either one at the tree's top where `path` is neither.
pub fn snapshots_line(root: &Path, path: Option<&Path>) -> Result<(String, String), String> {
    let mut places: Vec<PathBuf> = Vec::new();
    if let Some(path) = path {
        places.push(path.to_path_buf());
        if let Some(folder) = path.parent() {
            places.push(folder.join("pom.xml"));
            places.push(folder.join("build.gradle"));
            places.push(folder.join("build.gradle.kts"));
        }
    }
    places.extend(["pom.xml", "build.gradle", "build.gradle.kts", "settings.gradle", "settings.gradle.kts"].map(|name| root.join(name)));
    for place in places {
        match kind_of(&place) {
            Some(Kind::Maven) if place.is_file() => {
                let folder = place.parent().unwrap_or(root);
                return Ok((inside(root, folder), format!("{} -U dependency:resolve", maven::program(root, folder))));
            }
            Some(Kind::Gradle | Kind::Catalog) if place.is_file() => {
                let folder = gradle::build_root(root, &place);
                return Ok((inside(root, &folder), format!("{} --refresh-dependencies dependencies", gradle::program(&folder))));
            }
            _ => {}
        }
    }
    Err("No Maven or Gradle build here: a pom.xml, a build.gradle or a build.gradle.kts".to_string())
}

/// A finding in a build file: its span, how bad it is as the protocol numbers it, its code, its
/// message in Markdown, and the fixes it offers, each an edit of the file or a line to run.
pub(crate) struct Finding {
    pub from: Place,
    pub to: Place,
    pub severity: u8,
    pub code: &'static str,
    pub message: String,
    pub fixes: Vec<Value>,
}

pub(crate) const ERROR: u8 = 1;
pub(crate) const WARNING: u8 = 2;
pub(crate) const NOTE: u8 = 3;

impl Finding {
    pub fn new(lines: &Lines, from: usize, to: usize, severity: u8, code: &'static str, message: String) -> Finding {
        Finding { from: lines.place(from), to: lines.place(to), severity, code, message, fixes: Vec::new() }
    }

    fn value(&self) -> Value {
        let place = |place: &Place| json!({"line": place.line, "character": place.col});
        json!({
            "range": {"start": place(&self.from), "end": place(&self.to)},
            "severity": self.severity,
            "message": {"kind": "markdown", "value": self.message},
            "source": "orior",
            "code": self.code,
            "data": {"orior": self.fixes},
        })
    }
}

/// A fix that writes `text` over the span `from`, `to` of the file.
pub(crate) fn edit_fix(title: String, lines: &Lines, from: usize, to: usize, text: &str) -> Value {
    json!({"title": title, "edits": [TextEdit { from: lines.place(from), to: lines.place(to), text: text.to_string() }]})
}

/// A fix that runs `line` in the terminal, in `folder` of the tree.
pub(crate) fn run_fix(title: String, folder: String, line: String) -> Value {
    json!({"title": title, "run": {"folder": folder, "line": line}})
}

/// Where each line of a text starts, to turn a byte of it into a place.
pub(crate) struct Lines<'a> {
    text: &'a str,
    starts: Vec<usize>,
}

impl<'a> Lines<'a> {
    pub fn new(text: &'a str) -> Lines<'a> {
        let mut starts = vec![0];
        starts.extend(text.match_indices('\n').map(|(at, _)| at + 1));
        Lines { text, starts }
    }

    /// The line and the column, in UTF-16 units, of byte `at`.
    pub fn place(&self, at: usize) -> Place {
        let line = self.starts.partition_point(|&start| start <= at).saturating_sub(1);
        let start = self.starts[line];
        let at = at.min(self.text.len());
        Place { line: line as u32, col: self.text.get(start..at).map_or(0, |part| part.encode_utf16().count()) as u32 }
    }

    /// The byte line `line` starts at.
    pub fn start(&self, line: usize) -> usize {
        self.starts.get(line).copied().unwrap_or(self.text.len())
    }
}

/// The byte of `text` at `line`, `col`, the column in UTF-16 units.
pub(crate) fn byte_at(text: &str, line: u32, col: u32) -> usize {
    let lines = Lines::new(text);
    let start = lines.start(line as usize);
    let rest = &text[start..];
    let end = rest.find('\n').map_or(text.len(), |at| start + at);
    let mut units = 0;
    for (at, char) in text[start..end].char_indices() {
        if units >= col as usize {
            return start + at;
        }
        units += char.len_utf16();
    }
    end
}

/// Whether `char` is of a word, as the editor reads one.
pub(crate) fn word_char(char: char) -> bool {
    char.is_alphanumeric() || char == '_' || char == '$'
}

/// The run of characters `of` takes that ends at byte `at` of `text`, on its line.
pub(crate) fn run_before(text: &str, at: usize, of: impl Fn(char) -> bool) -> &str {
    let line_start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    let part = &text[line_start..at];
    let start = part.char_indices().rev().take_while(|(_, char)| of(*char)).last().map_or(part.len(), |(at, _)| at);
    &part[start..]
}

/// The items for `values`, each a value, its kind and its detail, given as each stands after what
/// was typed before the word the cursor is in: of `maven.compiler.so` typed, `source` for
/// `maven.compiler.source`. A value that does not start with what was typed is left out.
pub(crate) fn relative(token: &str, values: Vec<(String, &'static str, String)>) -> Vec<Item> {
    let word: usize = token.chars().rev().take_while(|char| word_char(*char)).map(char::len_utf8).sum();
    let base = &token[..token.len() - word];
    values
        .into_iter()
        .filter(|(value, _, _)| value.starts_with(base) && value.len() > base.len())
        .map(|(value, kind, detail)| {
            let rest = value[base.len()..].to_string();
            Item { label: rest.clone(), kind, detail, insert: rest, snippet: false }
        })
        .collect()
}

/// Plain items for `words`, each of `kind` with `detail`.
pub(crate) fn items(words: &[&str], kind: &'static str, detail: &str) -> Vec<Item> {
    words.iter().map(|word| Item { label: word.trim_end_matches("()").to_string(), kind, detail: detail.to_string(), insert: word.to_string(), snippet: false }).collect()
}

/// The fewest edits of one character that turn `a` into `b`.
pub(crate) fn distance(a: &str, b: &str) -> usize {
    let b: Vec<char> = b.chars().collect();
    let mut row: Vec<usize> = (0..=b.len()).collect();
    for (i, ca) in a.chars().enumerate() {
        let mut last = row[0];
        row[0] = i + 1;
        for (j, cb) in b.iter().enumerate() {
            let next = (row[j + 1] + 1).min(row[j] + 1).min(last + usize::from(ca != *cb));
            last = row[j + 1];
            row[j + 1] = next;
        }
    }
    row[b.len()]
}

/// Of `known`, the few nearest `name`, near enough to be what was meant, nearest first.
pub(crate) fn nearest<'a>(name: &str, known: impl Iterator<Item = &'a str>) -> Vec<String> {
    let most = (name.chars().count() / 4).max(2);
    let mut near: Vec<(usize, String)> = known.map(|one| (distance(name, one), one.to_string())).filter(|(far, one)| *far <= most && one != name).collect();
    near.sort();
    near.dedup();
    near.into_iter().take(3).map(|(_, one)| one).collect()
}

/// `folder` as a path in the tree at `root`, with forward slashes; empty for the top.
pub(crate) fn inside(root: &Path, folder: &Path) -> String {
    folder.strip_prefix(root).map(|part| part.to_string_lossy().replace('\\', "/")).unwrap_or_default()
}

/// Whether a version is a snapshot's.
pub(crate) fn snapshot(version: &str) -> bool {
    version.trim().ends_with("-SNAPSHOT")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kinds_go_by_the_file_name() {
        assert_eq!(kind_of(Path::new("a/build.gradle.kts")), Some(Kind::Gradle));
        assert_eq!(kind_of(Path::new("settings.gradle")), Some(Kind::Gradle));
        assert_eq!(kind_of(Path::new("gradle/libs.versions.toml")), Some(Kind::Catalog));
        assert_eq!(kind_of(Path::new("x/pom.xml")), Some(Kind::Maven));
        assert_eq!(kind_of(Path::new("src/SConscript")), Some(Kind::Scons));
        assert_eq!(kind_of(Path::new("Cargo.toml")), None);
    }

    #[test]
    fn values_complete_after_what_was_typed() {
        let values = vec![("maven.compiler.source".to_string(), "property", String::new()), ("project.version".to_string(), "property", String::new())];
        let found = relative("maven.compiler.so", values.clone());
        assert_eq!(found.iter().map(|item| item.insert.as_str()).collect::<Vec<_>>(), ["source"]);
        assert_eq!(relative("", values.clone()).len(), 2);
        assert_eq!(relative("maven.", values)[0].insert, "compiler.source");
    }

    #[test]
    fn places_count_columns_in_utf16() {
        let text = "ab\nxé𝄞z\n";
        let lines = Lines::new(text);
        let at = text.find('z').unwrap();
        assert_eq!(lines.place(at), Place { line: 1, col: 4 });
        assert_eq!(byte_at(text, 1, 4), at);
    }

    #[test]
    fn near_names_are_offered() {
        assert_eq!(nearest("implmentation", ["implementation", "api", "compileOnly"].into_iter()), ["implementation"]);
        assert!(nearest("zzz", ["implementation"].into_iter()).is_empty());
    }
}
