// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The definitions the editor reads its languages from, as the tree holds them.
//!
//! Each file type's oracle table under src/cu/types/file_defs, the head every k-file opens with, and
//! the language tables under src/lng. The editor builds its highlighting, its hover and its
//! completion from these and holds no word of its own. A form added to an oracle table is a form
//! the editor knows the next time it opens.

use std::fs;
use std::path::Path;

use serde::Serialize;

use crate::root::relative;

#[derive(Serialize)]
pub struct Table {
    pub name: String,
    pub path: String,
    pub columns: Vec<String>,
    pub rows: Vec<Vec<String>>,
}

#[derive(Serialize)]
pub struct FileType {
    pub ext: String,
    pub table: Table,
    /// The opening comment of the type's header, where it has one.
    pub header: String,
}

#[derive(Serialize)]
pub struct Head {
    pub bytes: u32,
    pub magic: String,
    pub version: u32,
}

#[derive(Serialize)]
pub struct Definitions {
    pub types: Vec<FileType>,
    pub head: Option<Head>,
    pub languages: Vec<Table>,
}

/// A tab separated table whose first line that is not a comment names its columns.
pub fn table(root: &Path, path: &Path) -> Option<Table> {
    let text = fs::read_to_string(path).ok()?;
    let mut lines = text.lines().filter(|line| !line.starts_with('#') && !line.trim().is_empty());
    let columns: Vec<String> = lines.next()?.split('\t').map(str::to_string).collect();
    let rows = lines.map(|line| line.split('\t').map(str::to_string).collect()).collect();
    let name = path.file_name()?.to_string_lossy().into_owned();
    Some(Table { name, path: relative(root, path), columns, rows })
}

/// The comment lines a C header opens with, past its license line.
fn header_comment(path: &Path) -> String {
    let Ok(text) = fs::read_to_string(path) else { return String::new() };
    text.lines()
        .map(str::trim)
        .take_while(|line| line.starts_with("//"))
        .filter(|line| !line.contains("SPDX"))
        .map(|line| line.trim_start_matches('/').trim())
        .collect::<Vec<_>>()
        .join(" ")
}

/// A `#define NAME value` read out of a header, its value up to the first space or `u` suffix.
fn define(text: &str, name: &str) -> Option<String> {
    let key = format!("#define {name} ");
    let line = text.lines().find(|line| line.starts_with(&key))?;
    Some(line[key.len()..].trim().to_string())
}

fn head(root: &Path) -> Option<Head> {
    let text = fs::read_to_string(root.join("src/cu/types/file_defs/krep_head.h")).ok()?;
    let number = |name: &str| define(&text, name)?.trim_end_matches('u').parse::<u32>().ok();
    let magic = define(&text, "KREP_MAGIC")?;
    let magic = magic.trim_matches('"').split('\\').next().unwrap_or("").to_string();
    Some(Head { bytes: number("KREP_HEAD_BYTES")?, magic, version: number("KREP_VERSION")? })
}

pub fn read(root: &Path) -> Definitions {
    let defs = root.join("src/cu/types/file_defs");
    let mut types = Vec::new();
    let mut dirs: Vec<_> = fs::read_dir(&defs).into_iter().flatten().flatten().map(|e| e.path()).filter(|p| p.is_dir()).collect();
    dirs.sort();
    for dir in dirs {
        let ext = dir.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let Some(table) = table(root, &dir.join(format!("{ext}.oracle.tsv"))) else { continue };
        let header = header_comment(&dir.join(format!("{ext}.h")));
        types.push(FileType { ext, table, header });
    }
    let mut languages: Vec<Table> = fs::read_dir(root.join("src/lng"))
        .into_iter()
        .flatten()
        .flatten()
        .map(|e| e.path())
        .filter(|p| p.extension().is_some_and(|e| e == "tsv"))
        .filter_map(|p| table(root, &p))
        .collect();
    languages.sort_by(|a, b| a.name.cmp(&b.name));
    Definitions { types, head: head(root), languages }
}

#[cfg(test)]
mod reading {
    #[test]
    fn the_tree_gives_every_type_and_the_head() {
        let Some(root) = crate::root::find() else { return };
        let defs = super::read(&root);
        for ext in ["g", "gsm", "krs", "kcr"] {
            let found = defs.types.iter().find(|t| t.ext == ext).unwrap_or_else(|| panic!("no {ext} table"));
            assert_eq!(found.table.columns, ["where", "who", "kind", "form", "gloss"]);
        }
        let head = defs.head.expect("the krep head");
        assert_eq!((head.bytes, head.magic.as_str()), (16, "KREP"));
        assert!(defs.languages.iter().any(|t| t.name == "gnascor_asm_lng.tsv"));
    }
}
