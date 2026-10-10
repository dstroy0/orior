// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The tree's environments: the places in the tree a Python environment stands, found at any depth,
//! each the tree's own environment once the reader chooses it.
//!
//! A virtual environment is a folder that holds `pyvenv.cfg`. A Pipfile's folder is a Pipenv
//! environment, which Pipenv is asked where it keeps; and a folder with `shell.nix`, `flake.nix` or
//! `default.nix` a Nix environment, whose development shell Nix is asked for its PATH. Each is named
//! by its folder's path in the tree: the name kept, and one that moves with the tree. The packages a
//! `setup.cfg`, a Pipfile or a `requirements.txt` beside it requires are read, and an environment
//! chosen says which of them it has not installed.
//!
//! The environment chosen is the toolchains' own while it stands: its folders come first on the
//! PATH of everything orior starts, and its Python is the tree's Python.

use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::toolchains::{self, TreeEnvironment};

/// An environment the tree holds: what makes it one, its folder in the tree, and the packages the
/// files beside it require.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Environment {
    pub kind: String,
    pub place: String,
    pub requires: Vec<String>,
}

/// Folders a search for environments does not go into.
const NOT_SEARCHED: [&str; 6] = [".git", "node_modules", "target", "build", "__pycache__", ".tox"];

/// How many folders below the top a search for environments goes.
const DEPTH: usize = 5;

/// Every environment the tree at `root` holds, nearest the top first.
pub fn found(root: &Path) -> Vec<Environment> {
    let mut out = Vec::new();
    let mut folders = vec![(root.to_path_buf(), 0usize)];
    while let Some((folder, depth)) = folders.pop() {
        let place = folder.strip_prefix(root).map(|inside| inside.to_string_lossy().replace('\\', "/")).unwrap_or_default();
        let requires = requires_in(&folder);
        if folder.join("pyvenv.cfg").is_file() {
            let project = folder.parent().map(requires_in).unwrap_or_default();
            out.push(Environment { kind: "venv".into(), place, requires: project });
            continue;
        }
        if folder.join("Pipfile").is_file() {
            out.push(Environment { kind: "pipenv".into(), place: place.clone(), requires: requires.clone() });
        }
        if ["shell.nix", "flake.nix", "default.nix"].iter().any(|name| folder.join(name).is_file()) {
            out.push(Environment { kind: "nix".into(), place: place.clone(), requires: requires.clone() });
        }
        if depth >= DEPTH {
            continue;
        }
        let mut below: Vec<PathBuf> = std::fs::read_dir(&folder).into_iter().flatten().flatten().map(|entry| entry.path()).filter(|path| path.is_dir() && path.file_name().is_some_and(|name| !NOT_SEARCHED.contains(&name.to_string_lossy().as_ref()))).collect();
        below.sort();
        below.reverse();
        folders.extend(below.into_iter().map(|path| (path, depth + 1)));
    }
    out.sort_by_key(|one| (one.place.matches('/').count() + usize::from(!one.place.is_empty()), one.place.clone(), one.kind.clone()));
    out
}

/// The packages a folder's `setup.cfg`, Pipfile or `requirements.txt` requires, by name, in order.
pub fn requires_in(folder: &Path) -> Vec<String> {
    let mut out: Vec<String> = Vec::new();
    let mut add = |name: &str| {
        let name: String = name.trim().chars().take_while(|c| c.is_alphanumeric() || matches!(c, '-' | '_' | '.')).collect();
        if !name.is_empty() && !out.iter().any(|one| one.eq_ignore_ascii_case(&name)) {
            out.push(name);
        }
    };
    if let Ok(text) = std::fs::read_to_string(folder.join("setup.cfg")) {
        let mut section = String::new();
        let mut taking = false;
        for line in text.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with('[') {
                section = trimmed.to_string();
                taking = false;
                continue;
            }
            if section != "[options]" || trimmed.starts_with('#') || trimmed.is_empty() {
                continue;
            }
            if let Some(rest) = trimmed.strip_prefix("install_requires") {
                taking = true;
                if let Some(value) = rest.trim_start().strip_prefix('=') {
                    value.split([',', ';']).for_each(&mut add);
                }
            } else if taking && line.starts_with([' ', '\t']) {
                add(trimmed);
            } else {
                taking = false;
            }
        }
    }
    if let Ok(text) = std::fs::read_to_string(folder.join("Pipfile")) {
        let mut packages = false;
        for line in text.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with('[') {
                packages = trimmed == "[packages]";
                continue;
            }
            if packages {
                if let Some((name, _)) = trimmed.split_once('=') {
                    add(name.trim().trim_matches('"'));
                }
            }
        }
    }
    if let Ok(text) = std::fs::read_to_string(folder.join("requirements.txt")) {
        for line in text.lines().map(str::trim).filter(|line| !line.is_empty() && !line.starts_with(['#', '-'])) {
            add(line);
        }
    }
    out
}

/// The Python and the bin folder of a virtual environment's folder.
fn venv_at(folder: &Path) -> Result<TreeEnvironment, String> {
    let bin = folder.join(if cfg!(windows) { "Scripts" } else { "bin" });
    let python = bin.join(if cfg!(windows) { "python.exe" } else { "python" });
    if !python.is_file() {
        return Err(format!("{} holds no Python", folder.display()));
    }
    Ok(TreeEnvironment { bin: vec![bin], python: Some(python), place: String::new() })
}

/// The output of a program the manifest names, run in `folder` with `args`.
fn ask(id: &str, folder: &Path, args: &[&str]) -> Result<String, String> {
    let tool = toolchains::manifest().into_iter().find(|tool| tool.id == id).ok_or_else(|| format!("{id} is not in the manifest"))?;
    let found = toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen());
    let program = found.program.ok_or_else(|| format!("{} is not found: File, Toolchains opens its install page or takes its folder", tool.name))?;
    let mut command = Command::new(program);
    command.args(args).current_dir(folder).env("PATH", toolchains::run_path()).env("PIPENV_IGNORE_VIRTUALENVS", "1").stdin(Stdio::null());
    crate::runner::quiet(&mut command);
    let out = command.output().map_err(|error| format!("{}: {error}", tool.name))?;
    if !out.status.success() {
        let said = String::from_utf8_lossy(&out.stderr);
        return Err(format!("{}: {}", tool.name, said.trim().lines().last().unwrap_or("it failed")));
    }
    Ok(String::from_utf8_lossy(&out.stdout).to_string())
}

/// The environment at `place` in the tree at `root`, of `kind`, made ready to use: for Pipenv, the
/// virtual environment Pipenv keeps for the folder; for Nix, the folders its development shell puts
/// on the PATH.
pub fn resolve(root: &Path, kind: &str, place: &str) -> Result<TreeEnvironment, String> {
    let folder = if place.is_empty() { root.to_path_buf() } else { root.join(place) };
    let mut env = match kind {
        "venv" => venv_at(&folder)?,
        "pipenv" => {
            let said = ask("pipenv", &folder, &["--venv"])?;
            venv_at(Path::new(said.trim()))?
        }
        "nix" => {
            let said = ask("nix", &folder, &["print-dev-env", "--json"])?;
            let value: Value = serde_json::from_str(&said).map_err(|error| format!("Nix: {error}"))?;
            let path = value["variables"]["PATH"]["value"].as_str().unwrap_or_default();
            let bin: Vec<PathBuf> = std::env::split_paths(path).filter(|dir| dir.display().to_string().contains("/nix/store/")).collect();
            let python = bin.iter().map(|dir| dir.join("python3")).find(|python| python.is_file());
            TreeEnvironment { bin, python, place: String::new() }
        }
        _ => return Err(format!("{kind} is no kind of environment")),
    };
    env.place = place.to_string();
    Ok(env)
}

/// The packages `requires` names that the Python of `env` has not installed.
pub fn missing(env: &TreeEnvironment, requires: &[String]) -> Vec<String> {
    let Some(python) = &env.python else {
        return Vec::new();
    };
    let mut command = Command::new(python);
    command.args(["-m", "pip", "list", "--format", "json", "--disable-pip-version-check"]).stdin(Stdio::null());
    crate::runner::quiet(&mut command);
    let Ok(out) = command.output() else {
        return Vec::new();
    };
    let listed: Vec<String> = serde_json::from_slice::<Value>(&out.stdout).ok().and_then(|value| value.as_array().cloned()).unwrap_or_default().iter().filter_map(|one| one["name"].as_str()).map(|name| name.to_lowercase().replace('_', "-")).collect();
    requires.iter().filter(|name| !listed.contains(&name.to_lowercase().replace('_', "-"))).cloned().collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn environments_are_found_at_any_depth_with_what_beside_them_requires() {
        let root = std::env::temp_dir().join(format!("orior-envs-{}", std::process::id()));
        let write = |name: &str, text: &str| {
            let path = root.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        };
        write(".venv/pyvenv.cfg", "home = x\n");
        write(".venv/Lib/site-packages/pyvenv.cfg", "not searched\n");
        write("setup.cfg", "[metadata]\nname = a\n\n[options]\ninstall_requires =\n    numpy>=1.20\n    scipy\npython_requires = >=3.10\n");
        write("tools/sub/Pipfile", "[[source]]\nurl = \"x\"\n\n[packages]\nrequests = \"*\"\n\"click\" = \">=8\"\n\n[dev-packages]\npytest = \"*\"\n");
        write("nixed/shell.nix", "{ }\n");
        write("node_modules/x/pyvenv.cfg", "not searched\n");
        let found = found(&root);
        let shown: Vec<String> = found.iter().map(|one| format!("{} {} {}", one.kind, one.place, one.requires.join(","))).collect();
        assert_eq!(shown, vec!["venv .venv numpy,scipy", "nix nixed ", "pipenv tools/sub requests,click"]);
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    #[ignore = "makes a virtual environment with the Python the manifest finds"]
    fn a_virtual_environment_made_in_the_tree_is_found_and_resolved() {
        let root = std::env::temp_dir().join(format!("orior-venv-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let python = toolchains::manifest().into_iter().find(|tool| tool.id == "python").map(|tool| toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen())).and_then(|found| found.program).unwrap();
        let made = Command::new(python).args(["-m", "venv", "--without-pip", "env"]).current_dir(&root).status().unwrap();
        assert!(made.success());
        let found = found(&root);
        assert_eq!(found[0].place, "env");
        let env = resolve(&root, "venv", "env").unwrap();
        assert!(env.python.as_ref().unwrap().is_file());
        let _ = std::fs::remove_dir_all(&root);
    }
}
