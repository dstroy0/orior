// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The orior tree the app works on, and the rule that keeps every path inside it.

use std::path::{Component, Path, PathBuf};

/// The file whose presence marks the top of an orior tree.
const MARKER: &str = "src/cu/engine/engine_config.h";

pub fn holds_tree(dir: &Path) -> bool {
    dir.join(MARKER).is_file()
}

/// The tree ORIOR_ROOT names, else the first one found walking up from the working directory, else
/// the first one found walking up from the program itself.
pub fn find() -> Option<PathBuf> {
    if let Ok(named) = std::env::var("ORIOR_ROOT") {
        let named = PathBuf::from(named);
        if holds_tree(&named) {
            return dunce::canonicalize(named).ok();
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

/// `relative` under `root`. A path that climbs out of the tree, or names a root of its own, is
/// refused before anything touches the disk.
pub fn inside(root: &Path, relative: &str) -> Result<PathBuf, String> {
    let mut joined = root.to_path_buf();
    for part in Path::new(relative).components() {
        match part {
            Component::Normal(name) => joined.push(name),
            Component::CurDir => {}
            _ => return Err(format!("{relative} is outside the tree")),
        }
    }
    Ok(joined)
}

/// A path under `root` written relative to it with forward slashes, the form every path takes in the
/// app.
pub fn relative(root: &Path, path: &Path) -> String {
    let short = path.strip_prefix(root).unwrap_or(path);
    short.to_string_lossy().replace('\\', "/")
}
