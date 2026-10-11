// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What a file is for, before it is opened: what its first comment says, and which files of the tree
//! use it.
//!
//! The first comment is the first that stands before the file's code, read with the comment marks of
//! the file's language as its plugin names them, past a first line of `#!`, and past a comment that
//! states a copyright or a license; a Python module's docstring counts as a comment, and a Markdown
//! file's first paragraph under its headings stands for one. Its lines are joined into sentences, and
//! the first few of them are given.
//!
//! A file uses another where it names the other's file name, as an import of a script, an include
//! of a header, a link or a stylesheet does, or where a line that imports, uses, includes or inputs
//! names the other's name without its extension as a word, as Python's imports, Rust's modules and
//! TeX's inputs do. The files read are the tree's text files, each up to READ bytes.

use std::io::Read;
use std::path::Path;

use serde::Serialize;

use crate::{files, format, plugins, root};

/// The most of a file read for its comment, and of each file read for its uses.
const HEAD: usize = 64 * 1024;
const READ: usize = 512 * 1024;

/// The most characters of a comment given, and the most users named.
const MOST_SAID: usize = 600;
const MOST_USERS: usize = 20;

/// What a file is for: what its first comment says, the files that use it, the first of them, and
/// how many there are in all.
#[derive(Serialize, Debug, Default, PartialEq)]
pub struct Purpose {
    pub said: String,
    pub users: Vec<String>,
    pub all: usize,
}

/// The comment marks of the language of the file at `path`: its line comment's and its block
/// comment's ends, as its plugin names them.
fn marks(path: &Path) -> (Option<String>, Option<(String, String)>) {
    let Some(id) = format::language_of(path) else {
        return (None, None);
    };
    let Some(plugin) = plugins::all().into_iter().find(|one| one.id == id) else {
        return (None, None);
    };
    let Ok(read) = serde_json::from_str::<serde_json::Value>(&plugin.text) else {
        return (None, None);
    };
    let comments = &read["comments"];
    let line = comments["line"].as_str().map(str::to_string);
    let block = match (comments["block"][0].as_str(), comments["block"][1].as_str()) {
        (Some(open), Some(close)) => Some((open.to_string(), close.to_string())),
        _ => None,
    };
    (line, block)
}

fn is_license(text: &str) -> bool {
    let lower = text.to_ascii_lowercase();
    lower.contains("copyright") || lower.contains("spdx-license") || lower.contains("license")
}

/// The first sentences of `text`, its lines joined, up to MOST_SAID characters.
fn sentences(text: &str) -> String {
    let joined = text.split_whitespace().collect::<Vec<_>>().join(" ");
    if joined.chars().count() <= MOST_SAID {
        return joined;
    }
    let cut: String = joined.chars().take(MOST_SAID).collect();
    match cut.rfind(". ") {
        Some(end) if end > MOST_SAID / 3 => cut[..=end].to_string(),
        _ => format!("{}…", cut.trim_end()),
    }
}

/// The text of the comment lines `lines`, each line's marks taken off.
fn uncommented(lines: &[&str], mark: &str) -> String {
    lines
        .iter()
        .map(|line| {
            let rest = line.trim_start().strip_prefix(mark).unwrap_or(line.trim_start());
            rest.trim_start_matches(['!', '/', '*', '#']).trim()
        })
        .collect::<Vec<_>>()
        .join("\n")
}

/// The first paragraph of a Markdown text under its headings, its marks of emphasis and its links'
/// addresses left out.
fn first_paragraph(text: &str) -> String {
    let mut paragraph: Vec<&str> = Vec::new();
    for line in text.lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            if !paragraph.is_empty() {
                break;
            }
            continue;
        }
        if paragraph.is_empty() && (trimmed.starts_with('#') || trimmed.starts_with("![") || trimmed.starts_with("[![") || trimmed.starts_with("<") || trimmed.starts_with("---") || trimmed.starts_with("```")) {
            continue;
        }
        paragraph.push(trimmed);
    }
    let joined = paragraph.join(" ");
    let mut out = String::new();
    let mut rest = joined.as_str();
    while let Some(open) = rest.find("](") {
        let Some(close) = rest[open..].find(')') else {
            break;
        };
        out.push_str(&rest[..open]);
        rest = &rest[open + close + 1..];
    }
    out.push_str(rest);
    out.replace(['[', '*', '`'], "").replace("__", "")
}

/// What the first comment of the text `text` of the file at `path` says, past a license's comment.
pub fn first_comment(path: &Path, text: &str) -> String {
    let extension = path.extension().map(|ext| ext.to_string_lossy().to_ascii_lowercase()).unwrap_or_default();
    if matches!(extension.as_str(), "md" | "markdown" | "mdx" | "rst" | "txt") {
        return sentences(&first_paragraph(text));
    }
    let (line_mark, block) = marks(path);
    let lines: Vec<&str> = text.lines().collect();
    let mut at = usize::from(lines.first().is_some_and(|line| line.starts_with("#!")));
    while at < lines.len() {
        let trimmed = lines[at].trim();
        if trimmed.is_empty() {
            at += 1;
            continue;
        }
        let said = if let Some(mark) = line_mark.as_deref().filter(|mark| trimmed.starts_with(*mark)) {
            let start = at;
            while at < lines.len() && lines[at].trim().starts_with(mark) {
                at += 1;
            }
            uncommented(&lines[start..at], mark)
        } else if let Some((open, close)) = block.as_ref().filter(|(open, _)| trimmed.starts_with(open.as_str())) {
            let rest = lines[at..].join("\n");
            let from = rest.find(open.as_str()).unwrap_or(0) + open.len();
            let body = match rest[from..].find(close.as_str()) {
                Some(end) => {
                    at += rest[..from + end + close.len()].matches('\n').count() + 1;
                    &rest[from..from + end]
                }
                None => {
                    at = lines.len();
                    &rest[from..]
                }
            };
            uncommented(&body.lines().collect::<Vec<_>>(), "*")
        } else if extension == "py" && (trimmed.starts_with("\"\"\"") || trimmed.starts_with("'''") || trimmed.starts_with("r\"\"\"")) {
            let quote = if trimmed.contains("'''") { "'''" } else { "\"\"\"" };
            let body = &text[text.find(quote).map(|from| from + 3).unwrap_or(0)..];
            body.split(quote).next().unwrap_or("").to_string()
        } else {
            return String::new();
        };
        if !said.trim().is_empty() && !is_license(&said) {
            return sentences(&said);
        }
    }
    String::new()
}

/// Whether the text `text` uses the file named `name`, whose name without its extension is `stem`.
pub fn uses(text: &str, name: &str, stem: &str) -> bool {
    let word = |at: usize, length: usize, within: &str| {
        let before = within[..at].chars().next_back();
        let after = within[at + length..].chars().next();
        !before.is_some_and(|c| c.is_alphanumeric() || c == '_' || c == '-') && !after.is_some_and(|c| c.is_alphanumeric() || c == '_')
    };
    if text.match_indices(name).any(|(at, _)| word(at, name.len(), text)) {
        return true;
    }
    if stem.len() < 2 {
        return false;
    }
    const IMPORTS: [&str; 12] = ["import ", "from ", "use ", "mod ", "pub use ", "pub mod ", "require", "#include", "\\input", "\\include", "source ", "load("];
    text.lines().any(|line| {
        let trimmed = line.trim_start();
        IMPORTS.iter().any(|keyword| trimmed.starts_with(keyword)) && line.match_indices(stem).any(|(at, _)| word(at, stem.len(), line))
    })
}

/// What the first comment of the file at `path` of the tree `root` says.
pub fn said(root: &Path, path: &str) -> Result<String, String> {
    let file = root::inside(root, path)?;
    let mut head = Vec::new();
    std::fs::File::open(&file).and_then(|opened| opened.take(HEAD as u64).read_to_end(&mut head)).map_err(|error| format!("{path}: {error}"))?;
    Ok(if head.contains(&0) { String::new() } else { first_comment(&file, &String::from_utf8_lossy(&head)) })
}

/// The files of the tree `root` that use the file at `path`, the first MOST_USERS of them by their
/// paths, and how many there are in all, the tree's files read on every core the machine has.
pub fn users(root: &Path, path: &str) -> Result<(Vec<String>, usize), String> {
    let file = root::inside(root, path)?;
    let name = file.file_name().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
    let stem = file.file_stem().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
    let others: Vec<String> = files::all(root).into_iter().filter(|other| other != path).collect();
    let cores = std::thread::available_parallelism().map(|count| count.get()).unwrap_or(4).min(16);
    let share = others.len().div_ceil(cores.max(1)).max(1);
    let mut found: Vec<String> = std::thread::scope(|scope| {
        let workers: Vec<_> = others
            .chunks(share)
            .map(|part| {
                let (name, stem) = (&name, &stem);
                scope.spawn(move || {
                    part.iter()
                        .filter(|other| {
                            let Ok(opened) = std::fs::File::open(root.join(other)) else {
                                return false;
                            };
                            let mut bytes = Vec::new();
                            opened.take(READ as u64).read_to_end(&mut bytes).is_ok() && !bytes.contains(&0) && uses(&String::from_utf8_lossy(&bytes), name, stem)
                        })
                        .cloned()
                        .collect::<Vec<_>>()
                })
            })
            .collect();
        workers.into_iter().flat_map(|worker| worker.join().unwrap_or_default()).collect()
    });
    found.sort();
    let all = found.len();
    found.truncate(MOST_USERS);
    Ok((found, all))
}

/// What the file at `path` of the tree `root` is for.
pub fn purpose(root: &Path, path: &str) -> Result<Purpose, String> {
    let said = said(root, path)?;
    let (users, all) = users(root, path)?;
    Ok(Purpose { said, users, all })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_first_comment_past_the_license_is_what_the_file_is_for() {
        let js = "// orior - Copyright (C) 2026\n// SPDX-License-Identifier: AGPL-3.0\n\n// The live preview: the file\n// in the editor shown as it reads.\n\nimport x from \"./x.js\";\n// later\n";
        assert_eq!(first_comment(Path::new("a.js"), js), "The live preview: the file in the editor shown as it reads.");
        let rust = "// Copyright\n\n//! Archives as folders: a zip\n//! opens as a folder.\n\nuse std::io;\n";
        assert_eq!(first_comment(Path::new("a.rs"), rust), "Archives as folders: a zip opens as a folder.");
        let python = "#!/usr/bin/env python3\n\"\"\"Rebuilds the oracles\nfrom their scripts.\"\"\"\nimport sys\n";
        assert_eq!(first_comment(Path::new("a.py"), python), "Rebuilds the oracles from their scripts.");
        let css = "/* The window's look:\n * its colors and its type. */\nbody { }\n";
        assert_eq!(first_comment(Path::new("a.css"), css), "The window's look: its colors and its type.");
        assert_eq!(first_comment(Path::new("a.js"), "const a = 1;\n// late\n"), "", "a comment after the code is not the first");
        let markdown = "# orior\n\n![badge](x.svg)\n\nAn IDE of its own, with **no** [libraries](https://x) from outside.\n\nMore.\n";
        assert_eq!(first_comment(Path::new("README.md"), markdown), "An IDE of its own, with no libraries from outside.");
        let long = format!("// {}\n", "A sentence of words. ".repeat(60));
        let said = first_comment(Path::new("a.js"), &long);
        assert!(said.ends_with("words.") && said.len() <= MOST_SAID, "{said}");
    }

    #[test]
    fn a_file_uses_another_by_its_file_name_or_by_an_import_of_its_name() {
        assert!(uses("import { x } from \"./preview.js\";", "preview.js", "preview"));
        assert!(!uses("import { x } from \"./mdpreview.js\";", "preview.js", "preview"), "a longer name is another file");
        assert!(uses("from tools.oracle import check\n", "oracle.py", "oracle"));
        assert!(uses("pub mod archive;\nuse crate::archive;\n", "archive.rs", "archive"));
        assert!(uses("#include \"qry_buffer.h\"\n", "qry_buffer.h", "qry_buffer"));
        assert!(uses("\\input{chapters/one}\n", "one.tex", "one"));
        assert!(!uses("the preview shows\n", "preview.js", "preview"), "a word in prose is no use");
    }

    #[test]
    fn a_file_s_purpose_names_the_files_of_the_tree_that_use_it() {
        let root = std::env::temp_dir().join(format!("orior_ui_purpose_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("src")).unwrap();
        std::fs::write(root.join("src/lib.js"), "// SPDX-License-Identifier: MIT\n\n// Helpers for the window.\nexport const one = 1;\n").unwrap();
        std::fs::write(root.join("src/main.js"), "import { one } from \"./lib.js\";\n").unwrap();
        std::fs::write(root.join("src/other.js"), "const nothing = 0;\n").unwrap();
        std::fs::write(root.join("index.html"), "<script type=\"module\" src=\"src/lib.js\"></script>\n").unwrap();
        let read = purpose(&root, "src/lib.js").unwrap();
        assert_eq!(read, Purpose { said: "Helpers for the window.".into(), users: vec!["index.html".into(), "src/main.js".into()], all: 2 });
        let _ = std::fs::remove_dir_all(&root);
    }
}
