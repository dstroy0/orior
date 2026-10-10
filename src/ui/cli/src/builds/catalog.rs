// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Gradle's version catalogs, `gradle/libs.versions.toml` and the rest: their versions, libraries,
//! bundles and plugins read, checked against each other, and completed. A script names an alias by
//! its accessor, the alias with each `-` and `_` a `.`: `groovy-core` is `libs.groovy.core`.

use std::path::Path;

use super::{edit_fix, nearest, relative, run_before, run_fix, snapshot, Finding, Lines, ERROR, NOTE, WARNING};
use crate::servers::Item;

/// The sections a catalog holds.
const SECTIONS: [&str; 5] = ["versions", "libraries", "bundles", "plugins", "metadata"];

/// The keys of a library's table, a plugin's and a version's.
const LIBRARY_KEYS: [&str; 5] = ["module", "group", "name", "version", "version.ref"];
const PLUGIN_KEYS: [&str; 3] = ["id", "version", "version.ref"];
const VERSION_KEYS: [&str; 5] = ["strictly", "require", "prefer", "reject", "rejectAll"];

/// The first words of a library's alias that name the catalog's other sections, which no library's
/// alias may start with.
const RESERVED: [&str; 6] = ["bundles", "versions", "plugins", "extensions", "class", "convention"];

/// A value as the catalog writes it: a string and the span inside its quotes, a table of fields, a
/// list of strings, or a bare word.
#[derive(Debug)]
pub(crate) enum Shape {
    Text(String, usize, usize),
    Table(Vec<Field>),
    List(Vec<(String, usize, usize)>),
    Bare(String),
}

/// A key and its value, the key's span kept.
#[derive(Debug)]
pub(crate) struct Field {
    pub key: String,
    pub from: usize,
    pub to: usize,
    pub value: Shape,
}

/// A section of the catalog: its name and span, and its entries.
#[derive(Debug)]
pub(crate) struct Section {
    pub name: String,
    pub from: usize,
    pub to: usize,
    pub entries: Vec<Field>,
}

/// A catalog as read, and what kept it from reading.
pub(crate) struct Parsed {
    pub sections: Vec<Section>,
    pub errors: Vec<(usize, usize, String)>,
}

/// The accessor a script names an alias by.
pub(crate) fn accessor(alias: &str) -> String {
    alias.replace(['-', '_'], ".")
}

/// The aliases of a catalog by section, each with what it stands for.
#[derive(Default)]
pub(crate) struct Aliases {
    pub libraries: Vec<(String, String)>,
    pub versions: Vec<(String, String)>,
    pub plugins: Vec<(String, String)>,
    pub bundles: Vec<(String, String)>,
}

/// The aliases of the catalog `text`.
pub(crate) fn aliases(text: &str) -> Aliases {
    let parsed = parse(text);
    let mut found = Aliases::default();
    for section in &parsed.sections {
        for entry in &section.entries {
            let about = describe(&entry.value);
            match section.name.as_str() {
                "libraries" => found.libraries.push((entry.key.clone(), about)),
                "versions" => found.versions.push((entry.key.clone(), about)),
                "plugins" => found.plugins.push((entry.key.clone(), about)),
                "bundles" => found.bundles.push((entry.key.clone(), about)),
                _ => {}
            }
        }
    }
    found
}

/// What a value says, in a few words, for a completion's detail.
fn describe(value: &Shape) -> String {
    match value {
        Shape::Text(text, _, _) => text.clone(),
        Shape::Bare(text) => text.clone(),
        Shape::List(items) => items.iter().map(|(text, _, _)| text.as_str()).collect::<Vec<_>>().join(", "),
        Shape::Table(fields) => {
            let get = |key: &str| fields.iter().find(|field| field.key == key).and_then(|field| match &field.value {
                Shape::Text(text, _, _) => Some(text.clone()),
                _ => None,
            });
            let module = get("module").or_else(|| Some(format!("{}:{}", get("group")?, get("name")?))).or_else(|| get("id")).unwrap_or_default();
            match (get("version"), get("version.ref")) {
                (Some(version), _) => format!("{module}:{version}"),
                (_, Some(name)) => format!("{module}, version {name}"),
                _ => module,
            }
        }
    }
}

/// Reads a catalog's text.
pub(crate) fn parse(text: &str) -> Parsed {
    let mut read = Reader { text, at: 0, errors: Vec::new() };
    let mut sections: Vec<Section> = Vec::new();
    loop {
        read.blank();
        let Some(char) = read.peek() else {
            break;
        };
        if char == '[' {
            let from = read.at + 1;
            let end = text[read.at..].find(['\n', ']']).map_or(text.len(), |at| read.at + at);
            if text[end..].starts_with(']') {
                sections.push(Section { name: text[from..end].trim().to_string(), from, to: end, entries: Vec::new() });
                read.at = end + 1;
            } else {
                read.errors.push((read.at, end, "A section's name is closed with `]`".to_string()));
                read.at = end;
            }
            read.rest_of_line();
            continue;
        }
        let start = read.at;
        match read.field() {
            Some(field) => match sections.last_mut() {
                Some(section) => section.entries.push(field),
                None => read.errors.push((start, read.at, "An entry stands under a section: `[versions]`, `[libraries]`, `[bundles]` or `[plugins]`".to_string())),
            },
            None => read.skip_line(),
        }
        read.rest_of_line();
    }
    Parsed { sections, errors: read.errors }
}

struct Reader<'a> {
    text: &'a str,
    at: usize,
    errors: Vec<(usize, usize, String)>,
}

impl Reader<'_> {
    fn peek(&self) -> Option<char> {
        self.text[self.at..].chars().next()
    }

    /// Passes spaces, and blank lines and comments with them.
    fn blank(&mut self) {
        while let Some(char) = self.peek() {
            if char == '#' {
                self.skip_line();
            } else if char.is_whitespace() {
                self.at += char.len_utf8();
            } else {
                break;
            }
        }
    }

    /// Passes spaces and tabs on the line.
    fn spaces(&mut self) {
        while let Some(char) = self.peek().filter(|char| *char == ' ' || *char == '\t') {
            self.at += char.len_utf8();
        }
    }

    fn skip_line(&mut self) {
        self.at = self.text[self.at..].find('\n').map_or(self.text.len(), |at| self.at + at + 1);
    }

    /// What follows a value on its line: a comment, or nothing.
    fn rest_of_line(&mut self) {
        self.spaces();
        match self.peek() {
            None | Some('\n') | Some('\r') | Some('#') => {}
            Some(_) => {
                let end = self.text[self.at..].find('\n').map_or(self.text.len(), |at| self.at + at);
                self.errors.push((self.at, end, "A line holds one entry".to_string()));
            }
        }
        self.skip_line();
    }

    /// A key, `=`, and a value.
    fn field(&mut self) -> Option<Field> {
        let from = self.at;
        let key = self.key()?;
        let to = self.at;
        self.spaces();
        if self.peek() != Some('=') {
            let end = self.text[self.at..].find('\n').map_or(self.text.len(), |at| self.at + at);
            self.errors.push((from, end.max(to), format!("`{key}` is given its value with `=`")));
            return None;
        }
        self.at += 1;
        self.spaces();
        let value = self.value()?;
        Some(Field { key, from, to, value })
    }

    /// A key, bare or quoted, its parts joined by dots.
    fn key(&mut self) -> Option<String> {
        let mut key = String::new();
        loop {
            match self.peek() {
                Some(quote @ ('"' | '\'')) => {
                    let (text, _, _) = self.string(quote)?;
                    key.push_str(&text);
                }
                Some(char) if char.is_alphanumeric() || char == '_' || char == '-' => {
                    let start = self.at;
                    while let Some(char) = self.peek().filter(|char| char.is_alphanumeric() || *char == '_' || *char == '-') {
                        self.at += char.len_utf8();
                    }
                    key.push_str(&self.text[start..self.at]);
                }
                _ => {
                    let end = self.text[self.at..].find('\n').map_or(self.text.len(), |at| self.at + at);
                    self.errors.push((self.at, end.max(self.at + 1).min(self.text.len()), "An entry starts with its name".to_string()));
                    return None;
                }
            }
            self.spaces();
            if self.peek() == Some('.') {
                self.at += 1;
                self.spaces();
                key.push('.');
            } else {
                return Some(key);
            }
        }
    }

    /// A string in `quote`s: its text and the span inside the quotes.
    fn string(&mut self, quote: char) -> Option<(String, usize, usize)> {
        let start = self.at;
        self.at += 1;
        let from = self.at;
        let mut text = String::new();
        while let Some(char) = self.peek() {
            if char == quote {
                let to = self.at;
                self.at += 1;
                return Some((text, from, to));
            }
            if char == '\n' {
                break;
            }
            if char == '\\' && quote == '"' {
                self.at += 1;
                if let Some(next) = self.peek() {
                    text.push(next);
                    self.at += next.len_utf8();
                }
                continue;
            }
            text.push(char);
            self.at += char.len_utf8();
        }
        self.errors.push((start, self.at, "A string is closed on its line".to_string()));
        None
    }

    fn value(&mut self) -> Option<Shape> {
        match self.peek()? {
            quote @ ('"' | '\'') => self.string(quote).map(|(text, from, to)| Shape::Text(text, from, to)),
            '{' => {
                self.at += 1;
                let mut fields = Vec::new();
                loop {
                    self.spaces();
                    match self.peek() {
                        Some('}') => {
                            self.at += 1;
                            return Some(Shape::Table(fields));
                        }
                        Some(',') => self.at += 1,
                        None | Some('\n') => {
                            self.errors.push((self.at.saturating_sub(1), self.at, "A table is closed with `}` on its line".to_string()));
                            return Some(Shape::Table(fields));
                        }
                        Some(_) => fields.push(self.field()?),
                    }
                }
            }
            '[' => {
                let start = self.at;
                self.at += 1;
                let mut items = Vec::new();
                loop {
                    self.blank();
                    match self.peek() {
                        Some(']') => {
                            self.at += 1;
                            return Some(Shape::List(items));
                        }
                        Some(',') => self.at += 1,
                        Some(quote @ ('"' | '\'')) => items.push(self.string(quote)?),
                        None => {
                            self.errors.push((start, start + 1, "A list is closed with `]`".to_string()));
                            return Some(Shape::List(items));
                        }
                        Some(_) => {
                            let end = self.text[self.at..].find([',', ']', '\n']).map_or(self.text.len(), |at| self.at + at);
                            self.errors.push((self.at, end, "A bundle lists its libraries' aliases as strings".to_string()));
                            self.at = end;
                        }
                    }
                }
            }
            _ => {
                let start = self.at;
                let end = self.text[self.at..].find([',', '}', ']', '\n', '#']).map_or(self.text.len(), |at| self.at + at);
                self.at = end;
                Some(Shape::Bare(self.text[start..end].trim().to_string()))
            }
        }
    }
}

/// The folder of the Gradle build a catalog belongs to: the one above its `gradle` folder.
fn build_folder(root: &Path, path: &Path) -> String {
    let folder = path.parent().and_then(Path::parent).unwrap_or(root);
    super::inside(root, folder)
}

/// The findings of a catalog.
pub(crate) fn check(root: &Path, path: &Path, text: &str) -> Vec<Finding> {
    let lines = Lines::new(text);
    let parsed = parse(text);
    let mut found: Vec<Finding> = parsed.errors.iter().map(|(from, to, message)| Finding::new(&lines, *from, *to, ERROR, "catalog-syntax", message.clone())).collect();
    let keys = |name: &str| -> Vec<String> { parsed.sections.iter().filter(|section| section.name == name).flat_map(|section| section.entries.iter()).map(|entry| accessor(&entry.key)).collect() };
    let versions = keys("versions");
    let libraries = keys("libraries");
    let folder = build_folder(root, path);
    let update = |coordinates: &str| run_fix(format!("Update the snapshot {coordinates}"), folder.clone(), format!("{} --refresh-dependencies dependencies", super::gradle::program(&root.join(&folder))));
    for section in &parsed.sections {
        if !SECTIONS.contains(&section.name.as_str()) {
            let mut finding = Finding::new(&lines, section.from, section.to, WARNING, "catalog-section", format!("A catalog has no section `[{}]`: its sections are `[versions]`, `[libraries]`, `[bundles]` and `[plugins]`", section.name));
            for near in nearest(&section.name, SECTIONS.into_iter()) {
                finding.fixes.push(edit_fix(format!("Change to [{near}]"), &lines, section.from, section.to, &near));
            }
            found.push(finding);
            continue;
        }
        if section.name == "metadata" {
            continue;
        }
        let mut seen: Vec<String> = Vec::new();
        for entry in &section.entries {
            let alias = &entry.key;
            let named = accessor(alias);
            if seen.contains(&named) {
                found.push(Finding::new(&lines, entry.from, entry.to, ERROR, "catalog-alias", format!("`{alias}` has the accessor of an alias above it, `{named}`")));
            }
            seen.push(named.clone());
            let shaped = alias.chars().next().is_some_and(|char| char.is_ascii_lowercase()) && alias.len() > 1 && alias.chars().all(|char| char.is_ascii_alphanumeric() || "_.-".contains(char));
            if !shaped {
                found.push(Finding::new(&lines, entry.from, entry.to, ERROR, "catalog-alias", format!("`{alias}` is no alias: an alias starts with a lowercase letter and goes on in letters, digits, `-`, `_` and `.`")));
            }
            if section.name == "libraries" && named.split('.').next().is_some_and(|first| RESERVED.contains(&first)) {
                found.push(Finding::new(&lines, entry.from, entry.to, ERROR, "catalog-alias", format!("A library's alias cannot start with `{}`, the name of a section of the catalog's accessor", named.split('.').next().unwrap_or_default())));
            }
            match section.name.as_str() {
                "versions" => check_version(&lines, entry, &update, &mut found),
                "libraries" => check_library(&lines, entry, &versions, &update, &mut found),
                "plugins" => check_plugin(&lines, entry, &versions, &update, &mut found),
                "bundles" => check_bundle(&lines, entry, &libraries, &mut found),
                _ => {}
            }
        }
    }
    found
}

fn check_version(lines: &Lines, entry: &Field, update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    match &entry.value {
        Shape::Text(text, from, to) if snapshot(text) => {
            let mut finding = Finding::new(lines, *from, *to, NOTE, "snapshot", format!("`{text}` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies"));
            finding.fixes.push(update(text));
            found.push(finding);
        }
        Shape::Text(..) => {}
        Shape::Table(fields) => {
            for field in fields.iter().filter(|field| !VERSION_KEYS.contains(&field.key.as_str())) {
                found.push(unknown_key(lines, field, &VERSION_KEYS, "a version's table"));
            }
        }
        _ => found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-version", format!("`{}` is a version written as a string, or a table of `strictly`, `require`, `prefer` and `reject`", entry.key))),
    }
}

fn unknown_key(lines: &Lines, field: &Field, known: &[&str], of: &str) -> Finding {
    let mut finding = Finding::new(lines, field.from, field.to, WARNING, "catalog-key", format!("{of} has no key `{}`: its keys are {}", field.key.replace('`', ""), known.iter().map(|key| format!("`{key}`")).collect::<Vec<_>>().join(", ")));
    for near in nearest(&field.key, known.iter().copied()) {
        finding.fixes.push(edit_fix(format!("Change to {near}"), lines, field.from, field.to, &near));
    }
    finding
}

/// Checks the version a library's or a plugin's table names, by `version` or `version.ref`.
fn check_versioned(lines: &Lines, fields: &[Field], versions: &[String], coordinates: &str, update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    for field in fields {
        let named = match (&field.key[..], &field.value) {
            ("version.ref", Shape::Text(name, from, to)) => Some((name, *from, *to)),
            ("version", Shape::Table(inner)) => inner.iter().find(|one| one.key == "ref").and_then(|one| match &one.value {
                Shape::Text(name, from, to) => Some((name, *from, *to)),
                _ => None,
            }),
            ("version", Shape::Text(version, from, to)) if snapshot(version) => {
                let mut finding = Finding::new(lines, *from, *to, NOTE, "snapshot", format!("`{coordinates}:{version}` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies"));
                finding.fixes.push(update(&format!("{coordinates}:{version}")));
                found.push(finding);
                None
            }
            _ => None,
        };
        if let Some((name, from, to)) = named {
            if !versions.contains(&accessor(name)) {
                let mut finding = Finding::new(lines, from, to, ERROR, "catalog-reference", format!("The catalog's `[versions]` has no `{name}`"));
                for near in nearest(&accessor(name), versions.iter().map(String::as_str)) {
                    finding.fixes.push(edit_fix(format!("Change to {near}"), lines, from, to, &near));
                }
                found.push(finding);
            }
        }
    }
}

fn check_library(lines: &Lines, entry: &Field, versions: &[String], update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    match &entry.value {
        Shape::Text(text, from, to) => {
            let parts: Vec<&str> = text.split(':').collect();
            if parts.len() != 3 || parts.iter().any(|part| part.trim().is_empty()) {
                found.push(Finding::new(lines, *from, *to, ERROR, "catalog-library", format!("`{text}` is no library: a library's string is `group:name:version`")));
            } else if snapshot(parts[2]) {
                let mut finding = Finding::new(lines, *from, *to, NOTE, "snapshot", format!("`{text}` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies"));
                finding.fixes.push(update(text));
                found.push(finding);
            }
        }
        Shape::Table(fields) => {
            for field in fields.iter().filter(|field| !LIBRARY_KEYS.contains(&field.key.as_str())) {
                found.push(unknown_key(lines, field, &LIBRARY_KEYS, "A library's table"));
            }
            let get = |key: &str| fields.iter().find(|field| field.key == key);
            let coordinates = match (get("module"), get("group"), get("name")) {
                (Some(Field { value: Shape::Text(module, from, to), .. }), _, _) => {
                    let parts: Vec<&str> = module.split(':').collect();
                    if parts.len() != 2 || parts.iter().any(|part| part.trim().is_empty()) {
                        found.push(Finding::new(lines, *from, *to, ERROR, "catalog-library", format!("`{module}` is no module: a module is `group:name`")));
                    }
                    module.clone()
                }
                (None, Some(Field { value: Shape::Text(group, ..), .. }), Some(Field { value: Shape::Text(name, ..), .. })) => format!("{group}:{name}"),
                _ => {
                    found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-library", format!("`{}` names its library by `module`, or by `group` and `name`", entry.key)));
                    String::new()
                }
            };
            check_versioned(lines, fields, versions, &coordinates, update, found);
        }
        _ => found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-library", format!("`{}` is a library written as a string, `group:name:version`, or a table", entry.key))),
    }
}

fn check_plugin(lines: &Lines, entry: &Field, versions: &[String], update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    match &entry.value {
        Shape::Text(text, from, to) => {
            let parts: Vec<&str> = text.split(':').collect();
            if parts.len() != 2 || parts.iter().any(|part| part.trim().is_empty()) {
                found.push(Finding::new(lines, *from, *to, ERROR, "catalog-plugin", format!("`{text}` is no plugin: a plugin's string is `id:version`")));
            }
        }
        Shape::Table(fields) => {
            for field in fields.iter().filter(|field| !PLUGIN_KEYS.contains(&field.key.as_str())) {
                found.push(unknown_key(lines, field, &PLUGIN_KEYS, "A plugin's table"));
            }
            let id = fields.iter().find(|field| field.key == "id").and_then(|field| match &field.value {
                Shape::Text(id, ..) => Some(id.clone()),
                _ => None,
            });
            match id {
                Some(id) => check_versioned(lines, fields, versions, &id, update, found),
                None => found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-plugin", format!("`{}` names its plugin by `id`", entry.key))),
            }
        }
        _ => found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-plugin", format!("`{}` is a plugin written as a string, `id:version`, or a table", entry.key))),
    }
}

fn check_bundle(lines: &Lines, entry: &Field, libraries: &[String], found: &mut Vec<Finding>) {
    let Shape::List(members) = &entry.value else {
        found.push(Finding::new(lines, entry.from, entry.to, ERROR, "catalog-bundle", format!("`{}` is a bundle written as a list of libraries' aliases", entry.key)));
        return;
    };
    for (member, from, to) in members {
        if !libraries.contains(&accessor(member)) {
            let mut finding = Finding::new(lines, *from, *to, ERROR, "catalog-reference", format!("The catalog's `[libraries]` has no `{member}`"));
            for near in nearest(&accessor(member), libraries.iter().map(String::as_str)) {
                finding.fixes.push(edit_fix(format!("Change to {near}"), lines, *from, *to, &near));
            }
            found.push(finding);
        }
    }
}

/// What completes the word before byte `at` of a catalog: a section's name after `[`, a table's
/// keys, the versions' aliases in a `version.ref`, and the libraries' in a bundle.
pub(crate) fn complete(text: &str, at: usize) -> Vec<Item> {
    let line_start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    let before = &text[line_start..at];
    let section = text[..line_start].lines().rev().find_map(|line| line.trim().strip_prefix('[').and_then(|rest| rest.split(']').next()).map(|name| name.trim().to_string())).unwrap_or_default();
    let typed = |of: fn(char) -> bool| run_before(text, at, of);
    let names = |chosen: &str| -> Vec<(String, &'static str, String)> {
        let parsed = parse(text);
        parsed.sections.iter().filter(|one| one.name == chosen).flat_map(|one| one.entries.iter()).map(|entry| (entry.key.clone(), "constant", describe(&entry.value))).collect()
    };
    if before.trim_start().starts_with('[') && !before.contains(']') {
        let token = typed(|char| char.is_alphanumeric());
        return relative(token, SECTIONS.iter().map(|name| (name.to_string(), "module", "section".to_string())).collect());
    }
    let quotes = before.chars().filter(|char| *char == '"' || *char == '\'').count();
    let token = typed(|char| char.is_alphanumeric() || "_.-".contains(char));
    if quotes % 2 == 1 {
        let opened = before[..before.len() - token.len()].trim_end_matches(['"', '\'']).trim_end();
        if opened.ends_with('=') && (opened.trim_end_matches('=').trim_end().ends_with("version.ref") || opened.trim_end_matches('=').trim_end().ends_with("ref")) {
            return relative(token, names("versions"));
        }
        if section == "bundles" {
            return relative(token, names("libraries"));
        }
        return Vec::new();
    }
    let open = before.rfind('{');
    if open.is_some_and(|open| !before[open..].contains('}')) {
        let last = before.rfind(['{', ',', '=']);
        if last.is_some_and(|last| matches!(before.as_bytes()[last], b'{' | b',')) {
            let keys: &[&str] = match section.as_str() {
                "libraries" => &LIBRARY_KEYS,
                "plugins" => &PLUGIN_KEYS,
                "versions" => &VERSION_KEYS,
                _ => &[],
            };
            let token = typed(|char| char.is_alphanumeric() || char == '.');
            return relative(token, keys.iter().map(|key| (key.to_string(), "field", format!("key of [{section}]"))).collect());
        }
    }
    Vec::new()
}

#[cfg(test)]
mod tests {
    use super::*;

    const CATALOG: &str = "[versions]\ngroovy = \"3.0.5\"\ncheckstyle = \"8.37\"\nmine = \"1.2-SNAPSHOT\"\n\n[libraries]\ngroovy-core = { module = \"org.codehaus.groovy:groovy\", version.ref = \"groovy\" }\ngroovy-json = { module = \"org.codehaus.groovy:groovy-json\", version.ref = \"grovy\" }\ncommons-lang3 = { group = \"org.apache.commons\", name = \"commons-lang3\", version = { strictly = \"[3.8, 4.0[\", prefer = \"3.9\" } }\nbad = \"only:two\"\n\n[bundles]\ngroovy = [\"groovy-core\", \"groovy-jsn\"]\n\n[plugins]\nversions = { id = \"com.github.ben-manes.versions\", version = \"0.45.0\" }\n\n[librarys]\n";

    #[test]
    fn a_catalog_reads_its_sections() {
        let parsed = parse(CATALOG);
        assert!(parsed.errors.is_empty(), "{:?}", parsed.errors);
        assert_eq!(parsed.sections.iter().map(|one| one.name.as_str()).collect::<Vec<_>>(), ["versions", "libraries", "bundles", "plugins", "librarys"]);
        let found = aliases(CATALOG);
        assert_eq!(found.libraries[0], ("groovy-core".to_string(), "org.codehaus.groovy:groovy, version groovy".to_string()));
        assert_eq!(found.bundles[0].1, "groovy-core, groovy-jsn");
    }

    #[test]
    fn a_catalog_is_checked_against_itself() {
        let root = Path::new("/tree");
        let found = check(root, &root.join("gradle/libs.versions.toml"), CATALOG);
        let said: Vec<(&str, u8, String)> = found.iter().map(|one| (one.code, one.severity, one.message.clone())).collect();
        assert!(said.iter().any(|(code, _, message)| *code == "catalog-reference" && message.contains("`grovy`")), "{said:?}");
        assert!(said.iter().any(|(code, _, message)| *code == "catalog-reference" && message.contains("`groovy-jsn`")), "{said:?}");
        assert!(said.iter().any(|(code, _, message)| *code == "catalog-library" && message.contains("only:two")), "{said:?}");
        assert!(said.iter().any(|(code, severity, _)| *code == "snapshot" && *severity == NOTE), "{said:?}");
        assert!(said.iter().any(|(code, _, _)| *code == "catalog-section"), "{said:?}");
        let grovy = found.iter().find(|one| one.message.contains("`grovy`")).unwrap();
        assert_eq!(grovy.fixes[0]["title"], "Change to groovy");
        let snapshot = found.iter().find(|one| one.code == "snapshot").unwrap();
        assert_eq!(snapshot.fixes[0]["run"]["line"], "gradle --refresh-dependencies dependencies");
        assert_eq!(found.len(), 5, "{said:?}");
    }

    #[test]
    fn a_catalog_completes_sections_keys_and_aliases() {
        let labels = |text: &str| complete(text, text.len()).into_iter().map(|item| item.label).collect::<Vec<_>>();
        assert!(labels("[versions]\na = \"1\"\n[lib").contains(&"libraries".to_string()));
        assert_eq!(labels("[versions]\ngroovy = \"1\"\n[libraries]\nx = { module = \"a:b\", version.ref = \"gr"), ["groovy"]);
        assert!(labels("[libraries]\nx = { mod").contains(&"module".to_string()));
        assert_eq!(labels("[libraries]\nx = { module = \"a:b\", version."), ["ref"]);
        assert_eq!(labels("[libraries]\ngroovy-core = \"a:b:1\"\n[bundles]\nall = [\"gro"), ["groovy-core"]);
    }
}
