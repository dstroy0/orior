// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Trust: whether a tree's own code runs as orior's tools read the tree. A tree holds code of its own
//! where a tool runs one of its files as it reads it:
//!
//! - `build.rs` beside a Cargo.toml, and a crate whose Cargo.toml makes it a procedural macro, which
//!   rust-analyzer builds and runs as it reads the tree;
//! - a Prettier or ESLint configuration written in JavaScript, which Prettier and ESLint run as they
//!   format or check a file;
//! - a hook git runs as it reads the tree's state, `core.fsmonitor` naming a program in the tree's
//!   `.git/config`.
//!
//! The reader's answer is kept in trust.json in orior's home, by folder, and a folder's answer holds
//! for every tree under it, the nearest folder's answer standing. A tree that holds code of its own
//! and is not trusted has that code held back: rust-analyzer starts with its build scripts, its
//! procedural macros and its checks off, the checkers the reader names do not run, Prettier does not
//! format where the tree's configuration is JavaScript, and git is told to run no fsmonitor hook. A
//! tree with no code of its own is never asked about.

use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

/// The configurations Prettier runs as code.
const PRETTIER: [&str; 6] = [".prettierrc.js", ".prettierrc.cjs", ".prettierrc.mjs", "prettier.config.js", "prettier.config.cjs", "prettier.config.mjs"];

/// The configurations ESLint runs as code.
const ESLINT: [&str; 5] = [".eslintrc.js", ".eslintrc.cjs", "eslint.config.js", "eslint.config.cjs", "eslint.config.mjs"];

/// How long a tree's code found is kept before the tree is looked through again.
const FOUND_FOR: Duration = Duration::from_secs(10);

/// The answers kept: the folders trusted and the folders whose code is held back.
#[derive(Serialize, Deserialize, Default)]
struct Kept {
    #[serde(default)]
    trusted: Vec<String>,
    #[serde(default)]
    refused: Vec<String>,
}

fn file() -> Option<PathBuf> {
    crate::home::folder().map(|home| home.join("trust.json"))
}

/// A folder as the answers are kept by: its path with `/` between its parts, and in lower case where
/// the system does not tell the cases apart.
fn key(folder: &Path) -> String {
    let written = folder.to_string_lossy().replace('\\', "/").trim_end_matches('/').to_string();
    if cfg!(windows) {
        written.to_lowercase()
    } else {
        written
    }
}

fn read(path: Option<&Path>) -> Kept {
    path.and_then(|path| std::fs::read_to_string(path).ok()).and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

/// Whether `inner` is `folder` or under it.
fn under(inner: &str, folder: &str) -> bool {
    inner == folder || inner.strip_prefix(folder).is_some_and(|rest| rest.starts_with('/'))
}

/// The reader's answer for `tree`: the answer kept for the nearest folder that is the tree or holds
/// it, true where it is trusted, and none where no answer is kept.
pub fn answer(tree: &Path) -> Option<bool> {
    answer_in(file().as_deref(), tree)
}

fn answer_in(path: Option<&Path>, tree: &Path) -> Option<bool> {
    let kept = read(path);
    let tree = key(tree);
    let nearest = |folders: &[String]| folders.iter().filter(|folder| under(&tree, folder)).map(String::len).max();
    match (nearest(&kept.trusted), nearest(&kept.refused)) {
        (None, None) => None,
        (trusted, refused) => Some(trusted > refused),
    }
}

/// Keeps the answer for `folder` and every tree under it, in place of those kept for it and for the
/// folders under it before.
pub fn set(folder: &Path, trusted: bool) -> Result<(), String> {
    set_in(&file().ok_or("orior has no home folder")?, folder, trusted)
}

fn set_in(path: &Path, folder: &Path, trusted: bool) -> Result<(), String> {
    let mut kept = read(Some(path));
    let folder = key(folder);
    kept.trusted.retain(|one| !under(one, &folder));
    kept.refused.retain(|one| !under(one, &folder));
    if trusted {
        kept.trusted.push(folder);
    } else {
        kept.refused.push(folder);
    }
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|error| error.to_string())?;
    }
    let text = serde_json::to_string_pretty(&kept).map_err(|error| error.to_string())?;
    std::fs::write(path, text).map_err(|error| format!("{}: {error}", path.display()))?;
    forget();
    Ok(())
}

/// Whether git's `core.fsmonitor`, as `config` sets it, names a program to run, and not git's own
/// monitor or none.
fn names_a_hook(config: &str) -> bool {
    let mut in_core = false;
    for line in config.lines().map(str::trim) {
        if line.starts_with('[') {
            in_core = line.trim_start_matches('[').trim_end_matches(']').trim().eq_ignore_ascii_case("core");
        } else if let Some((name, value)) = line.split_once('=').filter(|_| in_core) {
            let value = value.trim().trim_matches('"').to_ascii_lowercase();
            if name.trim().eq_ignore_ascii_case("fsmonitor") && !["", "true", "false", "yes", "no", "on", "off", "1", "0"].contains(&value.as_str()) {
                return true;
            }
        }
    }
    false
}

/// The files of `tree` its tools run as code as they read it, as the tree's paths.
fn looked_for(tree: &Path) -> Vec<String> {
    let files = crate::files::all(tree);
    let mut found = Vec::new();
    for file in &files {
        let (folder, name) = file.rsplit_once('/').map_or(("", file.as_str()), |(folder, name)| (folder, name));
        let beside = |other: &str| if folder.is_empty() { other.to_string() } else { format!("{folder}/{other}") };
        let code = match name {
            "build.rs" => files.binary_search(&beside("Cargo.toml")).is_ok(),
            "Cargo.toml" => std::fs::read_to_string(tree.join(file)).is_ok_and(|text| text.lines().any(|line| line.replace(' ', "") == "proc-macro=true")),
            _ => PRETTIER.contains(&name) || ESLINT.contains(&name),
        };
        if code {
            found.push(file.clone());
        }
    }
    if std::fs::read_to_string(tree.join(".git").join("config")).is_ok_and(|config| names_a_hook(&config)) {
        found.push(".git/config".to_string());
    }
    found
}

/// The code found in each tree looked through, and when.
static FOUND: Mutex<Vec<(PathBuf, Instant, Vec<String>)>> = Mutex::new(Vec::new());

fn forget() {
    if let Ok(mut found) = FOUND.lock() {
        found.clear();
    }
}

/// The files of `tree` its tools run as code as they read it, kept FOUND_FOR.
pub fn own_code(tree: &Path) -> Vec<String> {
    if let Ok(found) = FOUND.lock() {
        if let Some((_, _, code)) = found.iter().find(|(at, when, _)| at == tree && when.elapsed() < FOUND_FOR) {
            return code.clone();
        }
    }
    let code = looked_for(tree);
    if let Ok(mut found) = FOUND.lock() {
        found.retain(|(at, _, _)| at != tree);
        found.push((tree.to_path_buf(), Instant::now(), code.clone()));
    }
    code
}

/// Whether `tree` holds code of its own the reader has not trusted, held back until they do.
pub fn held_back(tree: &Path) -> bool {
    answer(tree) != Some(true) && !own_code(tree).is_empty()
}

/// Whether Prettier is held back in `tree`: its configuration there is JavaScript, and the tree is
/// not trusted.
pub fn prettier_held_back(tree: &Path) -> bool {
    held_back(tree) && own_code(tree).iter().any(|file| PRETTIER.contains(&file.rsplit('/').next().unwrap_or(file)))
}

/// The words git is started with in `tree`: that it run no fsmonitor hook, where the configuration
/// of the repository holding the tree names one and the tree is not trusted, and none otherwise.
pub fn git_words(tree: &Path) -> &'static [&'static str] {
    let config = tree.ancestors().map(|folder| folder.join(".git")).find(|git| git.exists()).map(|git| git.join("config"));
    let hook = config.and_then(|config| std::fs::read_to_string(config).ok()).is_some_and(|config| names_a_hook(&config));
    if hook && answer(tree) != Some(true) {
        &["-c", "core.fsmonitor=false"]
    } else {
        &[]
    }
}

/// What the window is told of `tree`: the answer kept, the code it holds, and the folder above it,
/// for an answer that holds for every tree there.
#[derive(Serialize)]
pub struct Told {
    pub answer: Option<bool>,
    pub code: Vec<String>,
    pub tree: String,
    pub above: Option<String>,
}

pub fn told(tree: &Path) -> Told {
    forget();
    Told {
        answer: answer(tree),
        code: own_code(tree),
        tree: tree.display().to_string(),
        above: tree.parent().filter(|parent| parent.parent().is_some()).map(|parent| parent.display().to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_hook_is_a_program_and_not_gits_own_monitor() {
        assert!(names_a_hook("[core]\n\tfsmonitor = .git/hooks/watch\n"));
        assert!(!names_a_hook("[core]\n\tfsmonitor = true\n"));
        assert!(!names_a_hook("[remote \"origin\"]\n\tfsmonitor = run\n"));
    }

    #[test]
    fn a_folder_is_under_itself_and_not_under_a_name_that_starts_the_same() {
        assert!(under("d:/trees/one", "d:/trees"));
        assert!(under("d:/trees", "d:/trees"));
        assert!(!under("d:/treesmore/one", "d:/trees"));
    }

    #[test]
    fn the_code_a_tree_runs_is_found_and_the_nearest_answer_stands() {
        let home = std::env::temp_dir().join(format!("orior_trust_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&home);
        let trees = home.join("trees");
        let tree = trees.join("crate");
        std::fs::create_dir_all(tree.join("src")).unwrap();
        std::fs::write(tree.join("Cargo.toml"), "[package]\nname = \"crate\"\n").unwrap();
        std::fs::write(tree.join("build.rs"), "fn main() {}\n").unwrap();
        std::fs::write(tree.join("src/lib.rs"), "").unwrap();
        std::fs::write(tree.join("prettier.config.js"), "module.exports = {};\n").unwrap();
        let tree = dunce::canonicalize(&tree).unwrap();
        assert_eq!(looked_for(&tree), ["build.rs", "prettier.config.js"]);
        let kept = home.join("trust.json");
        assert_eq!(answer_in(Some(&kept), &tree), None);
        set_in(&kept, tree.parent().unwrap(), true).unwrap();
        assert_eq!(answer_in(Some(&kept), &tree), Some(true), "the folder above's answer holds for the tree");
        set_in(&kept, &tree, false).unwrap();
        assert_eq!(answer_in(Some(&kept), &tree), Some(false), "the tree's own answer stands over the folder's");
        set_in(&kept, tree.parent().unwrap(), true).unwrap();
        assert_eq!(answer_in(Some(&kept), &tree), Some(true), "the folder's answer takes the place of the tree's");
        std::fs::create_dir_all(tree.join(".git")).unwrap();
        std::fs::write(tree.join(".git/config"), "[core]\n\tfsmonitor = watch\n").unwrap();
        assert_eq!(git_words(&tree.join("src")), ["-c", "core.fsmonitor=false"], "git in a tree not trusted runs no hook");
        std::fs::remove_dir_all(&home).unwrap();
    }
}
