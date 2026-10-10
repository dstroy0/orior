// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's own folder, for what it keeps between runs and what a reader adds to it: the settings,
//! the reader's plugins under `plugins/`, the stylesheet `user.css` the window lays over its own, and
//! `menus.json`, the reader's menus and toolbar.
//! It is `orior` in the system's folder for settings, %APPDATA% on Windows and $XDG_CONFIG_HOME or
//! ~/.config elsewhere, or the folder ORIOR_HOME names.

use std::path::PathBuf;

pub fn folder() -> Option<PathBuf> {
    if let Some(named) = std::env::var_os("ORIOR_HOME") {
        return Some(PathBuf::from(named));
    }
    let base = if cfg!(windows) {
        std::env::var_os("APPDATA").map(PathBuf::from)
    } else {
        std::env::var_os("XDG_CONFIG_HOME").map(PathBuf::from).or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".config")))
    };
    base.map(|base| base.join("orior"))
}

pub fn plugins() -> Option<PathBuf> {
    folder().map(|folder| folder.join("plugins"))
}

pub fn user_css() -> Option<PathBuf> {
    folder().map(|folder| folder.join("user.css"))
}

pub fn menus() -> Option<PathBuf> {
    folder().map(|folder| folder.join("menus.json"))
}

/// What menus.json holds when orior makes it: a note on what each part is for, and nothing laid out.
const MENUS: &str = "{\n  \"about\": \"Each item is a command's name, as orior <menu> <command> names it, or { \\\"command\\\": name, \\\"label\\\": text, \\\"icon\\\": name }, or \\\"-\\\" for a line; { \\\"label\\\": text, \\\"items\\\": [...] } holds more. A menu named as one of orior's stands in its place, the others stand before Help, and hide takes orior's away by name.\",\n  \"menus\": [],\n  \"hide\": [],\n  \"toolbar\": []\n}\n";

/// The reader's menus, made where they are not there yet.
pub fn ensure_menus() -> Result<PathBuf, String> {
    let path = menus().ok_or("orior has no folder of its own to keep menus in")?;
    if !path.exists() {
        if let Some(dir) = path.parent() {
            std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
        }
        std::fs::write(&path, MENUS).map_err(|error| format!("{}: {error}", path.display()))?;
    }
    Ok(path)
}

/// What user.css holds when orior makes it: a note on what it is for.
const USER_CSS: &str = "/* orior lays this stylesheet over its own, last, and reads it again each time the window\n   comes back to the front. Every color and font the app uses is a variable, named as\n   Preferences lists them. This rule sets two of them for both schemes:\n\n   :root[data-scheme] { --fuse-fire: #ff7a1a; --code: \"Fira Code\", monospace; }\n\n   Any other rule of CSS works here as well. */\n";

/// The reader's stylesheet, made where it is not there yet.
pub fn ensure_user_css() -> Result<PathBuf, String> {
    let path = user_css().ok_or("orior has no folder of its own to keep a stylesheet in")?;
    if !path.exists() {
        if let Some(dir) = path.parent() {
            std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
        }
        std::fs::write(&path, USER_CSS).map_err(|error| format!("{}: {error}", path.display()))?;
    }
    Ok(path)
}

/// Opens a file or a folder as the system opens it: a folder in its file manager, a stylesheet in
/// the program that opens stylesheets.
pub fn reveal(path: &std::path::Path) -> Result<(), String> {
    let mut command = if cfg!(windows) {
        let mut command = std::process::Command::new("explorer");
        command.arg(path);
        command
    } else if cfg!(target_os = "macos") {
        let mut command = std::process::Command::new("open");
        command.arg(path);
        command
    } else {
        let mut command = std::process::Command::new("xdg-open");
        command.arg(path);
        command
    };
    command.spawn().map(|_| ()).map_err(|error| format!("{}: {error}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_menus_made_for_the_reader_are_json_with_nothing_laid_out() {
        let made: serde_json::Value = serde_json::from_str(MENUS).unwrap();
        assert!(made["about"].as_str().is_some_and(|about| about.contains("hide")));
        for key in ["menus", "hide", "toolbar"] {
            assert_eq!(made[key], serde_json::json!([]), "{key}");
        }
    }
}
