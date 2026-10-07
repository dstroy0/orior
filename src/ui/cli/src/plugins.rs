// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Plugins: what the window takes in from outside its own code. A language plugin is a folder named
//! by its id holding plugin.json:
//!
//!   id, name, version       the plugin's own name for itself, the name it shows, and its version
//!   kind                    "language", or "tool" for a tool plugin, which validate.rs reads
//!   extensions              the file extensions it opens, without the dot
//!   comments                { line, block: [open, close] }, either left out where it has none
//!   pairs, quotes           the brackets it closes and the quotes it pairs
//!   indentAfter             a pattern a line ends with to indent the next deeper
//!   grammar                 lists of words by name and the tokenizer, as editor/tokens.js reads one;
//!                           a pattern is a string, or { pattern, flags } where it takes flags
//!   hovers                  a word and the text hovering it shows, where it has any
//!   snippets                { label, body } for the completion list, `${1:name}` a stop in a body
//!
//! The plugins that come with orior are under src/ui/plugins and built into the program. The
//! reader's are under `plugins/` in orior's own folder, and one of the reader's stands in for one
//! that comes with orior of the same id.
//!
//! The generator writes a language plugin from a few answers: a name, the extensions, how a line and
//! a block are commented, the keywords, types and constants, and the quotes. Given a plugin to start
//! from, it copies that plugin's grammar and changes what the answers name. Beside plugin.json it
//! writes a sample file in the new language to open and try.

use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};

use crate::home;

include!(concat!(env!("OUT_DIR"), "/bundled.rs"));

/// A plugin as found: its id, where it came from, "bundled" or "user", the folder of a reader's own,
/// and the text of its plugin.json.
#[derive(Serialize, Clone, Debug)]
pub struct Plugin {
    pub id: String,
    pub source: &'static str,
    pub folder: Option<String>,
    pub text: String,
}

/// Every plugin: those that come with orior, then the reader's, each in the order of its id.
pub fn all() -> Vec<Plugin> {
    let mut found: Vec<Plugin> = BUNDLED.iter().map(|(id, text)| Plugin { id: id.to_string(), source: "bundled", folder: None, text: text.to_string() }).collect();
    if let Some(dir) = home::plugins() {
        found.extend(user_in(&dir));
    }
    found
}

fn user_in(dir: &Path) -> Vec<Plugin> {
    let mut found = Vec::new();
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let file = entry.path().join("plugin.json");
            if let Ok(text) = std::fs::read_to_string(&file) {
                found.push(Plugin { id: entry.file_name().to_string_lossy().into_owned(), source: "user", folder: Some(entry.path().to_string_lossy().into_owned()), text });
            }
        }
    }
    found.sort_by(|a, b| a.id.cmp(&b.id));
    found
}

/// What the generator is asked for. Every field may be left out.
#[derive(Deserialize, Default, Debug, Clone)]
#[serde(default, rename_all = "camelCase")]
pub struct Spec {
    pub name: String,
    pub id: String,
    pub extensions: Vec<String>,
    pub from: String,
    pub line_comment: String,
    pub block_comment: Vec<String>,
    pub keywords: Vec<String>,
    pub types: Vec<String>,
    pub constants: Vec<String>,
    pub quotes: Vec<String>,
}

/// A name as an id: its letters and digits in lower case, every run of anything else one dash.
pub fn slug(name: &str) -> String {
    let mut id = String::new();
    for c in name.trim().chars() {
        if c.is_ascii_alphanumeric() {
            id.push(c.to_ascii_lowercase());
        } else if !id.ends_with('-') && !id.is_empty() {
            id.push('-');
        }
    }
    id.trim_end_matches('-').to_string()
}

/// `text` as a pattern that matches only `text`.
fn escape(text: &str) -> String {
    let mut out = String::new();
    for c in text.chars() {
        if r"\^$.|?*+()[]{}/".contains(c) {
            out.push('\\');
        }
        out.push(c);
    }
    out
}

const NUMBER: &str = r"0[xX][\da-fA-F_]+|0[bB][01_]+|(?:\d[\d_]*)?\.?\d[\d_]*(?:[eE][+-]?\d+)?";

// The words of a list, trimmed, each once, in the order first given.
fn words(list: &[String]) -> Vec<String> {
    let mut seen = std::collections::HashSet::new();
    list.iter().map(|word| word.trim().to_string()).filter(|word| !word.is_empty() && seen.insert(word.clone())).collect()
}

/// The grammar the answers ask for, made from nothing.
fn grammar_of(spec: &Spec, quotes: &[String]) -> Value {
    let mut grammar = Map::new();
    let mut cases = Map::new();
    for (name, list, token) in [("keywords", &spec.keywords, "keyword"), ("types", &spec.types, "type"), ("constants", &spec.constants, "predefined")] {
        let list = words(list);
        if !list.is_empty() {
            grammar.insert(name.into(), json!(list));
            cases.insert(format!("@{name}"), json!(token));
        }
    }
    cases.insert("@default".into(), json!("identifier"));
    let mut root = vec![json!({ "include": "@whitespace" }), json!([r"[A-Za-z_]\w*", { "cases": cases }]), json!([NUMBER, "number"])];
    let mut tokenizer = Map::new();
    for (at, quote) in quotes.iter().enumerate() {
        let state = format!("string{at}");
        root.push(json!([escape(quote), "string", format!("@{state}")]));
        tokenizer.insert(state, json!([[format!(r"(?:(?!{}|\\).)+", escape(quote)), "string"], [r"\\.", "string.escape"], [escape(quote), "string", "@pop"]]));
    }
    root.push(json!([r"[{}()\[\]]", "@brackets"]));
    root.push(json!([r"[<>=!~?:&|+\-*/^%]+", "operator"]));
    root.push(json!([r"[;,.]", "delimiter"]));
    let mut whitespace = vec![json!([r"\s+", ""])];
    if let [open, close] = spec.block_comment.as_slice() {
        whitespace.push(json!([escape(open), "comment", "@comment"]));
        tokenizer.insert("comment".into(), json!([[format!("(?:(?!{}).)+", escape(close)), "comment"], [escape(close), "comment", "@pop"]]));
    }
    if !spec.line_comment.trim().is_empty() {
        whitespace.push(json!([format!("{}.*$", escape(spec.line_comment.trim())), "comment"]));
    }
    let mut ordered = Map::new();
    ordered.insert("root".into(), json!(root));
    ordered.insert("whitespace".into(), json!(whitespace));
    ordered.extend(tokenizer);
    grammar.insert("tokenizer".into(), Value::Object(ordered));
    Value::Object(grammar)
}

fn comments_of(spec: &Spec) -> Value {
    let mut comments = Map::new();
    if !spec.line_comment.trim().is_empty() {
        comments.insert("line".into(), json!(spec.line_comment.trim()));
    }
    if let [open, close] = spec.block_comment.as_slice() {
        comments.insert("block".into(), json!([open, close]));
    }
    Value::Object(comments)
}

/// The plugin of `id` to start from: the reader's where they have one, else the one that comes with
/// orior.
fn template(id: &str) -> Result<Value, String> {
    let found = all().into_iter().rfind(|plugin| plugin.id == id).ok_or_else(|| format!("no plugin {id} to start from"))?;
    serde_json::from_str(&found.text).map_err(|error| format!("plugin {id} does not read: {error}"))
}

/// The plugin the answers ask for, as JSON.
pub fn draft(spec: &Spec) -> Result<Value, String> {
    let id = slug(if spec.id.trim().is_empty() { &spec.name } else { &spec.id });
    if id.is_empty() {
        return Err("a plugin needs a name made of letters or digits".into());
    }
    let name = if spec.name.trim().is_empty() { id.clone() } else { spec.name.trim().to_string() };
    let extensions: Vec<String> = spec.extensions.iter().map(|ext| ext.trim().trim_start_matches('.').to_ascii_lowercase()).filter(|ext| !ext.is_empty()).collect();
    if extensions.is_empty() {
        return Err("a plugin needs an extension to open, such as --ext foo".into());
    }
    let quotes = if spec.quotes.is_empty() { vec!["\"".to_string(), "'".to_string()] } else { words(&spec.quotes) };
    let mut plugin = Map::new();
    plugin.insert("id".into(), json!(id));
    plugin.insert("name".into(), json!(name));
    plugin.insert("version".into(), json!("1.0.0"));
    plugin.insert("kind".into(), json!("language"));
    plugin.insert("extensions".into(), json!(extensions));
    if spec.from.trim().is_empty() {
        plugin.insert("comments".into(), comments_of(spec));
        plugin.insert("pairs".into(), json!(["()", "[]", "{}"]));
        plugin.insert("quotes".into(), json!(quotes));
        plugin.insert("indentAfter".into(), json!(r"[{[(]\s*$"));
        plugin.insert("grammar".into(), grammar_of(spec, &quotes));
        plugin.insert("hovers".into(), json!({}));
        plugin.insert("snippets".into(), json!([]));
        return Ok(Value::Object(plugin));
    }
    let Value::Object(from) = template(spec.from.trim())? else {
        return Err(format!("plugin {} is not an object", spec.from.trim()));
    };
    for (key, value) in from {
        if !plugin.contains_key(&key) {
            plugin.insert(key, value);
        }
    }
    if !spec.line_comment.trim().is_empty() || spec.block_comment.len() == 2 {
        plugin.insert("comments".into(), comments_of(spec));
    }
    if !spec.quotes.is_empty() {
        plugin.insert("quotes".into(), json!(quotes));
    }
    if let Some(Value::Object(grammar)) = plugin.get_mut("grammar") {
        for (name, list) in [("keywords", &spec.keywords), ("types", &spec.types), ("constants", &spec.constants)] {
            let list = words(list);
            if list.is_empty() {
                continue;
            }
            let key = if name == "keywords" && grammar.contains_key("words") { "words" } else { name };
            grammar.insert(key.into(), json!(list));
        }
    }
    Ok(Value::Object(plugin))
}

/// A plugin as its file holds it: each value on one line where it fits in WIDE characters.
pub fn text_of(value: &Value) -> String {
    let mut out = String::new();
    write_value(&mut out, value, "");
    out.push('\n');
    out
}

const WIDE: usize = 110;

fn write_value(out: &mut String, value: &Value, indent: &str) {
    let flat = value.to_string();
    if flat.len() + indent.len() <= WIDE || !(value.is_array() || value.is_object()) {
        out.push_str(&flat);
        return;
    }
    let inner = format!("{indent}  ");
    match value {
        Value::Array(items) if items.iter().all(Value::is_string) => {
            let mut lines = vec![String::new()];
            for item in items {
                let word = item.to_string();
                let line = lines.last_mut().expect("one line at least");
                if !line.is_empty() && inner.len() + line.len() + word.len() + 2 > WIDE {
                    lines.push(word);
                } else if line.is_empty() {
                    line.push_str(&word);
                } else {
                    line.push_str(", ");
                    line.push_str(&word);
                }
            }
            out.push_str("[\n");
            let count = lines.len();
            for (at, line) in lines.into_iter().enumerate() {
                out.push_str(&inner);
                out.push_str(&line);
                out.push_str(if at + 1 < count { ",\n" } else { "\n" });
            }
            out.push_str(indent);
            out.push(']');
        }
        Value::Array(items) => {
            out.push_str("[\n");
            for (at, item) in items.iter().enumerate() {
                out.push_str(&inner);
                write_value(out, item, &inner);
                out.push_str(if at + 1 < items.len() { ",\n" } else { "\n" });
            }
            out.push_str(indent);
            out.push(']');
        }
        Value::Object(fields) => {
            out.push_str("{\n");
            for (at, (key, item)) in fields.iter().enumerate() {
                out.push_str(&inner);
                out.push_str(&Value::String(key.clone()).to_string());
                out.push_str(": ");
                write_value(out, item, &inner);
                out.push_str(if at + 1 < fields.len() { ",\n" } else { "\n" });
            }
            out.push_str(indent);
            out.push('}');
        }
        _ => out.push_str(&flat),
    }
}

/// A few lines in the new language to open and try: a comment, a keyword, a type, a constant, a
/// string and a number, each where the plugin has one.
fn sample_of(plugin: &Value) -> String {
    let mut lines = Vec::new();
    let comments = &plugin["comments"];
    if let Some(line) = comments["line"].as_str() {
        lines.push(format!("{line} {} opens in this plugin", plugin["name"].as_str().unwrap_or("this file")));
    }
    if let (Some(open), Some(close)) = (comments["block"][0].as_str(), comments["block"][1].as_str()) {
        lines.push(format!("{open} a block comment {close}"));
    }
    let first = |name: &str| plugin["grammar"][name].as_array().and_then(|list| list.first()).and_then(Value::as_str).map(str::to_string);
    let keyword = first("keywords").or_else(|| first("words")).unwrap_or_else(|| "word".into());
    let kind = first("types").unwrap_or_else(|| "Type".into());
    let constant = first("constants").unwrap_or_else(|| "42".into());
    let quote = plugin["quotes"][0].as_str().unwrap_or("\"");
    lines.push(format!("{keyword} {kind} name = {constant};"));
    lines.push(format!("{keyword} text = {quote}a string{quote}, count = 0x2a + 1.5e3;"));
    lines.push(String::new());
    lines.join("\n")
}

/// Writes the plugin the answers ask for under `dir`, in a folder of its id, with a sample file
/// beside it, and gives the folder. A plugin of that id already there is kept unless `replace`.
pub fn create_in(dir: &Path, spec: &Spec, replace: bool) -> Result<PathBuf, String> {
    let plugin = draft(spec)?;
    let id = plugin["id"].as_str().unwrap_or_default().to_string();
    let folder = dir.join(&id);
    let file = folder.join("plugin.json");
    if file.exists() && !replace {
        return Err(format!("a plugin {id} is there already, in {}: give another name, or replace it", folder.display()));
    }
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    std::fs::write(&file, text_of(&plugin)).map_err(|error| format!("{}: {error}", file.display()))?;
    let ext = plugin["extensions"][0].as_str().unwrap_or("txt");
    let sample = folder.join(format!("sample.{ext}"));
    std::fs::write(&sample, sample_of(&plugin)).map_err(|error| format!("{}: {error}", sample.display()))?;
    Ok(folder)
}

/// Writes the plugin under the reader's plugins folder.
pub fn create(spec: &Spec, replace: bool) -> Result<PathBuf, String> {
    let dir = home::plugins().ok_or("orior has no folder of its own to keep plugins in")?;
    create_in(&dir, spec, replace)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spec() -> Spec {
        Spec {
            name: "Toy Lang".into(),
            extensions: vec![".toy".into()],
            line_comment: "--".into(),
            block_comment: vec!["{-".into(), "-}".into()],
            keywords: vec!["let".into(), "in".into(), "let".into()],
            types: vec!["Int".into()],
            constants: vec!["true".into()],
            ..Spec::default()
        }
    }

    #[test]
    fn every_bundled_plugin_reads_and_names_itself() {
        assert!(BUNDLED.len() >= 16);
        for (id, text) in BUNDLED {
            let value: Value = serde_json::from_str(text).unwrap_or_else(|error| panic!("{id}: {error}"));
            assert_eq!(value["id"], json!(id), "{id}");
            match value["kind"].as_str() {
                Some("language") => assert!(value["extensions"].as_array().is_some_and(|list| !list.is_empty()), "{id}"),
                Some("tool") => assert!(value["languages"].as_array().is_some_and(|list| !list.is_empty()), "{id}"),
                other => panic!("{id}: kind {other:?}"),
            }
        }
    }

    #[test]
    fn a_name_becomes_an_id() {
        assert_eq!(slug("Toy Lang"), "toy-lang");
        assert_eq!(slug("  C++ & More!! "), "c-more");
        assert_eq!(slug("!!!"), "");
    }

    #[test]
    fn a_plugin_from_nothing_holds_what_was_asked() {
        let plugin = draft(&spec()).expect("drafts");
        assert_eq!(plugin["id"], json!("toy-lang"));
        assert_eq!(plugin["extensions"], json!(["toy"]));
        assert_eq!(plugin["comments"], json!({ "line": "--", "block": ["{-", "-}"] }));
        assert_eq!(plugin["grammar"]["keywords"], json!(["let", "in"]));
        assert_eq!(plugin["grammar"]["tokenizer"]["whitespace"][1], json!([r"\{-", "comment", "@comment"]));
        assert_eq!(plugin["grammar"]["tokenizer"]["whitespace"][2], json!(["--.*$", "comment"]));
        let back: Value = serde_json::from_str(&text_of(&plugin)).expect("its text reads back");
        assert_eq!(back, plugin);
    }

    #[test]
    fn a_plugin_from_another_takes_its_grammar() {
        let plugin = draft(&Spec { name: "Rusty".into(), extensions: vec!["rsy".into()], from: "rust".into(), keywords: vec!["fn".into()], ..Spec::default() }).expect("drafts");
        assert_eq!(plugin["id"], json!("rusty"));
        assert_eq!(plugin["extensions"], json!(["rsy"]));
        assert_eq!(plugin["grammar"]["words"], json!(["fn"]));
        assert!(plugin["grammar"]["types"].as_array().is_some_and(|list| list.len() > 10));
        assert!(draft(&Spec { name: "x".into(), extensions: vec!["x".into()], from: "nothing-here".into(), ..Spec::default() }).is_err());
    }

    #[test]
    fn creating_writes_once_and_replaces_only_when_asked() {
        let dir = std::env::temp_dir().join(format!("orior-plugins-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let folder = create_in(&dir, &spec(), false).expect("creates");
        assert!(folder.join("plugin.json").is_file());
        let sample = std::fs::read_to_string(folder.join("sample.toy")).expect("a sample");
        assert!(sample.starts_with("-- Toy Lang"));
        assert!(create_in(&dir, &spec(), false).is_err());
        assert!(create_in(&dir, &spec(), true).is_ok());
        assert_eq!(user_in(&dir).len(), 1);
        let _ = std::fs::remove_dir_all(&dir);
    }
}
