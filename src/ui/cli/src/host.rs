// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior in a Flatpak's sandbox runs the machine's own programs on the machine. Inside the sandbox
//! none of the machine's compilers, SDKs and tools are there: each program on the machine's PATH
//! is given a stand-in of the same name in a folder of orior's own, put first on the PATH of orior
//! and of all it starts: a script that runs the program on the machine through `flatpak-spawn
//! --host`, in the folder it was started in, with the terminal's kind, the language, and each
//! variable ORIOR_HOST_ENV names. A job, a tool, a language server, git and the terminal's shell all
//! reach the machine's own that way. Outside a sandbox nothing changes.

use std::path::{Path, PathBuf};

/// The programs that keep running inside the sandbox: the one that reaches the machine, and orior.
const INSIDE: [&str; 3] = ["flatpak-spawn", "orior", "orior-cli"];

/// The variables every stand-in passes to the machine's program, where they are set.
const PASSED: [&str; 4] = ["TERM", "COLORTERM", "LANG", "LC_ALL"];

/// Whether orior runs inside a Flatpak's sandbox.
pub fn sandboxed() -> bool {
    cfg!(target_os = "linux") && Path::new("/.flatpak-info").is_file()
}

fn shell_word(text: &str) -> String {
    format!("'{}'", text.replace('\'', r"'\''"))
}

/// The stand-in for the machine's program `name`: the program and its words, after each variable
/// passed to it, run on the machine in the folder the stand-in was started in.
pub fn stand_in(name: &str) -> String {
    let passed: String = PASSED.iter().map(|variable| format!(r#" ${{{variable}+"--env={variable}=${variable}"}}"#)).collect();
    let mut text = String::from("#!/bin/sh\n");
    text.push_str(&format!("set -- {} \"$@\"\n", shell_word(name)));
    text.push_str("for name in $ORIOR_HOST_ENV; do\n");
    text.push_str("  eval \"value=\\${$name-}\"\n");
    text.push_str("  set -- \"--env=$name=$value\" \"$@\"\n");
    text.push_str("done\n");
    text.push_str(&format!("exec flatpak-spawn --host --watch-bus --directory=\"$PWD\"{passed} \"$@\"\n"));
    text
}

/// The folder the stand-ins are in.
pub fn folder() -> PathBuf {
    std::env::var_os("XDG_RUNTIME_DIR").map(PathBuf::from).unwrap_or_else(std::env::temp_dir).join("orior-host")
}

/// Writes a stand-in in `into` for each program of `names` but those that keep running inside.
pub fn write_stand_ins(into: &Path, names: &[String]) -> Result<usize, String> {
    std::fs::create_dir_all(into).map_err(|error| format!("{}: {error}", into.display()))?;
    let mut count = 0;
    for name in names.iter().filter(|name| !name.is_empty() && !name.contains(['/', '\\']) && name.as_str() != ".." && !INSIDE.contains(&name.as_str())) {
        let path = into.join(name);
        std::fs::write(&path, stand_in(name)).map_err(|error| format!("{}: {error}", path.display()))?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            std::fs::set_permissions(&path, std::fs::Permissions::from_mode(0o755)).map_err(|error| format!("{}: {error}", path.display()))?;
        }
        count += 1;
    }
    Ok(count)
}

/// Inside a sandbox, makes a stand-in for each program on the machine's PATH and puts their folder
/// first on orior's own PATH, which all it starts take. Says how many it made, or nothing outside a
/// sandbox. Run as orior starts, before it starts anything else.
pub fn prepare() -> Option<Result<usize, String>> {
    if !sandboxed() {
        return None;
    }
    let listed = std::process::Command::new("flatpak-spawn").args(["--host", "sh", "-c", "IFS=:; for dir in $PATH; do ls -1 \"$dir\" 2>/dev/null; done"]).output();
    let names: Vec<String> = match listed {
        Ok(out) if out.status.success() => String::from_utf8_lossy(&out.stdout).lines().map(str::to_string).collect(),
        Ok(out) => return Some(Err(format!("flatpak-spawn --host: {}", String::from_utf8_lossy(&out.stderr).trim()))),
        Err(error) => return Some(Err(format!("flatpak-spawn: {error}"))),
    };
    let into = folder();
    let made = write_stand_ins(&into, &names);
    if made.is_ok() {
        let mut path = vec![into];
        path.extend(std::env::split_paths(&std::env::var_os("PATH").unwrap_or_default()));
        if let Ok(joined) = std::env::join_paths(path) {
            // orior starts nothing before this, and no thread of its own runs yet.
            unsafe { std::env::set_var("PATH", joined) };
        }
    }
    Some(made)
}

#[cfg(test)]
mod standing_in {
    use super::{stand_in, write_stand_ins};

    #[test]
    fn each_program_on_the_machine_gets_a_stand_in_that_runs_it_there() {
        let text = stand_in("cargo");
        assert!(text.starts_with("#!/bin/sh\n"));
        assert!(text.contains(r#"set -- 'cargo' "$@""#), "{text}");
        assert!(text.contains(r#"exec flatpak-spawn --host --watch-bus --directory="$PWD" ${TERM+"--env=TERM=$TERM"}"#), "{text}");
        assert!(text.ends_with("\"$@\"\n"), "{text}");
        assert!(stand_in("it's").contains(r"'it'\''s'"));
        let folder = std::env::temp_dir().join(format!("orior_ui_host_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&folder);
        let made = write_stand_ins(&folder, &["git".into(), "flatpak-spawn".into(), "orior".into(), "../x".into(), String::new()]).unwrap();
        assert_eq!(made, 1, "orior and the program that reaches the machine keep running inside");
        assert!(folder.join("git").is_file() && !folder.join("flatpak-spawn").exists());
        std::fs::remove_dir_all(&folder).unwrap();
    }
}
