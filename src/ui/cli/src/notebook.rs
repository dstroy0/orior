// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Notebooks as files: a Jupyter notebook's JSON, and a script of `# %%` cells, as Jupytext's
//! percent form writes one. Each reads as the same notebook, a list of cells, each with its kind,
//! its source as one text, its outputs, the count of its last run and what else the file kept of it;
//! and each writes back as it was read.
//!
//! A notebook's JSON is written as Jupyter writes it: its keys in order, one space to each level,
//! and each cell's source and each output's text as lists of lines, which keeps a diff of it to the
//! lines that changed. A script keeps its lines before the first cell, writes a markdown cell's lines
//! behind the language's comment, and holds no outputs.

use std::path::Path;

use serde_json::{json, Map, Value};

/// The comment a script's cells are marked with in the language of `path`, by its extension.
fn comment_of(path: &Path) -> &'static str {
    match path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).as_deref() {
        Some("js" | "ts" | "mjs" | "cs" | "cpp" | "c" | "java" | "kt" | "scala" | "go" | "rs" | "swift") => "//",
        Some("m") => "%",
        Some("lua" | "sql" | "hs") => "--",
        _ => "#",
    }
}

/// Whether a file is a notebook: a .ipynb, or a script whose lines mark cells with `# %%`.
pub fn is_notebook(path: &Path, text: &str) -> bool {
    if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("ipynb")) {
        return true;
    }
    let mark = format!("{} %%", comment_of(path));
    text.lines().any(|line| line.trim_start().starts_with(&mark))
}

/// A cell's source or an output's text as one text, from a list of lines or a text.
fn joined(value: &Value) -> String {
    match value {
        Value::Array(lines) => lines.iter().filter_map(Value::as_str).collect(),
        Value::String(text) => text.clone(),
        _ => String::new(),
    }
}

/// A text as Jupyter keeps it: its lines, each with its line end but the last.
fn lines_of(text: &str) -> Value {
    Value::Array(text.split_inclusive('\n').map(|line| Value::String(line.to_string())).collect())
}

/// Reads the notebook at `path` from its text.
pub fn read(path: &Path, text: &str) -> Result<Value, String> {
    if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("ipynb")) {
        let mut book: Value = serde_json::from_str(text).map_err(|error| format!("{}: {error}", path.display()))?;
        let cells = book["cells"].as_array_mut().ok_or_else(|| format!("{} holds no cells", path.display()))?;
        for cell in cells.iter_mut() {
            cell["source"] = Value::String(joined(&cell["source"]));
            for output in cell.get_mut("outputs").and_then(Value::as_array_mut).into_iter().flatten() {
                if output.get("text").is_some() {
                    output["text"] = Value::String(joined(&output["text"]));
                }
                if let Some(data) = output.get_mut("data").and_then(Value::as_object_mut) {
                    for (kind, value) in data.iter_mut() {
                        if value.is_array() && !kind.ends_with("json") {
                            *value = Value::String(joined(value));
                        }
                    }
                }
            }
        }
        book["format"] = Value::String("ipynb".into());
        return Ok(book);
    }
    let comment = comment_of(path);
    let mark = format!("{comment} %%");
    let mut header = String::new();
    let mut cells: Vec<Value> = Vec::new();
    let mut current: Option<(String, String, Vec<String>)> = None;
    let finish = |current: &mut Option<(String, String, Vec<String>)>, cells: &mut Vec<Value>| {
        if let Some((kind, title, lines)) = current.take() {
            let mut body = lines;
            while body.last().is_some_and(|line| line.trim().is_empty()) {
                body.pop();
            }
            let source = if kind == "markdown" {
                body.iter().map(|line| line.strip_prefix(&format!("{comment} ")).or_else(|| line.strip_prefix(comment)).unwrap_or(line).to_string()).collect::<Vec<_>>().join("\n")
            } else {
                body.join("\n")
            };
            cells.push(json!({"cell_type": kind, "source": source, "metadata": {"title": title}, "outputs": [], "execution_count": null}));
        }
    };
    for line in text.lines() {
        if let Some(rest) = line.trim_start().strip_prefix(&mark) {
            finish(&mut current, &mut cells);
            let rest = rest.trim();
            let (kind, title) = if let Some(at) = rest.find("[markdown]").or_else(|| rest.find("[md]")) {
                ("markdown", format!("{}{}", &rest[..at], &rest[at..].split_once(']').map(|(_, after)| after).unwrap_or_default()).trim().to_string())
            } else if let Some(at) = rest.find("[raw]") {
                ("raw", format!("{}{}", &rest[..at], &rest[at + 5..]).trim().to_string())
            } else {
                ("code", rest.to_string())
            };
            current = Some((kind.to_string(), title, Vec::new()));
        } else if let Some((_, _, lines)) = current.as_mut() {
            lines.push(line.to_string());
        } else {
            header.push_str(line);
            header.push('\n');
        }
    }
    finish(&mut current, &mut cells);
    let language = match comment {
        "//" => "javascript",
        "%" => "matlab",
        "--" => "lua",
        _ => "python",
    };
    Ok(json!({"format": "percent", "header": header, "cells": cells, "metadata": {"language": language}}))
}

/// A value with every object's keys in order, as Jupyter writes them.
fn sorted(value: &Value) -> Value {
    match value {
        Value::Object(map) => {
            let mut keys: Vec<&String> = map.keys().collect();
            keys.sort();
            let mut out = Map::new();
            for key in keys {
                out.insert(key.clone(), sorted(&map[key]));
            }
            Value::Object(out)
        }
        Value::Array(items) => Value::Array(items.iter().map(sorted).collect()),
        other => other.clone(),
    }
}

/// The notebook's text, to write to `path`.
pub fn write(path: &Path, book: &Value) -> Result<String, String> {
    if book["format"] == "percent" {
        let comment = comment_of(path);
        let mut text = book["header"].as_str().unwrap_or_default().to_string();
        for (index, cell) in book["cells"].as_array().into_iter().flatten().enumerate() {
            if index > 0 || !text.is_empty() {
                if !text.ends_with("\n\n") && !text.is_empty() {
                    text.push('\n');
                }
            }
            let title = cell["metadata"]["title"].as_str().unwrap_or_default();
            let kind = cell["cell_type"].as_str().unwrap_or("code");
            let mut mark = format!("{comment} %%");
            if !title.is_empty() {
                mark.push(' ');
                mark.push_str(title);
            }
            if kind == "markdown" || kind == "raw" {
                mark.push_str(&format!(" [{kind}]"));
            }
            text.push_str(&mark);
            text.push('\n');
            let source = cell["source"].as_str().unwrap_or_default();
            for line in source.lines() {
                if kind == "markdown" {
                    text.push_str(&if line.is_empty() { comment.to_string() } else { format!("{comment} {line}") });
                } else {
                    text.push_str(line);
                }
                text.push('\n');
            }
        }
        return Ok(text);
    }
    let mut book = book.clone();
    if let Some(map) = book.as_object_mut() {
        map.remove("format");
    }
    for cell in book["cells"].as_array_mut().into_iter().flatten() {
        cell["source"] = lines_of(&joined(&cell["source"]));
        for output in cell.get_mut("outputs").and_then(Value::as_array_mut).into_iter().flatten() {
            if output.get("text").is_some() {
                output["text"] = lines_of(&joined(&output["text"]));
            }
            if let Some(data) = output.get_mut("data").and_then(Value::as_object_mut) {
                for (kind, value) in data.iter_mut() {
                    if value.is_string() && !kind.ends_with("json") && kind.starts_with("text/") {
                        *value = lines_of(value.as_str().unwrap_or_default());
                    }
                }
            }
        }
    }
    let mut out = Vec::new();
    let mut writer = serde_json::Serializer::with_formatter(&mut out, serde_json::ser::PrettyFormatter::with_indent(b" "));
    serde::Serialize::serialize(&sorted(&book), &mut writer).map_err(|error| error.to_string())?;
    let mut text = String::from_utf8(out).map_err(|error| error.to_string())?;
    text.push('\n');
    Ok(text)
}

/// A notebook's JSON without its outputs and run counts, as a commit that keeps them out writes it.
pub fn without_outputs(text: &str) -> Result<String, String> {
    let mut book: Value = serde_json::from_str(text).map_err(|error| error.to_string())?;
    for cell in book["cells"].as_array_mut().into_iter().flatten() {
        if cell["cell_type"] == "code" {
            cell["outputs"] = json!([]);
            cell["execution_count"] = Value::Null;
        }
    }
    book["format"] = Value::String("ipynb".into());
    write(Path::new("x.ipynb"), &read_json(&book))
}

fn read_json(book: &Value) -> Value {
    let mut book = book.clone();
    for cell in book["cells"].as_array_mut().into_iter().flatten() {
        cell["source"] = Value::String(joined(&cell["source"]));
    }
    book
}

/// A notebook's cells as a diff compares them: each cell's kind, its source and its outputs' text,
/// the run counts, ids and metadata that change on every run left out.
pub fn for_diff(text: &str) -> Result<String, String> {
    let book = read(Path::new("x.ipynb"), text)?;
    let mut out = String::new();
    for (index, cell) in book["cells"].as_array().into_iter().flatten().enumerate() {
        out.push_str(&format!("# cell {} ({})\n", index + 1, cell["cell_type"].as_str().unwrap_or("code")));
        out.push_str(cell["source"].as_str().unwrap_or_default());
        if !out.ends_with('\n') {
            out.push('\n');
        }
        for output in cell["outputs"].as_array().into_iter().flatten() {
            let shown = output["text"].as_str().map(str::to_string).or_else(|| output["data"]["text/plain"].as_str().map(str::to_string)).or_else(|| output["evalue"].as_str().map(|value| format!("{}: {value}", output["ename"].as_str().unwrap_or("error"))));
            let kind = output["output_type"].as_str().unwrap_or("output");
            match shown {
                Some(text) => {
                    out.push_str(&format!("## {kind}\n{text}"));
                    if !out.ends_with('\n') {
                        out.push('\n');
                    }
                }
                None => out.push_str(&format!("## {kind}: {}\n", output["data"].as_object().map(|data| data.keys().cloned().collect::<Vec<_>>().join(", ")).unwrap_or_default())),
            }
        }
    }
    Ok(out)
}

#[cfg(test)]
mod notebooks {
    use super::*;

    const BOOK: &str = "{\n \"cells\": [\n  {\n   \"cell_type\": \"code\",\n   \"execution_count\": 3,\n   \"id\": \"a1\",\n   \"metadata\": {},\n   \"outputs\": [\n    {\n     \"name\": \"stdout\",\n     \"output_type\": \"stream\",\n     \"text\": [\n      \"hi\\n\"\n     ]\n    }\n   ],\n   \"source\": [\n    \"print('hi')\\n\",\n    \"x = 1\"\n   ]\n  },\n  {\n   \"cell_type\": \"markdown\",\n   \"id\": \"b2\",\n   \"metadata\": {},\n   \"source\": [\n    \"# Title\"\n   ]\n  }\n ],\n \"metadata\": {\n  \"kernelspec\": {\n   \"display_name\": \"Python 3\",\n   \"language\": \"python\",\n   \"name\": \"python3\"\n  }\n },\n \"nbformat\": 4,\n \"nbformat_minor\": 5\n}\n";

    #[test]
    fn a_notebook_reads_as_cells_and_writes_back_as_it_was() {
        let book = read(Path::new("a.ipynb"), BOOK).unwrap();
        assert_eq!(book["cells"][0]["source"], "print('hi')\nx = 1");
        assert_eq!(book["cells"][0]["outputs"][0]["text"], "hi\n");
        assert_eq!(write(Path::new("a.ipynb"), &book).unwrap(), BOOK);
    }

    #[test]
    fn a_percent_script_reads_as_cells_and_writes_back_as_it_was() {
        let script = "import os\n\n# %% Load\nx = 1\nprint(x)\n\n# %% [markdown]\n# A note\n#\n# on two lines\n\n# %%\ny = x + 1\n";
        let book = read(Path::new("s.py"), script).unwrap();
        assert_eq!(book["header"], "import os\n\n");
        assert_eq!(book["cells"].as_array().unwrap().len(), 3);
        assert_eq!(book["cells"][0]["metadata"]["title"], "Load");
        assert_eq!(book["cells"][1]["cell_type"], "markdown");
        assert_eq!(book["cells"][1]["source"], "A note\n\non two lines");
        assert_eq!(write(Path::new("s.py"), &book).unwrap(), script);
        assert!(is_notebook(Path::new("s.py"), script));
        assert!(!is_notebook(Path::new("plain.py"), "print(1)\n"));
    }

    #[test]
    fn a_diff_reads_sources_and_outputs_and_no_run_counts() {
        let shown = for_diff(BOOK).unwrap();
        assert_eq!(shown, "# cell 1 (code)\nprint('hi')\nx = 1\n## stream\nhi\n# cell 2 (markdown)\n# Title\n");
        let bare = without_outputs(BOOK).unwrap();
        assert!(bare.contains("\"execution_count\": null") && bare.contains("\"outputs\": []"));
    }
}
