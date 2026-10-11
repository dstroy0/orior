// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Templates: the reader's own starts for a project, each a folder under `templates/` in orior's own
//! folder. A project begun from one is a copy of it in a new folder, `{{name}}` written as the
//! project's name in every file's name and in every file's text. A tree kept as a template is a copy
//! of the files the tree's patterns show, past git's own folder.

use std::path::{Path, PathBuf};

use crate::{files, home};

/// The folder that holds the reader's templates.
pub fn folder() -> Option<PathBuf> {
    home::folder().map(|folder| folder.join("templates"))
}

/// The reader's templates, by name, in order.
pub fn list() -> Vec<String> {
    folder().map(|folder| list_in(&folder)).unwrap_or_default()
}

fn list_in(folder: &Path) -> Vec<String> {
    let mut names: Vec<String> = std::fs::read_dir(folder).into_iter().flatten().flatten().filter(|entry| entry.path().is_dir()).map(|entry| entry.file_name().to_string_lossy().to_string()).collect();
    names.sort();
    names
}

/// Whether a name is one a folder may take: no separator, no `..`, and none of the characters a path
/// reads as more than a name.
fn plain(name: &str) -> bool {
    !name.trim().is_empty() && !name.contains(['/', '\\', ':', '*', '?', '"', '<', '>', '|']) && name != "." && name != ".."
}

/// Copies `from` into `to`, made, `{{name}}` written as `name` in each file's name and in each file's
/// text where it is text.
fn copy(from: &Path, to: &Path, name: &str) -> Result<(), String> {
    std::fs::create_dir_all(to).map_err(|error| format!("{}: {error}", to.display()))?;
    for entry in std::fs::read_dir(from).map_err(|error| format!("{}: {error}", from.display()))?.flatten() {
        let named = entry.file_name().to_string_lossy().replace("{{name}}", name);
        let target = to.join(&named);
        if entry.path().is_dir() {
            copy(&entry.path(), &target, name)?;
            continue;
        }
        let bytes = std::fs::read(entry.path()).map_err(|error| format!("{}: {error}", entry.path().display()))?;
        let written = match String::from_utf8(bytes) {
            Ok(text) => text.replace("{{name}}", name).into_bytes(),
            Err(raw) => raw.into_bytes(),
        };
        std::fs::write(&target, written).map_err(|error| format!("{}: {error}", target.display()))?;
    }
    Ok(())
}

/// Begins a project named `name` in `parent` from the template `template`. Gives its folder.
pub fn create(template: &str, parent: &Path, name: &str) -> Result<PathBuf, String> {
    create_in(&folder().ok_or("orior has no folder of its own to keep templates in")?, template, parent, name)
}

fn create_in(folder: &Path, template: &str, parent: &Path, name: &str) -> Result<PathBuf, String> {
    if !plain(name) {
        return Err(format!("{name} is no name a folder may take"));
    }
    let source = folder.join(template);
    if !plain(template) || !source.is_dir() {
        return Err(format!("{template} is no template of yours: they are the folders in {}", folder.display()));
    }
    let target = parent.join(name);
    if target.exists() {
        return Err(format!("{} is there already", target.display()));
    }
    copy(&source, &target, name)?;
    Ok(target)
}

/// Keeps the tree at `root` as the template `name`: the files its patterns show, past git's folder.
/// Gives the template's folder.
pub fn keep(root: &Path, name: &str) -> Result<PathBuf, String> {
    if !plain(name) {
        return Err(format!("{name} is no name a template may take"));
    }
    let target = folder().ok_or("orior has no folder of its own to keep templates in")?.join(name);
    if target.exists() {
        return Err(format!("{} is a template already", name));
    }
    for file in files::all(root) {
        if file == ".git" || file.starts_with(".git/") {
            continue;
        }
        let to = target.join(&file);
        if let Some(dir) = to.parent() {
            std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
        }
        std::fs::copy(root.join(&file), &to).map_err(|error| format!("{file}: {error}"))?;
    }
    Ok(target)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_project_is_begun_from_a_template_with_its_name_written_in() {
        let home = std::env::temp_dir().join(format!("orior-templates-{}", std::process::id()));
        let template = home.join("templates").join("script");
        std::fs::create_dir_all(template.join("{{name}}")).unwrap();
        std::fs::write(template.join("{{name}}").join("main.py"), "\"\"\"{{name}}.\"\"\"\n").unwrap();
        std::fs::write(template.join("logo.bin"), [0xff, 0xfe, 0x00]).unwrap();
        let folder = home.join("templates");
        assert_eq!(list_in(&folder), vec!["script"]);
        let parent = home.join("work");
        std::fs::create_dir_all(&parent).unwrap();
        let made = create_in(&folder, "script", &parent, "tally").unwrap();
        assert_eq!(std::fs::read_to_string(made.join("tally").join("main.py")).unwrap(), "\"\"\"tally.\"\"\"\n");
        assert_eq!(std::fs::read(made.join("logo.bin")).unwrap(), vec![0xff, 0xfe, 0x00]);
        assert!(create_in(&folder, "script", &parent, "tally").is_err(), "a folder there already is not written over");
        assert!(create_in(&folder, "../x", &parent, "y").is_err() && create_in(&folder, "script", &parent, "a/b").is_err());
        let _ = std::fs::remove_dir_all(&home);
    }
}
