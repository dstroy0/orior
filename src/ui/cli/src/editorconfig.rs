// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A file's indentation as `.editorconfig` files set it: each from the file's folder up, until one
//! says `root = true` or the disk's root, the nearer over the farther, and in each the last section
//! whose pattern names the file over those before it. Only indentation is read: `indent_style`,
//! `indent_size`, which `tab` makes the tab's width, and `tab_width`.
//!
//! A section's pattern names a file by its name wherever it stands, or by its path from the folder of
//! its `.editorconfig` where it has a `/`. `*` is any letters of a name, `**` any letters and folders,
//! `?` one letter, `[abc]` and `[!abc]` one letter of or not of those, `{a,b}` either text, and
//! `{1..3}` any whole number between.

use std::fs;
use std::path::{Path, PathBuf};

use serde::Serialize;

/// The indentation found: tabs or spaces, and the width of a step, each None where nothing set it.
#[derive(Serialize, Debug, Default, PartialEq)]
pub struct Indent {
    pub tabs: Option<bool>,
    pub size: Option<u32>,
}

/// The most texts a pattern's braces may spread into.
const SPREAD_MOST: usize = 1024;

pub fn indent(path: &Path) -> Indent {
    let mut files: Vec<(PathBuf, String)> = Vec::new();
    for folder in path.ancestors().skip(1) {
        if let Ok(text) = fs::read_to_string(folder.join(".editorconfig")) {
            let root = text.lines().take_while(|line| !line.trim_start().starts_with('[')).any(|line| {
                let (key, value) = line.split_once('=').unwrap_or_default();
                key.trim().eq_ignore_ascii_case("root") && value.trim().eq_ignore_ascii_case("true")
            });
            files.push((folder.to_path_buf(), text));
            if root {
                break;
            }
        }
    }
    let (mut style, mut size, mut width) = (None::<String>, None::<String>, None::<String>);
    for (folder, text) in files.iter().rev() {
        let relative = path.strip_prefix(folder).map(|rest| rest.to_string_lossy().replace('\\', "/")).unwrap_or_default();
        let name = path.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default();
        let mut inside = false;
        for line in text.lines().map(str::trim) {
            if line.is_empty() || line.starts_with('#') || line.starts_with(';') {
                continue;
            }
            if let Some(pattern) = line.strip_prefix('[').and_then(|rest| rest.strip_suffix(']')) {
                inside = names(pattern, &relative, &name);
                continue;
            }
            let Some((key, value)) = line.split_once('=') else { continue };
            if !inside {
                continue;
            }
            let value = value.trim().to_lowercase();
            match key.trim().to_lowercase().as_str() {
                "indent_style" => style = Some(value),
                "indent_size" => size = Some(value),
                "tab_width" => width = Some(value),
                _ => {}
            }
        }
    }
    let tabs = style.as_deref().and_then(|style| match style {
        "tab" => Some(true),
        "space" => Some(false),
        _ => None,
    });
    let width = width.and_then(|width| width.parse().ok());
    let size = match size.as_deref() {
        Some("tab") => width,
        Some(number) => number.parse().ok().or(width),
        None if tabs == Some(true) => width,
        None => None,
    };
    Indent { tabs, size }
}

/// Whether a section's pattern names the file: by its name alone where the pattern has no `/`, and by
/// its path from the `.editorconfig`'s folder where it has one.
fn names(pattern: &str, relative: &str, name: &str) -> bool {
    let (pattern, target) = if pattern.contains('/') { (pattern.trim_start_matches('/'), relative) } else { (pattern, name) };
    let target: Vec<char> = target.chars().collect();
    spread(pattern).iter().any(|one| glob(&one.chars().collect::<Vec<_>>(), &target))
}

/// The texts a pattern's braces spread into: `{a,b}` into a and b, `{1..3}` into 1, 2 and 3.
fn spread(pattern: &str) -> Vec<String> {
    let letters: Vec<char> = pattern.chars().collect();
    let mut depth = 0;
    let mut open = None;
    for (at, letter) in letters.iter().enumerate() {
        match letter {
            '\\' => {}
            '{' => {
                if depth == 0 {
                    open = Some(at);
                }
                depth += 1;
            }
            '}' if depth > 0 => {
                depth -= 1;
                if depth == 0 {
                    let start = open.unwrap_or(0);
                    let head: String = letters[..start].iter().collect();
                    let inner: String = letters[start + 1..at].iter().collect();
                    let tail: String = letters[at + 1..].iter().collect();
                    let choices = choices(&inner);
                    let mut out = Vec::new();
                    for choice in choices {
                        for rest in spread(&tail) {
                            if out.len() < SPREAD_MOST {
                                out.push(format!("{head}{choice}{rest}"));
                            }
                        }
                    }
                    return out;
                }
            }
            _ => {}
        }
    }
    vec![pattern.to_string()]
}

/// The choices inside one pair of braces: the texts between its commas, each spread in turn, or the
/// whole numbers of a range.
fn choices(inner: &str) -> Vec<String> {
    if let Some((from, to)) = inner.split_once("..") {
        if let (Ok(from), Ok(to)) = (from.parse::<i64>(), to.parse::<i64>()) {
            let (low, high) = (from.min(to), from.max(to));
            return (low..=high).take(SPREAD_MOST).map(|number| number.to_string()).collect();
        }
    }
    let mut parts = Vec::new();
    let mut depth = 0;
    let mut part = String::new();
    for letter in inner.chars() {
        match letter {
            '{' => depth += 1,
            '}' => depth -= 1,
            ',' if depth == 0 => {
                parts.push(std::mem::take(&mut part));
                continue;
            }
            _ => {}
        }
        part.push(letter);
    }
    parts.push(part);
    if parts.len() == 1 {
        return vec![format!("{{{inner}}}")];
    }
    parts.iter().flat_map(|part| spread(part)).collect()
}

/// Whether a pattern with its braces spread names the whole of a text.
fn glob(pattern: &[char], text: &[char]) -> bool {
    match pattern {
        [] => text.is_empty(),
        ['*', '*', rest @ ..] => (0..=text.len()).any(|at| glob(rest, &text[at..])),
        ['*', rest @ ..] => {
            let name = text.iter().position(|letter| *letter == '/').unwrap_or(text.len());
            (0..=name).any(|at| glob(rest, &text[at..]))
        }
        ['?', rest @ ..] => text.first().is_some_and(|letter| *letter != '/') && glob(rest, &text[1..]),
        ['[', rest @ ..] if rest.contains(&']') => {
            let close = rest.iter().position(|letter| *letter == ']').unwrap_or(0);
            let (set, after) = (&rest[..close], &rest[close + 1..]);
            let (not, set) = match set {
                ['!', set @ ..] => (true, set),
                set => (false, set),
            };
            let Some(letter) = text.first() else { return false };
            let mut inside = false;
            let mut at = 0;
            while at < set.len() {
                if at + 2 < set.len() && set[at + 1] == '-' {
                    inside |= set[at] <= *letter && *letter <= set[at + 2];
                    at += 3;
                } else {
                    inside |= set[at] == *letter;
                    at += 1;
                }
            }
            inside != not && glob(after, &text[1..])
        }
        ['\\', letter, rest @ ..] => text.first() == Some(letter) && glob(rest, &text[1..]),
        [letter, rest @ ..] => text.first() == Some(letter) && glob(rest, &text[1..]),
    }
}

#[cfg(test)]
mod reading {
    use super::{Indent, indent};
    use std::fs;

    #[test]
    fn the_nearest_section_that_names_a_file_sets_its_indent() {
        let root = std::env::temp_dir().join(format!("orior_ui_editorconfig_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("src/deep")).unwrap();
        fs::write(root.join(".editorconfig"), "root = true\n\n[*]\nindent_style = space\nindent_size = 4\n\n[*.{go,mk}]\nindent_style = tab\ntab_width = 8\n\n[Makefile]\nindent_style = tab\n\n[lib/**.js]\nindent_size = 2\n\n[file[0-9].txt]\nindent_size = 3\n").unwrap();
        fs::write(root.join("src/.editorconfig"), "[*.py]\nindent_size = 2\n").unwrap();
        assert_eq!(indent(&root.join("a.rs")), Indent { tabs: Some(false), size: Some(4) });
        assert_eq!(indent(&root.join("main.go")), Indent { tabs: Some(true), size: Some(4) });
        assert_eq!(indent(&root.join("rules.mk")), Indent { tabs: Some(true), size: Some(4) });
        assert_eq!(indent(&root.join("src/deep/x.py")), Indent { tabs: Some(false), size: Some(2) });
        assert_eq!(indent(&root.join("lib/a/b.js")), Indent { tabs: Some(false), size: Some(2) });
        assert_eq!(indent(&root.join("file7.txt")), Indent { tabs: Some(false), size: Some(3) });
        assert_eq!(indent(&root.join("filex.txt")), Indent { tabs: Some(false), size: Some(4) });
        fs::write(root.join(".editorconfig"), "root = true\n[*.go]\nindent_style = tab\ntab_width = 8\n").unwrap();
        assert_eq!(indent(&root.join("main.go")), Indent { tabs: Some(true), size: Some(8) });
        assert_eq!(indent(&root.join("a.rs")), Indent::default());
        fs::remove_dir_all(&root).unwrap();
    }
}
