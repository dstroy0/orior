// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! JSON by its schema: a JSON file read with the place of each of its values, checked against the
//! schema its `$schema` names, or the one its name is known by, and completed from it, each key
//! the schema gives an object and each value it allows. A schema is checked against its own schema
//! in turn: the meta-schemas of drafts 4, 6 and 7, 2019-09 and 2020-12 come with orior.
//!
//! A schema is found by its path beside the file, by its URL among those orior holds, or in
//! `schemas/` of orior's own folder, where a schema fetched is kept. One named by a URL and found in
//! none of these is noted, and its note offers the line that fetches it with curl, run in the
//! terminal; nothing is fetched without that press.
//!
//! The keywords read are those of the drafts from 4 to 2020-12 that say what a value is: `type`,
//! `enum`, `const`, the bounds of numbers, strings, arrays and objects, `pattern`, `required`,
//! `properties`, `patternProperties`, `additionalProperties`, `propertyNames`, `dependencies`,
//! `dependentRequired`, `dependentSchemas`, `items`, `prefixItems`, `additionalItems`, `contains`,
//! `uniqueItems`, `allOf`, `anyOf`, `oneOf`, `not`, `if`, `then`, `else`, `$ref` within a schema
//! and across files, and `deprecated`. A comment, `//` or `/* */`, is passed over, as a JSON file
//! with comments writes one.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use serde_json::Value;

use crate::builds::{edit_fix, nearest, relative, run_fix, Finding, Lines, ERROR, NOTE, WARNING};
use crate::servers::Item;

/// The meta-schemas orior holds, each by its URL with no scheme and no `#`.
const BUNDLED: [(&str, &str); 5] = [
    ("json-schema.org/draft-07/schema", include_str!("../schemas/draft-07.json")),
    ("json-schema.org/draft-06/schema", include_str!("../schemas/draft-07.json")),
    ("json-schema.org/draft-04/schema", include_str!("../schemas/draft-04.json")),
    ("json-schema.org/draft/2020-12/schema", include_str!("../schemas/2020-12.json")),
    ("json-schema.org/draft/2019-09/schema", include_str!("../schemas/2020-12.json")),
];

/// The schemas files of these names follow where they name none, by the SchemaStore's URL.
const KNOWN: [(&str, &str); 8] = [
    ("package.json", "https://json.schemastore.org/package.json"),
    ("tsconfig.json", "https://json.schemastore.org/tsconfig.json"),
    ("jsconfig.json", "https://json.schemastore.org/jsconfig.json"),
    (".eslintrc.json", "https://json.schemastore.org/eslintrc.json"),
    (".prettierrc.json", "https://json.schemastore.org/prettierrc.json"),
    ("composer.json", "https://getcomposer.org/schema.json"),
    (".babelrc.json", "https://json.schemastore.org/babelrc.json"),
    ("devcontainer.json", "https://raw.githubusercontent.com/devcontainers/spec/main/schemas/devContainer.schema.json"),
];

/// The most documents a schema reaches by `$ref`, and the deepest a value is checked.
const MOST_DOCUMENTS: usize = 32;
const DEEPEST: usize = 64;

/// A file larger than this is not checked.
const LARGEST: usize = 2 << 20;

/// Whether the file at `path` is JSON this module reads.
pub fn reads(path: &Path) -> bool {
    path.extension().is_some_and(|ext| matches!(ext.to_string_lossy().to_lowercase().as_str(), "json" | "jsonc"))
}

// The reading of JSON.

/// A value of a JSON file and its span.
#[derive(Clone, Debug)]
pub(crate) struct Node {
    pub value: Json,
    pub from: usize,
    pub to: usize,
}

#[derive(Clone, Debug)]
pub(crate) enum Json {
    Object(Vec<Member>),
    Array(Vec<Node>),
    Text(String),
    Number(f64),
    Bool(bool),
    Null,
}

/// A key of an object, its span with its quotes, and its value where one is written.
#[derive(Clone, Debug)]
pub(crate) struct Member {
    pub key: String,
    pub from: usize,
    pub to: usize,
    pub value: Option<Node>,
}

/// A JSON file as read, and what kept it from reading.
pub(crate) struct Read {
    pub root: Option<Node>,
    pub errors: Vec<(usize, usize, String)>,
}

pub(crate) fn read(text: &str) -> Read {
    let mut parser = Parser { text, bytes: text.as_bytes(), at: 0, errors: Vec::new(), depth: 0 };
    parser.blank();
    let root = parser.value();
    parser.blank();
    if parser.at < parser.bytes.len() && root.is_some() {
        parser.errors.push((parser.at, (parser.at + 1).min(text.len()), "The file holds one value, and more stands after it".to_string()));
    }
    Read { root, errors: parser.errors }
}

struct Parser<'a> {
    text: &'a str,
    bytes: &'a [u8],
    at: usize,
    errors: Vec<(usize, usize, String)>,
    depth: usize,
}

impl Parser<'_> {
    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.at).copied()
    }

    /// Passes spaces and comments.
    fn blank(&mut self) {
        loop {
            while self.peek().is_some_and(|byte| byte.is_ascii_whitespace()) {
                self.at += 1;
            }
            if self.text[self.at..].starts_with("//") {
                self.at = self.text[self.at..].find('\n').map_or(self.bytes.len(), |end| self.at + end);
            } else if self.text[self.at..].starts_with("/*") {
                self.at = self.text[self.at + 2..].find("*/").map_or(self.bytes.len(), |end| self.at + 2 + end + 2);
            } else {
                return;
            }
        }
    }

    fn value(&mut self) -> Option<Node> {
        self.blank();
        let from = self.at;
        let byte = self.peek()?;
        if self.depth > 512 {
            self.errors.push((from, from + 1, "The values nest too deep to read".to_string()));
            self.at = self.bytes.len();
            return None;
        }
        let value = match byte {
            b'{' => {
                self.depth += 1;
                let found = self.object();
                self.depth -= 1;
                found
            }
            b'[' => {
                self.depth += 1;
                let found = self.array();
                self.depth -= 1;
                found
            }
            b'"' => Json::Text(self.string()),
            b't' | b'f' | b'n' => {
                let word: String = self.text[self.at..].chars().take_while(|char| char.is_ascii_alphabetic()).collect();
                self.at += word.len();
                match word.as_str() {
                    "true" => Json::Bool(true),
                    "false" => Json::Bool(false),
                    "null" => Json::Null,
                    _ => {
                        self.errors.push((from, self.at, format!("`{word}` is no value: a value is a string, a number, an object, an array, `true`, `false` or `null`")));
                        Json::Null
                    }
                }
            }
            b'-' | b'0'..=b'9' => {
                let len = self.text[self.at..].chars().take_while(|char| char.is_ascii_digit() || "+-.eE".contains(*char)).count();
                let raw = &self.text[self.at..self.at + len];
                self.at += len;
                match raw.parse::<f64>() {
                    Ok(number) => Json::Number(number),
                    Err(_) => {
                        self.errors.push((from, self.at, format!("`{raw}` is no number")));
                        Json::Null
                    }
                }
            }
            _ => {
                let end = self.text[self.at..].find([',', '}', ']', '\n']).map_or(self.bytes.len(), |end| self.at + end).max(self.at + 1).min(self.bytes.len());
                self.errors.push((from, end, "A value stands here: a string, a number, an object, an array, `true`, `false` or `null`".to_string()));
                self.at = end;
                return None;
            }
        };
        Some(Node { value, from, to: self.at })
    }

    /// A string in double quotes, its escapes read.
    fn string(&mut self) -> String {
        let start = self.at;
        self.at += 1;
        let mut out = String::new();
        while let Some(char) = self.text[self.at..].chars().next() {
            match char {
                '"' => {
                    self.at += 1;
                    return out;
                }
                '\n' => break,
                '\\' => {
                    self.at += 1;
                    let Some(next) = self.text[self.at..].chars().next() else {
                        break;
                    };
                    self.at += next.len_utf8();
                    match next {
                        'n' => out.push('\n'),
                        't' => out.push('\t'),
                        'r' => out.push('\r'),
                        'b' => out.push('\u{8}'),
                        'f' => out.push('\u{c}'),
                        'u' => {
                            let hex = self.text.get(self.at..self.at + 4).unwrap_or_default();
                            if let Some(char) = u32::from_str_radix(hex, 16).ok().and_then(char::from_u32) {
                                out.push(char);
                            }
                            self.at = (self.at + 4).min(self.bytes.len());
                        }
                        other => out.push(other),
                    }
                }
                other => {
                    out.push(other);
                    self.at += other.len_utf8();
                }
            }
        }
        self.errors.push((start, self.at, "This string is not closed with `\"` on its line".to_string()));
        out
    }

    fn object(&mut self) -> Json {
        let open = self.at;
        self.at += 1;
        let mut members: Vec<Member> = Vec::new();
        loop {
            self.blank();
            match self.peek() {
                Some(b'}') => {
                    self.at += 1;
                    break;
                }
                Some(b'"') => {
                    let from = self.at;
                    let key = self.string();
                    let to = self.at;
                    self.blank();
                    let value = if self.peek() == Some(b':') {
                        self.at += 1;
                        let value = self.value();
                        if value.is_none() && self.peek().is_none() {
                            self.errors.push((from, to, format!("`{key}` is given no value")));
                        }
                        value
                    } else {
                        self.errors.push((from, to, format!("`{key}` is followed by `:` and its value")));
                        None
                    };
                    if let Some(earlier) = members.iter().find(|one| one.key == key) {
                        let shown = &self.text[earlier.from..earlier.to];
                        self.errors.push((from, to, format!("{shown} is a key of this object above it")));
                    }
                    members.push(Member { key, from, to, value });
                    self.blank();
                    match self.peek() {
                        Some(b',') => {
                            let comma = self.at;
                            self.at += 1;
                            self.blank();
                            if self.peek() == Some(b'}') {
                                self.errors.push((comma, comma + 1, "No member follows this `,`".to_string()));
                            }
                        }
                        Some(b'}') => {}
                        Some(b'"') => {
                            self.errors.push((self.at, self.at + 1, "Members are parted by `,`".to_string()));
                        }
                        _ => {
                            self.errors.push((open, open + 1, "This `{` is not closed with `}`".to_string()));
                            break;
                        }
                    }
                }
                None => {
                    self.errors.push((open, open + 1, "This `{` is not closed with `}`".to_string()));
                    break;
                }
                Some(_) => {
                    let end = self.text[self.at..].find([',', '}', '\n']).map_or(self.bytes.len(), |end| self.at + end).max(self.at + 1).min(self.bytes.len());
                    self.errors.push((self.at, end, "A key is a string in double quotes".to_string()));
                    self.at = end;
                    if self.peek() == Some(b',') {
                        self.at += 1;
                    }
                }
            }
        }
        Json::Object(members)
    }

    fn array(&mut self) -> Json {
        let open = self.at;
        self.at += 1;
        let mut items = Vec::new();
        loop {
            self.blank();
            match self.peek() {
                Some(b']') => {
                    self.at += 1;
                    break;
                }
                None => {
                    self.errors.push((open, open + 1, "This `[` is not closed with `]`".to_string()));
                    break;
                }
                Some(_) => {
                    let before = self.at;
                    if let Some(item) = self.value() {
                        items.push(item);
                    }
                    self.blank();
                    match self.peek() {
                        Some(b',') => {
                            let comma = self.at;
                            self.at += 1;
                            self.blank();
                            if self.peek() == Some(b']') {
                                self.errors.push((comma, comma + 1, "No item follows this `,`".to_string()));
                            }
                        }
                        Some(b']') => {}
                        None => {}
                        Some(_) if self.at == before => {
                            self.at += 1;
                        }
                        Some(_) => self.errors.push((self.at, self.at + 1, "Items are parted by `,`".to_string())),
                    }
                }
            }
        }
        Json::Array(items)
    }
}

impl Node {
    /// The value as serde_json holds one, for `enum`, `const` and `uniqueItems`.
    fn plain(&self) -> Value {
        match &self.value {
            Json::Object(members) => Value::Object(members.iter().filter_map(|member| Some((member.key.clone(), member.value.as_ref()?.plain()))).collect()),
            Json::Array(items) => Value::Array(items.iter().map(Node::plain).collect()),
            Json::Text(text) => Value::String(text.clone()),
            Json::Number(number) => serde_json::Number::from_f64(*number).map_or(Value::Null, Value::Number),
            Json::Bool(bool) => Value::Bool(*bool),
            Json::Null => Value::Null,
        }
    }

    fn kind(&self) -> &'static str {
        match &self.value {
            Json::Object(_) => "object",
            Json::Array(_) => "array",
            Json::Text(_) => "string",
            Json::Number(number) if number.fract() == 0.0 => "integer",
            Json::Number(_) => "number",
            Json::Bool(_) => "boolean",
            Json::Null => "null",
        }
    }
}

/// Whether two values are equal as JSON's values are, a number by its value.
fn same(a: &Value, b: &Value) -> bool {
    match (a, b) {
        (Value::Number(x), Value::Number(y)) => x.as_f64() == y.as_f64(),
        (Value::Array(x), Value::Array(y)) => x.len() == y.len() && x.iter().zip(y).all(|(x, y)| same(x, y)),
        (Value::Object(x), Value::Object(y)) => x.len() == y.len() && x.iter().all(|(key, value)| y.get(key).is_some_and(|other| same(value, other))),
        _ => a == b,
    }
}

// The schemas.

/// A URL as a key among those orior holds: no scheme, no `#` and no `/` at its end.
fn url_key(url: &str) -> String {
    let rest = url.split_once("://").map_or(url, |(_, rest)| rest);
    rest.trim_end_matches('#').trim_end_matches('/').to_string()
}

/// The file a schema fetched from `url` is kept in.
pub(crate) fn kept_at(url: &str) -> Option<PathBuf> {
    let name: String = url_key(url).chars().map(|char| if char.is_ascii_alphanumeric() || char == '.' || char == '-' { char } else { '_' }).collect();
    crate::home::folder().map(|folder| folder.join("schemas").join(format!("{name}.json")))
}

/// The schemas a check reaches, each by its URL or its path, and those it could not find.
#[derive(Default)]
struct Store {
    docs: HashMap<String, Value>,
    missing: Vec<String>,
}

impl Store {
    /// The document at `key`, read where it was not: a bundled meta-schema, a file, or a schema kept
    /// from an earlier fetch.
    fn load(&mut self, key: &str) -> Option<&Value> {
        if !self.docs.contains_key(key) {
            if self.docs.len() >= MOST_DOCUMENTS || self.missing.iter().any(|one| one == key) {
                return None;
            }
            let text = if key.contains("://") {
                let wanted = url_key(key);
                BUNDLED.iter().find(|(url, _)| *url == wanted).map(|(_, text)| text.to_string()).or_else(|| kept_at(key).and_then(|path| std::fs::read_to_string(path).ok()))
            } else {
                std::fs::read_to_string(key).ok()
            };
            match text.and_then(|text| serde_json::from_str::<Value>(strip_comments(&text).as_str()).ok()) {
                Some(value) => {
                    self.docs.insert(key.to_string(), value);
                }
                None => {
                    self.missing.push(key.to_string());
                    return None;
                }
            }
        }
        self.docs.get(key)
    }

    /// The schema `reference` names from the document at `base`, and that document's key.
    fn resolve(&mut self, base: &str, reference: &str) -> Option<(String, Value)> {
        let (target, fragment) = reference.split_once('#').unwrap_or((reference, ""));
        let key = if target.is_empty() { base.to_string() } else { join(base, target) };
        let doc = self.load(&key)?.clone();
        let found = if fragment.is_empty() {
            doc
        } else if fragment.starts_with('/') {
            doc.pointer(&percent_decoded(fragment))?.clone()
        } else {
            anchored(&doc, fragment)?.clone()
        };
        Some((key, found))
    }
}

/// A fragment of a reference with its `%XX` escapes read.
fn percent_decoded(text: &str) -> String {
    let mut out = Vec::new();
    let bytes = text.as_bytes();
    let mut at = 0;
    while at < bytes.len() {
        if bytes[at] == b'%' && at + 2 < bytes.len() + 1 {
            if let Some(byte) = text.get(at + 1..at + 3).and_then(|hex| u8::from_str_radix(hex, 16).ok()) {
                out.push(byte);
                at += 3;
                continue;
            }
        }
        out.push(bytes[at]);
        at += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

/// The schema in `doc` whose `$anchor`, or whose `$id` or `id` of `#name`, is `name`.
fn anchored<'a>(doc: &'a Value, name: &str) -> Option<&'a Value> {
    match doc {
        Value::Object(map) => {
            let hash = format!("#{name}");
            if map.get("$anchor").and_then(Value::as_str) == Some(name) || map.get("$id").or_else(|| map.get("id")).and_then(Value::as_str) == Some(hash.as_str()) {
                return Some(doc);
            }
            map.values().find_map(|value| anchored(value, name))
        }
        Value::Array(items) => items.iter().find_map(|value| anchored(value, name)),
        _ => None,
    }
}

/// `target` read from `base`: a URL as it stands, and otherwise beside the base, a URL's or a file's.
fn join(base: &str, target: &str) -> String {
    if target.contains("://") {
        return target.to_string();
    }
    if base.contains("://") {
        let folder = base.rsplit_once('/').map_or(base, |(folder, _)| folder);
        let mut parts: Vec<&str> = folder.split('/').collect();
        for part in target.split('/') {
            match part {
                "." => {}
                ".." => {
                    if parts.len() > 3 {
                        parts.pop();
                    }
                }
                other => parts.push(other),
            }
        }
        return parts.join("/");
    }
    let folder = Path::new(base).parent().unwrap_or(Path::new(""));
    let joined = folder.join(target.strip_prefix("file://").unwrap_or(target));
    dunce::canonicalize(&joined).unwrap_or(joined).display().to_string()
}

/// JSON's text with its comments taken out, each by spaces, for serde_json to read a schema that
/// holds comments.
fn strip_comments(text: &str) -> String {
    let bytes = text.as_bytes();
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    let mut quoted = false;
    while at < bytes.len() {
        let rest = &text[at..];
        if quoted {
            let char = rest.chars().next().unwrap_or(' ');
            out.push(char);
            at += char.len_utf8();
            if char == '\\' {
                if let Some(next) = text[at..].chars().next() {
                    out.push(next);
                    at += next.len_utf8();
                }
            } else if char == '"' {
                quoted = false;
            }
        } else if rest.starts_with("//") {
            let end = rest.find('\n').unwrap_or(rest.len());
            out.extend(std::iter::repeat_n(' ', end));
            at += end;
        } else if let Some(inside) = rest.strip_prefix("/*") {
            let end = inside.find("*/").map_or(rest.len(), |end| end + 4);
            out.extend(rest[..end].chars().map(|char| if char == '\n' { '\n' } else { ' ' }));
            at += end;
        } else {
            let char = rest.chars().next().unwrap_or(' ');
            quoted = char == '"';
            out.push(char);
            at += char.len_utf8();
        }
    }
    out
}

/// Where a file's schema is: its `$schema`, read beside the file where it is a path, or the one its
/// name is known by; with the span of the `$schema` value where it names one.
fn schema_of(path: &Path, root: Option<&Node>) -> Option<(String, Option<(usize, usize)>)> {
    if let Some(Node { value: Json::Object(members), .. }) = root {
        if let Some(member) = members.iter().find(|member| member.key == "$schema") {
            if let Some(Node { value: Json::Text(named), from, to }) = &member.value {
                let key = if named.contains("://") && !named.starts_with("file://") { named.clone() } else { join(&path.display().to_string(), named) };
                return Some((key, Some((*from, *to))));
            }
        }
    }
    let name = path.file_name()?.to_string_lossy().to_lowercase();
    KNOWN.iter().find(|(known, _)| *known == name).map(|(_, url)| (url.to_string(), None))
}

// The check.

struct Check<'a> {
    store: Store,
    lines: &'a Lines<'a>,
    text: &'a str,
    found: Vec<Finding>,
}

impl Check<'_> {
    /// Checks `node` against `schema`, read in the document at `base`; `key` is the span of the key
    /// that holds the node, where one does.
    fn node(&mut self, node: &Node, schema: &Value, base: &str, key: Option<(usize, usize)>, depth: usize) {
        if depth > DEEPEST {
            return;
        }
        let map = match schema {
            Value::Bool(true) => return,
            Value::Bool(false) => {
                let (from, to) = key.unwrap_or((node.from, node.to));
                self.found.push(Finding::new(self.lines, from, to, WARNING, "schema", "The schema allows no value here".to_string()));
                return;
            }
            Value::Object(map) => map,
            _ => return,
        };
        let (from, to) = (node.from, node.to);
        let base = map.get("$id").and_then(Value::as_str).filter(|id| id.contains("://")).map_or(base.to_string(), |id| id.trim_end_matches('#').to_string());
        if let Some(reference) = map.get("$ref").and_then(Value::as_str) {
            if let Some((doc, found)) = self.store.resolve(&base, reference) {
                self.node(node, &found, &doc, key, depth + 1);
            }
        }
        if map.get("deprecated") == Some(&Value::Bool(true)) {
            if let Some((key_from, key_to)) = key {
                let said = map.get("deprecationMessage").and_then(Value::as_str).map_or(String::new(), |said| format!(": {said}"));
                self.found.push(Finding::new(self.lines, key_from, key_to, NOTE, "schema-deprecated", format!("{} is deprecated{said}", &self.text[key_from..key_to])));
            }
        }
        if let Some(types) = map.get("type") {
            let allowed: Vec<&str> = match types {
                Value::String(one) => vec![one.as_str()],
                Value::Array(all) => all.iter().filter_map(Value::as_str).collect(),
                _ => Vec::new(),
            };
            let kind = node.kind();
            let fits = allowed.is_empty() || allowed.iter().any(|one| *one == kind || (*one == "number" && kind == "integer"));
            if !fits {
                self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-type", format!("A value of type {} stands here, where the schema asks for {}", kind_said(kind), allowed.iter().map(|one| kind_said(one)).collect::<Vec<_>>().join(" or "))));
                return;
            }
        }
        let plain = node.plain();
        if let Some(Value::Array(choices)) = map.get("enum") {
            if !choices.iter().any(|choice| same(choice, &plain)) {
                let mut finding = Finding::new(self.lines, from, to, WARNING, "schema-enum", format!("{} is none of the values the schema allows: {}", shown(&plain), choices.iter().take(12).map(shown).collect::<Vec<_>>().join(", ")));
                if let Value::String(given) = &plain {
                    let strings: Vec<&str> = choices.iter().filter_map(Value::as_str).collect();
                    for near in nearest(given, strings.into_iter()) {
                        finding.fixes.push(edit_fix(format!("Change to \"{near}\""), self.lines, from, to, &Value::String(near.clone()).to_string()));
                    }
                }
                self.found.push(finding);
            }
        }
        if let Some(wanted) = map.get("const") {
            if !same(wanted, &plain) {
                self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-const", format!("The schema asks for {} here", shown(wanted))));
            }
        }
        match &node.value {
            Json::Number(number) => self.number(*number, map, from, to),
            Json::Text(text) => self.text_value(text, map, from, to),
            Json::Array(items) => self.array(node, items, map, &base, depth),
            Json::Object(members) => self.object(node, members, map, &base, key, depth),
            _ => {}
        }
        if let Some(Value::Array(all)) = map.get("allOf") {
            for one in all {
                self.node(node, one, &base, key, depth + 1);
            }
        }
        for (word, exactly) in [("anyOf", false), ("oneOf", true)] {
            if let Some(Value::Array(choices)) = map.get(word) {
                self.choose(node, choices, &base, key, depth, exactly);
            }
        }
        if let Some(not) = map.get("not") {
            if self.fits(node, not, &base, depth) {
                self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-not", "The schema rules this value out".to_string()));
            }
        }
        if let Some(condition) = map.get("if") {
            let branch = if self.fits(node, condition, &base, depth) { map.get("then") } else { map.get("else") };
            if let Some(branch) = branch {
                self.node(node, branch, &base, key, depth + 1);
            }
        }
    }

    /// Whether `node` fits `schema`, its findings set aside.
    fn fits(&mut self, node: &Node, schema: &Value, base: &str, depth: usize) -> bool {
        self.trial(node, schema, base, depth).is_empty()
    }

    /// The findings of `node` against `schema`, taken out of the check's own.
    fn trial(&mut self, node: &Node, schema: &Value, base: &str, depth: usize) -> Vec<Finding> {
        let kept = std::mem::take(&mut self.found);
        self.node(node, schema, base, None, depth + 1);
        let tried = std::mem::replace(&mut self.found, kept);
        tried.into_iter().filter(|one| one.severity != NOTE).collect()
    }

    /// `anyOf` and `oneOf`: the value fits one of the choices, or exactly one; where it fits none,
    /// the findings of the choice it comes nearest are given.
    fn choose(&mut self, node: &Node, choices: &[Value], base: &str, key: Option<(usize, usize)>, depth: usize, exactly: bool) {
        let tried: Vec<Vec<Finding>> = choices.iter().map(|choice| self.trial(node, choice, base, depth)).collect();
        let fitting = tried.iter().filter(|one| one.is_empty()).count();
        if fitting == 0 {
            if let Some(nearest) = tried.into_iter().min_by_key(Vec::len) {
                self.found.extend(nearest);
            }
        } else if exactly && fitting > 1 {
            let (from, to) = key.unwrap_or((node.from, node.to));
            self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-one-of", format!("This value fits {fitting} of the shapes the schema allows, where it asks for exactly one")));
        }
    }

    fn number(&mut self, number: f64, map: &serde_json::Map<String, Value>, from: usize, to: usize) {
        let get = |word: &str| map.get(word).and_then(Value::as_f64);
        let draft4 = |word: &str| map.get(word) == Some(&Value::Bool(true));
        let shown = |value: f64| Value::from(value).to_string().trim_end_matches(".0").to_string();
        let mut said = None;
        if let Some(least) = get("minimum") {
            if number < least || (draft4("exclusiveMinimum") && number == least) {
                said = Some(format!("{} is below the least the schema allows, {}{}", shown(number), shown(least), if draft4("exclusiveMinimum") { ", itself left out" } else { "" }));
            }
        }
        if let Some(most) = get("maximum") {
            if number > most || (draft4("exclusiveMaximum") && number == most) {
                said = Some(format!("{} is above the most the schema allows, {}{}", shown(number), shown(most), if draft4("exclusiveMaximum") { ", itself left out" } else { "" }));
            }
        }
        if let Some(least) = get("exclusiveMinimum") {
            if number <= least {
                said = Some(format!("{} is not above {}, as the schema asks", shown(number), shown(least)));
            }
        }
        if let Some(most) = get("exclusiveMaximum") {
            if number >= most {
                said = Some(format!("{} is not below {}, as the schema asks", shown(number), shown(most)));
            }
        }
        if let Some(step) = get("multipleOf").filter(|step| *step > 0.0) {
            let ratio = number / step;
            if (ratio - ratio.round()).abs() > 1e-9 {
                said = Some(format!("{} is no multiple of {}", shown(number), shown(step)));
            }
        }
        if let Some(said) = said {
            self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-range", said));
        }
    }

    fn text_value(&mut self, text: &str, map: &serde_json::Map<String, Value>, from: usize, to: usize) {
        let length = text.chars().count() as u64;
        if let Some(least) = map.get("minLength").and_then(Value::as_u64).filter(|least| length < *least) {
            self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-length", format!("This string is {length} long, shorter than the {least} the schema asks for")));
        }
        if let Some(most) = map.get("maxLength").and_then(Value::as_u64).filter(|most| length > *most) {
            self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-length", format!("This string is {length} long, longer than the {most} the schema allows")));
        }
        if let Some(pattern) = map.get("pattern").and_then(Value::as_str) {
            if matches(pattern, text) == Some(false) {
                self.found.push(Finding::new(self.lines, from, to, WARNING, "schema-pattern", format!("This string does not match the schema's pattern `{pattern}`")));
            }
        }
    }

    fn array(&mut self, node: &Node, items: &[Node], map: &serde_json::Map<String, Value>, base: &str, depth: usize) {
        let from = node.from;
        let count = items.len() as u64;
        if let Some(least) = map.get("minItems").and_then(Value::as_u64).filter(|least| count < *least) {
            self.found.push(Finding::new(self.lines, from, from + 1, WARNING, "schema-items", format!("This array holds {count}, fewer than the {least} items the schema asks for")));
        }
        if let Some(most) = map.get("maxItems").and_then(Value::as_u64).filter(|most| count > *most) {
            self.found.push(Finding::new(self.lines, from, from + 1, WARNING, "schema-items", format!("This array holds {count}, more than the {most} items the schema allows")));
        }
        if map.get("uniqueItems") == Some(&Value::Bool(true)) {
            let plain: Vec<Value> = items.iter().map(Node::plain).collect();
            for (index, item) in items.iter().enumerate() {
                if plain[..index].iter().any(|earlier| same(earlier, &plain[index])) {
                    self.found.push(Finding::new(self.lines, item.from, item.to, WARNING, "schema-unique", "This item is in the array above it, where the schema asks for each once".to_string()));
                }
            }
        }
        let tuple: Option<&Vec<Value>> = match (map.get("prefixItems"), map.get("items")) {
            (Some(Value::Array(prefix)), _) => Some(prefix),
            (None, Some(Value::Array(prefix))) => Some(prefix),
            _ => None,
        };
        let rest = if map.get("prefixItems").is_some() { map.get("items") } else if tuple.is_some() { map.get("additionalItems") } else { map.get("items") };
        for (index, item) in items.iter().enumerate() {
            match tuple.and_then(|tuple| tuple.get(index)) {
                Some(schema) => self.node(item, schema, base, None, depth + 1),
                None => {
                    if let Some(schema) = rest {
                        if schema == &Value::Bool(false) {
                            self.found.push(Finding::new(self.lines, item.from, item.to, WARNING, "schema-items", format!("The schema allows {} items here, and no more", tuple.map_or(0, Vec::len))));
                        } else {
                            self.node(item, schema, base, None, depth + 1);
                        }
                    }
                }
            }
        }
        if let Some(contains) = map.get("contains") {
            let held = items.iter().filter(|item| self.fits(item, contains, base, depth)).count() as u64;
            let least = map.get("minContains").and_then(Value::as_u64).unwrap_or(1);
            if held < least {
                self.found.push(Finding::new(self.lines, from, from + 1, WARNING, "schema-contains", format!("This array holds {held} of the items the schema's `contains` asks for, fewer than {least}")));
            }
            if let Some(most) = map.get("maxContains").and_then(Value::as_u64).filter(|most| held > *most) {
                self.found.push(Finding::new(self.lines, from, from + 1, WARNING, "schema-contains", format!("This array holds {held} of the items the schema's `contains` asks for, more than {most}")));
            }
        }
    }

    fn object(&mut self, node: &Node, members: &[Member], map: &serde_json::Map<String, Value>, base: &str, key: Option<(usize, usize)>, depth: usize) {
        let at = key.unwrap_or((node.from, node.from + 1));
        let count = members.len() as u64;
        if let Some(least) = map.get("minProperties").and_then(Value::as_u64).filter(|least| count < *least) {
            self.found.push(Finding::new(self.lines, at.0, at.1, WARNING, "schema-properties", format!("This object holds {count}, fewer than the {least} properties the schema asks for")));
        }
        if let Some(most) = map.get("maxProperties").and_then(Value::as_u64).filter(|most| count > *most) {
            self.found.push(Finding::new(self.lines, at.0, at.1, WARNING, "schema-properties", format!("This object holds {count}, more than the {most} properties the schema allows")));
        }
        let has = |name: &str| members.iter().any(|member| member.key == name);
        if let Some(Value::Array(required)) = map.get("required") {
            let missing: Vec<&str> = required.iter().filter_map(Value::as_str).filter(|name| !has(name)).collect();
            if !missing.is_empty() {
                self.found.push(Finding::new(self.lines, at.0, at.1, WARNING, "schema-required", format!("This object has no {}, which the schema asks for", missing.iter().map(|name| format!("`{name}`")).collect::<Vec<_>>().join(", "))));
            }
        }
        let mut dependent: Vec<(&String, &Value)> = Vec::new();
        for word in ["dependencies", "dependentRequired", "dependentSchemas"] {
            if let Some(Value::Object(all)) = map.get(word) {
                dependent.extend(all.iter());
            }
        }
        for (name, needs) in dependent {
            let Some(member) = members.iter().find(|member| &member.key == name) else {
                continue;
            };
            match needs {
                Value::Array(names) => {
                    let missing: Vec<&str> = names.iter().filter_map(Value::as_str).filter(|one| !has(one)).collect();
                    if !missing.is_empty() {
                        self.found.push(Finding::new(self.lines, member.from, member.to, WARNING, "schema-required", format!("{} asks for {} beside it, which this object has not", &self.text[member.from..member.to], missing.iter().map(|one| format!("`{one}`")).collect::<Vec<_>>().join(", "))));
                    }
                }
                schema => self.node(node, schema, base, key, depth + 1),
            }
        }
        let properties = map.get("properties").and_then(Value::as_object);
        let patterns = map.get("patternProperties").and_then(Value::as_object);
        let additional = map.get("additionalProperties");
        for member in members {
            if let Some(names) = map.get("propertyNames") {
                let name = Node { value: Json::Text(member.key.clone()), from: member.from, to: member.to };
                self.node(&name, names, base, None, depth + 1);
            }
            let mut matched = false;
            if let Some(schema) = properties.and_then(|all| all.get(&member.key)) {
                matched = true;
                if let Some(value) = &member.value {
                    self.node(value, schema, base, Some((member.from, member.to)), depth + 1);
                }
            }
            for (pattern, schema) in patterns.into_iter().flatten() {
                if matches(pattern, &member.key) == Some(true) {
                    matched = true;
                    if let Some(value) = &member.value {
                        self.node(value, schema, base, Some((member.from, member.to)), depth + 1);
                    }
                }
            }
            if matched {
                continue;
            }
            match additional {
                Some(Value::Bool(false)) => {
                    let mut finding = Finding::new(self.lines, member.from, member.to, WARNING, "schema-property", format!("{} is no property the schema gives this object", &self.text[member.from..member.to]));
                    for near in nearest(&member.key, properties.into_iter().flat_map(|all| all.keys().map(String::as_str))) {
                        finding.fixes.push(edit_fix(format!("Change to \"{near}\""), self.lines, member.from, member.to, &Value::String(near.clone()).to_string()));
                    }
                    self.found.push(finding);
                }
                Some(schema) => {
                    if let Some(value) = &member.value {
                        self.node(value, schema, base, Some((member.from, member.to)), depth + 1);
                    }
                }
                None => {}
            }
        }
    }
}

/// Whether `pattern`, as ECMAScript writes one, matches somewhere in `text`; None where orior does
/// not read the pattern.
fn matches(pattern: &str, text: &str) -> Option<bool> {
    let compiled = crate::regexp::Regexp::new(pattern, "u").ok()?;
    let chars: Vec<char> = text.chars().collect();
    Some((0..=chars.len()).any(|at| compiled.match_at(&chars, at).is_some()))
}

/// A type's name as a message says it.
fn kind_said(kind: &str) -> String {
    match kind {
        "object" | "array" | "integer" => format!("an {kind}"),
        "null" => "`null`".to_string(),
        other => format!("a {other}"),
    }
}

/// A value as a message shows it, cut short where it is long.
fn shown(value: &Value) -> String {
    let text = value.to_string();
    if text.chars().count() > 40 {
        format!("`{}…`", text.chars().take(40).collect::<String>())
    } else {
        format!("`{text}`")
    }
}

/// The diagnostics of the JSON file at `path`, its text `text`: what kept it from reading, and what
/// its schema finds, in the protocol's form.
pub fn check(root: &Path, path: &Path, text: &str) -> Vec<Value> {
    if text.len() > LARGEST {
        return Vec::new();
    }
    let lines = Lines::new(text);
    let parsed = read(text);
    let mut found: Vec<Finding> = parsed.errors.iter().map(|(from, to, message)| Finding::new(&lines, *from, *to, ERROR, "json", message.clone())).collect();
    if let (Some(node), Some((key, named))) = (parsed.root.as_ref(), schema_of(path, parsed.root.as_ref())) {
        let mut check = Check { store: Store::default(), lines: &lines, text, found: Vec::new() };
        match check.store.load(&key).cloned() {
            Some(schema) => check.node(node, &schema, &key, None, 0),
            None => {
                if let Some((from, to)) = named {
                    let mut finding = Finding::new(&lines, from, to, NOTE, "schema-missing", format!("orior holds no schema at {}", &text[from..to]));
                    if key.starts_with("http") {
                        if let Some(place) = kept_at(&key) {
                            let place = place.display().to_string().replace('\\', "/");
                            finding.fixes.push(run_fix(format!("Fetch the schema to {place}"), crate::builds::inside(root, root), format!("curl -fsSL --create-dirs -o '{place}' '{key}'")));
                        }
                    }
                    found.push(finding);
                }
            }
        }
        found.extend(check.found);
    }
    found.iter().map(Finding::value).collect()
}

// Completion.

/// A step into a JSON value: an object's key or an array's index.
#[derive(Clone, Debug, PartialEq)]
enum Step {
    Key(String),
    Index(usize),
}

/// What stands open at a place of a JSON text: each object and array, the keys an object holds
/// before the place, and the key it is at.
#[derive(Debug)]
enum Frame {
    Object { key: Option<String>, colon: bool, keys: Vec<String> },
    Array { index: usize },
}

/// Where the cursor stands: the steps to the value it is in, whether it is at a key, and whether it
/// is inside a string.
struct Spot {
    steps: Vec<Step>,
    at_key: bool,
    in_string: bool,
    keys: Vec<String>,
}

fn spot(text: &str) -> Option<Spot> {
    let mut frames: Vec<Frame> = Vec::new();
    let mut steps: Vec<Step> = Vec::new();
    let bytes = text.as_bytes();
    let mut at = 0;
    let mut in_string = false;
    while at < bytes.len() {
        let rest = &text[at..];
        if rest.starts_with("//") {
            at = rest.find('\n').map_or(bytes.len(), |end| at + end);
            continue;
        }
        if let Some(inside) = rest.strip_prefix("/*") {
            at = inside.find("*/").map_or(bytes.len(), |end| at + 2 + end + 2);
            continue;
        }
        match bytes[at] {
            b'"' => {
                let mut end = at + 1;
                let mut closed = false;
                while end < bytes.len() {
                    match bytes[end] {
                        b'\\' => end += 2,
                        b'"' => {
                            closed = true;
                            break;
                        }
                        _ => end += 1,
                    }
                }
                if !closed {
                    in_string = true;
                    break;
                }
                let inner = &text[at + 1..end];
                if let Some(Frame::Object { key, colon: false, .. }) = frames.last_mut() {
                    *key = Some(inner.to_string());
                }
                at = end + 1;
                continue;
            }
            b':' => {
                if let Some(Frame::Object { colon, .. }) = frames.last_mut() {
                    *colon = true;
                }
            }
            b',' => match frames.last_mut() {
                Some(Frame::Object { key, colon, keys }) => {
                    if let Some(done) = key.take() {
                        keys.push(done);
                    }
                    *colon = false;
                }
                Some(Frame::Array { index }) => *index += 1,
                None => {}
            },
            b'{' | b'[' => {
                match frames.last() {
                    Some(Frame::Object { key: Some(key), colon: true, .. }) => steps.push(Step::Key(key.clone())),
                    Some(Frame::Array { index }) => steps.push(Step::Index(*index)),
                    _ => {}
                }
                frames.push(if bytes[at] == b'{' { Frame::Object { key: None, colon: false, keys: Vec::new() } } else { Frame::Array { index: 0 } });
            }
            b'}' | b']' => {
                frames.pop();
                if !frames.is_empty() {
                    steps.pop();
                }
            }
            _ => {}
        }
        at += 1;
    }
    match frames.last()? {
        Frame::Object { key, colon, keys } => {
            let mut steps = steps;
            if *colon {
                steps.push(Step::Key(key.clone().unwrap_or_default()));
            }
            Some(Spot { steps, at_key: !colon, in_string, keys: keys.clone() })
        }
        Frame::Array { index } => {
            let mut steps = steps;
            steps.push(Step::Index(*index));
            Some(Spot { steps, at_key: false, in_string, keys: Vec::new() })
        }
    }
}

/// The schemas that hold at the end of `steps` from `schema`, each with the document it is read in:
/// `$ref`s followed, `allOf`, `anyOf`, `oneOf` and `if`'s branches taken together.
fn schemas_at(store: &mut Store, schema: &Value, base: &str, steps: &[Step], depth: usize) -> Vec<(Value, String)> {
    if depth > DEEPEST {
        return Vec::new();
    }
    let Value::Object(map) = schema else {
        return Vec::new();
    };
    let mut found = Vec::new();
    if let Some(reference) = map.get("$ref").and_then(Value::as_str) {
        if let Some((doc, target)) = store.resolve(base, reference) {
            found.extend(schemas_at(store, &target, &doc, steps, depth + 1));
        }
    }
    for word in ["allOf", "anyOf", "oneOf"] {
        if let Some(Value::Array(all)) = map.get(word) {
            for one in all {
                found.extend(schemas_at(store, one, base, steps, depth + 1));
            }
        }
    }
    for word in ["then", "else"] {
        if let Some(one) = map.get(word) {
            found.extend(schemas_at(store, one, base, steps, depth + 1));
        }
    }
    let Some((step, rest)) = steps.split_first() else {
        found.push((schema.clone(), base.to_string()));
        return found;
    };
    let next: Vec<&Value> = match step {
        Step::Key(key) => {
            let mut next: Vec<&Value> = map.get("properties").and_then(|all| all.get(key)).into_iter().collect();
            if let Some(Value::Object(patterns)) = map.get("patternProperties") {
                next.extend(patterns.iter().filter(|(pattern, _)| matches(pattern, key) == Some(true)).map(|(_, schema)| schema));
            }
            if next.is_empty() {
                next.extend(map.get("additionalProperties").filter(|schema| schema.is_object()));
            }
            next
        }
        Step::Index(index) => {
            let tuple = map.get("prefixItems").or_else(|| map.get("items").filter(|items| items.is_array()));
            match tuple.and_then(|tuple| tuple.get(*index)) {
                Some(one) => vec![one],
                None => map.get("items").filter(|items| items.is_object()).or_else(|| map.get("additionalItems")).into_iter().collect(),
            }
        }
    };
    for one in next {
        found.extend(schemas_at(store, one, base, rest, depth + 1));
    }
    found
}

/// The first line of a schema's description, or its type, for a completion's detail.
fn detail_of(schema: &Value) -> String {
    let described = schema.get("description").or_else(|| schema.get("title")).and_then(Value::as_str).map(|said| said.lines().next().unwrap_or_default().to_string());
    described.or_else(|| schema.get("type").map(|kind| match kind {
        Value::String(one) => one.clone(),
        other => other.to_string(),
    })).unwrap_or_default()
}

/// What completes the word before `line`, `col` of the JSON file at `path`, its text `text`: the keys
/// its schema gives the object the cursor is in and that it does not hold yet, or the values the
/// schema allows the value the cursor is at.
pub fn complete(path: &Path, text: &str, line: u32, col: u32) -> Vec<Item> {
    let at = crate::builds::byte_at(text, line, col);
    let Some(here) = spot(&text[..at]) else {
        return Vec::new();
    };
    let parsed = read(text);
    let Some((key, _)) = schema_of(path, parsed.root.as_ref()) else {
        return Vec::new();
    };
    let mut store = Store::default();
    let Some(schema) = store.load(&key).cloned() else {
        return Vec::new();
    };
    let token = crate::builds::run_before(text, at, |char| char.is_alphanumeric() || "_$-.@/".contains(char));
    if here.at_key {
        let mut values: Vec<(String, &'static str, String, Value)> = Vec::new();
        for (one, _) in schemas_at(&mut store, &schema, &key, &here.steps, 0) {
            for (name, property) in one.get("properties").and_then(Value::as_object).into_iter().flatten() {
                if !here.keys.contains(name) && !values.iter().any(|(known, ..)| known == name) {
                    values.push((name.clone(), "field", detail_of(property), property.clone()));
                }
            }
        }
        return values
            .into_iter()
            .filter(|(name, ..)| name.starts_with(&token[..token.len() - token.chars().rev().take_while(|char| crate::builds::word_char(*char)).map(char::len_utf8).sum::<usize>()]))
            .map(|(name, kind, detail, property)| {
                if here.in_string {
                    let base = &token[..token.len() - token.chars().rev().take_while(|char| crate::builds::word_char(*char)).map(char::len_utf8).sum::<usize>()];
                    let rest = name[base.len()..].to_string();
                    return Item { label: rest.clone(), kind, detail, insert: rest, snippet: false };
                }
                let shape = match property.get("type").and_then(Value::as_str) {
                    Some("string") => "\"$0\"",
                    Some("object") => "{$0}",
                    Some("array") => "[$0]",
                    _ => "$0",
                };
                Item { label: name.clone(), kind, detail, insert: format!("{}: {shape}", Value::String(name).to_string().replace('$', "\\$")), snippet: true }
            })
            .collect();
    }
    let mut values: Vec<(String, &'static str, String)> = Vec::new();
    let mut add = |value: &Value, detail: String| {
        let text = if here.in_string { value.as_str().map(str::to_string) } else { Some(value.to_string()) };
        if let Some(text) = text {
            if !values.iter().any(|(known, ..)| *known == text) {
                values.push((text, "value", detail));
            }
        }
    };
    for (one, _) in schemas_at(&mut store, &schema, &key, &here.steps, 0) {
        let detail = detail_of(&one);
        for choice in one.get("enum").and_then(Value::as_array).into_iter().flatten() {
            add(choice, detail.clone());
        }
        for given in one.get("const").into_iter().chain(one.get("default")).chain(one.get("examples").and_then(Value::as_array).into_iter().flatten()) {
            add(given, detail.clone());
        }
        let types: Vec<&str> = match one.get("type") {
            Some(Value::String(kind)) => vec![kind.as_str()],
            Some(Value::Array(all)) => all.iter().filter_map(Value::as_str).collect(),
            _ => Vec::new(),
        };
        if types.contains(&"boolean") {
            add(&Value::Bool(true), detail.clone());
            add(&Value::Bool(false), detail.clone());
        }
        if types.contains(&"null") {
            add(&Value::Null, detail.clone());
        }
    }
    relative(token, values)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tree(name: &str, files: &[(&str, &str)]) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-schema-{}-{name}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for (name, text) in files {
            let path = dir.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        }
        dir
    }

    fn messages(found: &[Value]) -> Vec<String> {
        found.iter().map(|one| format!("{}:{}", one["code"].as_str().unwrap_or_default(), one["message"]["value"].as_str().unwrap_or_default())).collect()
    }

    #[test]
    fn json_reads_with_its_places_and_its_faults() {
        let parsed = read("{\n  \"a\": [1, 2,],\n  \"b\": tru,\n  // a comment\n  \"a\": {}\n}");
        let said: Vec<&str> = parsed.errors.iter().map(|(_, _, message)| message.as_str()).collect();
        assert!(said.iter().any(|one| one.contains("No item follows")), "{said:?}");
        assert!(said.iter().any(|one| one.contains("`tru` is no value")), "{said:?}");
        assert!(said.iter().any(|one| one.contains("\"a\" is a key of this object above it")), "{said:?}");
        assert_eq!(said.len(), 3, "{said:?}");
        let Some(Node { value: Json::Object(members), .. }) = parsed.root else { panic!() };
        assert_eq!(members.len(), 3);
        assert_eq!(members[0].from, 4);
    }

    const SCHEMA: &str = r##"{
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "required": ["name", "version"],
      "additionalProperties": false,
      "definitions": {"level": {"type": "string", "enum": ["low", "medium", "high"], "description": "How loud"}},
      "properties": {
        "$schema": {"type": "string"},
        "name": {"type": "string", "minLength": 2, "description": "The name"},
        "version": {"type": "string", "pattern": "^\\d+\\.\\d+\\.\\d+$"},
        "count": {"type": "integer", "minimum": 1, "maximum": 10},
        "level": {"$ref": "#/definitions/level"},
        "tags": {"type": "array", "items": {"type": "string"}, "uniqueItems": true},
        "old": {"type": "boolean", "deprecated": true},
        "server": {"type": "object", "properties": {"port": {"type": "integer"}, "host": {"type": "string"}}, "required": ["port"]}
      }
    }"##;

    #[test]
    fn a_file_is_checked_against_the_schema_it_names() {
        let dir = tree("checked", &[("schema.json", SCHEMA)]);
        let text = "{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  \"version\": \"1.0\",\n  \"count\": 12,\n  \"level\": \"loud\",\n  \"tags\": [\"a\", \"a\"],\n  \"old\": true,\n  \"server\": {\"host\": 1},\n  \"nmae\": 1\n}\n";
        let found = check(&dir, &dir.join("a.json"), text);
        let said = messages(&found);
        let has = |code: &str, part: &str| said.iter().any(|one| one.starts_with(code) && one.contains(part));
        assert!(has("schema-length", "shorter than the 2"), "{said:?}");
        assert!(has("schema-pattern", "pattern"), "{said:?}");
        assert!(has("schema-range", "12 is above the most the schema allows, 10"), "{said:?}");
        assert!(has("schema-enum", "`\"loud\"`"), "{said:?}");
        assert!(has("schema-unique", "above it"), "{said:?}");
        assert!(has("schema-deprecated", "\"old\" is deprecated"), "{said:?}");
        assert!(has("schema-required", "`port`"), "{said:?}");
        assert!(has("schema-type", "type an integer") || has("schema-type", "a string"), "{said:?}");
        assert!(has("schema-property", "\"nmae\" is no property"), "{said:?}");
        let typo = found.iter().find(|one| one["code"] == "schema-property").unwrap();
        assert_eq!(typo["data"]["orior"][0]["edits"][0]["text"], "\"name\"");
        let loud = found.iter().find(|one| one["code"] == "schema-enum").unwrap();
        assert_eq!(loud["data"]["orior"][0]["title"], "Change to \"low\"");
        assert_eq!(found.len(), 9, "{said:?}");
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn a_schema_is_checked_against_its_own_schema() {
        let text = "{\n  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n  \"type\": \"strin\",\n  \"minLength\": -1,\n  \"required\": [\"a\", \"a\"]\n}\n";
        let found = check(Path::new("/t"), Path::new("/t/s.json"), text);
        let said = messages(&found);
        assert!(said.iter().any(|one| one.contains("\"strin\"")), "{said:?}");
        assert!(said.iter().any(|one| one.contains("-1 is below the least")), "{said:?}");
        assert!(said.iter().any(|one| one.starts_with("schema-unique")), "{said:?}");
        let modern = "{\"$schema\": \"https://json-schema.org/draft/2020-12/schema\", \"prefixItems\": 3}";
        assert!(messages(&check(Path::new("/t"), Path::new("/t/s.json"), modern)).iter().any(|one| one.starts_with("schema-type")));
    }

    #[test]
    fn a_schema_not_held_offers_its_fetch() {
        let text = "{\"$schema\": \"https://example.com/none.json\"}";
        let found = check(Path::new("/t"), Path::new("/t/a.json"), text);
        assert_eq!(found.len(), 1);
        assert_eq!(found[0]["code"], "schema-missing");
        let line = found[0]["data"]["orior"][0]["run"]["line"].as_str().unwrap();
        assert!(line.starts_with("curl -fsSL --create-dirs -o '") && line.ends_with("' 'https://example.com/none.json'"), "{line}");
    }

    #[test]
    fn keys_and_values_complete_from_the_schema() {
        let dir = tree("completed", &[("schema.json", SCHEMA)]);
        let path = dir.join("a.json");
        let labels = |text: &str| {
            let lines = Lines::new(text);
            let place = lines.place(text.len());
            complete(&path, text, place.line, place.col).into_iter().map(|item| (item.label, item.insert)).collect::<Vec<_>>()
        };
        let keys = labels("{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  ");
        assert!(keys.iter().any(|(label, insert)| label == "version" && insert == "\"version\": \"$0\""), "{keys:?}");
        assert!(!keys.iter().any(|(label, _)| label == "name"), "a key the object holds is not offered: {keys:?}");
        let quoted = labels("{\"$schema\": \"./schema.json\", \"ser");
        assert!(quoted.iter().any(|(label, insert)| label == "server" && insert == "server"), "{quoted:?}");
        let inner = labels("{\"$schema\": \"./schema.json\", \"server\": {\"po");
        assert_eq!(inner.iter().map(|(label, _)| label.as_str()).collect::<Vec<_>>(), ["port", "host"]);
        let levels = labels("{\"$schema\": \"./schema.json\", \"level\": \"");
        assert_eq!(levels.iter().map(|(label, _)| label.as_str()).collect::<Vec<_>>(), ["low", "medium", "high"]);
        let flags = labels("{\"$schema\": \"./schema.json\", \"old\": ");
        assert_eq!(flags.iter().map(|(label, _)| label.as_str()).collect::<Vec<_>>(), ["true", "false"]);
        let meta = labels("{\"$schema\": \"http://json-schema.org/draft-07/schema#\", \"type\": \"");
        assert!(meta.iter().any(|(label, _)| label == "object"), "{meta:?}");
        let _ = std::fs::remove_dir_all(dir);
    }
}
