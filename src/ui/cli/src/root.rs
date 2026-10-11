// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The orior tree the app works on, and the rule that keeps every path inside it.
//!
//! A window can hold folders from other places, or single files from them, beside its tree: each is
//! mounted under a name of its own, and a path that starts `@name/` is under that folder, or `@name`
//! is that file. The rule that keeps a path inside the tree keeps one under a mount inside the mount.

use std::path::{Component, Path, PathBuf};
use std::sync::Mutex;

/// The file whose presence marks the top of the engine's tree, which the command line finds by
/// walking up where no tree is named.
const MARKER: &str = "src/cu/engine/engine_config.h";

/// The folders and files mounted beside the tree, each under its name.
static MOUNTS: Mutex<Vec<(String, PathBuf)>> = Mutex::new(Vec::new());

pub fn holds_tree(dir: &Path) -> bool {
    dir.join(MARKER).is_file()
}

/// The folder at `path` as a tree: any folder is one, by its full path.
pub fn tree_at(path: &Path) -> Result<PathBuf, String> {
    let full = dunce::canonicalize(path).map_err(|error| format!("{}: {error}", path.display()))?;
    if full.is_dir() { Ok(full) } else { Err(format!("{} is no folder", full.display())) }
}

/// The tree ORIOR_ROOT names, else the engine's tree found walking up from the working directory,
/// else the one found walking up from the program itself.
pub fn find() -> Option<PathBuf> {
    if let Ok(named) = std::env::var("ORIOR_ROOT") {
        if let Ok(tree) = tree_at(Path::new(&named)) {
            return Some(tree);
        }
    }
    let starts = [
        std::env::current_dir().ok(),
        std::env::current_exe().ok().and_then(|exe| exe.parent().map(Path::to_path_buf)),
    ];
    for start in starts.into_iter().flatten() {
        if let Some(dir) = start.ancestors().find(|dir| holds_tree(dir)) {
            return dunce::canonicalize(dir).ok();
        }
    }
    None
}

/// Mounts the folder or file at `path` beside the tree, under the last part of its path, with a
/// number after it where another has that name, and gives the name; one mounted already keeps its
/// name.
pub fn mount(path: &Path) -> Result<String, String> {
    let path = dunce::canonicalize(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let mut mounts = MOUNTS.lock().map_err(|_| "the mounts are held".to_string())?;
    if let Some((name, _)) = mounts.iter().find(|(_, one)| *one == path) {
        return Ok(name.clone());
    }
    let base = path.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_else(|| "folder".to_string()).replace(['/', '\\'], "_");
    let mut name = base.clone();
    let mut count = 2;
    while mounts.iter().any(|(one, _)| *one == name) {
        name = format!("{base}-{count}");
        count += 1;
    }
    mounts.push((name.clone(), path));
    Ok(name)
}

/// Takes the mount named `name` away.
pub fn unmount(name: &str) {
    if let Ok(mut mounts) = MOUNTS.lock() {
        mounts.retain(|(one, _)| one != name);
    }
}

/// Takes every mount away, as a window opening another tree does.
pub fn unmount_all() {
    if let Ok(mut mounts) = MOUNTS.lock() {
        mounts.clear();
    }
}

/// The mounts, each its name and its folder or file, in the order they were mounted.
pub fn mounts() -> Vec<(String, PathBuf)> {
    MOUNTS.lock().map(|mounts| mounts.clone()).unwrap_or_default()
}

/// `relative` under `root`, or under the mount its first part names where it starts with `@`. A path
/// that climbs out of the tree or its mount, or names a root of its own, is refused before anything
/// touches the disk.
pub fn inside(root: &Path, relative: &str) -> Result<PathBuf, String> {
    let (mut joined, rest) = match relative.strip_prefix('@') {
        Some(mounted) => {
            let (name, rest) = mounted.split_once('/').unwrap_or((mounted, ""));
            let base = mounts().into_iter().find(|(one, _)| one == name).map(|(_, path)| path).ok_or_else(|| format!("{relative} is in no folder of this window"))?;
            (base, rest)
        }
        None => (root.to_path_buf(), relative),
    };
    for part in Path::new(rest).components() {
        match part {
            Component::Normal(name) => joined.push(name),
            Component::CurDir => {}
            _ => return Err(format!("{relative} is outside the tree")),
        }
    }
    Ok(joined)
}

/// `relative` under `root` or its mount, as `inside` gives it, or joined to `root` where `inside`
/// refuses it, for a path the app wrote itself.
pub fn full(root: &Path, relative: &str) -> PathBuf {
    inside(root, relative).unwrap_or_else(|_| root.join(relative))
}

/// A path under `root`, or under a mount, written relative to it with forward slashes, the form every
/// path takes in the app: one under a mount starts with its name after `@`.
pub fn relative(root: &Path, path: &Path) -> String {
    if !path.starts_with(root) {
        let found = mounts().into_iter().filter(|(_, base)| path.starts_with(base)).max_by_key(|(_, base)| base.as_os_str().len());
        if let Some((name, base)) = found {
            let rest = path.strip_prefix(&base).unwrap_or(path).to_string_lossy().replace('\\', "/");
            return if rest.is_empty() { format!("@{name}") } else { format!("@{name}/{rest}") };
        }
    }
    let short = path.strip_prefix(root).unwrap_or(path);
    short.to_string_lossy().replace('\\', "/")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_mounted_folder_and_file_are_reached_by_their_names_and_kept_inside() {
        let base = std::env::temp_dir().join(format!("orior-mounts-{}", std::process::id()));
        let tree = base.join("tree");
        let other = base.join("other");
        std::fs::create_dir_all(other.join("sub")).unwrap();
        std::fs::create_dir_all(&tree).unwrap();
        std::fs::write(other.join("sub/a.txt"), "a").unwrap();
        std::fs::write(base.join("lone.txt"), "lone").unwrap();
        let folder = mount(&other).unwrap();
        let file = mount(&base.join("lone.txt")).unwrap();
        assert_eq!(mount(&other).unwrap(), folder);
        let reached = inside(&tree, &format!("@{folder}/sub/a.txt")).unwrap();
        assert_eq!(std::fs::read_to_string(&reached).unwrap(), "a");
        assert_eq!(std::fs::read_to_string(inside(&tree, &format!("@{file}")).unwrap()).unwrap(), "lone");
        assert_eq!(relative(&tree, &reached), format!("@{folder}/sub/a.txt"));
        assert!(inside(&tree, &format!("@{folder}/../escape")).is_err());
        assert!(inside(&tree, "@nothing/a").is_err());
        assert_eq!(relative(&tree, &tree.join("x/y.rs")), "x/y.rs");
        unmount(&folder);
        unmount(&file);
        assert!(inside(&tree, &format!("@{folder}/sub/a.txt")).is_err());
        let _ = std::fs::remove_dir_all(&base);
    }
}
