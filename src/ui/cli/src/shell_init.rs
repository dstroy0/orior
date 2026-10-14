// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The start of a terminal's bash: the reader's own files first, as a login shell reads them, then
//! the tree's environment, as activating it puts it, and the folder said before each prompt.
//!
//! A login shell's own files run after the shell is given its PATH, and can put another Python ahead
//! of the tree's environment, as conda's base or pyenv's shims do: the terminal's `python` is then not
//! the one the code help reads. The terminal's bash starts with orior's own file in place of a login
//! shell's, which reads /etc/profile and the first of ~/.bash_profile, ~/.bash_login and ~/.profile
//! that is there, as a login shell does, and only then puts the environment's folders first and sets
//! VIRTUAL_ENV. A path given in Windows' form is read through cygpath, as Git's bash takes paths.

use std::path::{Path, PathBuf};

/// orior's start of an interactive bash.
const BASH: &str = r#"if [ -n "$ORIOR_LOGIN" ]; then
  [ -r /etc/profile ] && . /etc/profile
  for one in ~/.bash_profile ~/.bash_login ~/.profile; do
    if [ -r "$one" ]; then
      . "$one"
      break
    fi
  done
elif [ -r ~/.bashrc ]; then
  . ~/.bashrc
fi
if [ -n "$ORIOR_PATH_FIRST" ]; then
  first="$ORIOR_PATH_FIRST"
  if command -v cygpath > /dev/null 2>&1; then
    first="$(cygpath -p "$first")"
  fi
  PATH="$first:$PATH"
  export PATH
fi
if [ -n "$ORIOR_VIRTUAL_ENV" ]; then
  VIRTUAL_ENV="$ORIOR_VIRTUAL_ENV"
  export VIRTUAL_ENV
fi
if [ -z "$PROMPT_COMMAND" ]; then
  PROMPT_COMMAND='printf '"'"'\033]7;file://%s%s\007'"'"' "${HOSTNAME:-}" "$PWD"'
fi
unset ORIOR_LOGIN ORIOR_PATH_FIRST ORIOR_VIRTUAL_ENV first one
"#;

/// orior's start of a bash, written to `folder` where it is not there as it stands, and its path.
pub fn bash_file(folder: &Path) -> Result<PathBuf, String> {
    let path = folder.join("bash_start.sh");
    if std::fs::read_to_string(&path).ok().as_deref() != Some(BASH) {
        std::fs::create_dir_all(folder).map_err(|error| format!("{}: {error}", folder.display()))?;
        std::fs::write(&path, BASH).map_err(|error| format!("{}: {error}", path.display()))?;
    }
    Ok(path)
}

/// The words bash starts with to read orior's file at `file`, where it starts as a login shell would
/// where `login`, and what is set for it: the tree's environment's folders and, for a virtual
/// environment, its folder.
pub fn bash_start(file: &Path, login: bool) -> (Vec<String>, Vec<(String, String)>) {
    let words = vec!["--init-file".to_string(), file.display().to_string(), "-i".to_string()];
    let mut env = Vec::new();
    if login {
        env.push(("ORIOR_LOGIN".to_string(), "1".to_string()));
    }
    if let Some(tree) = crate::toolchains::environment() {
        if let Ok(first) = std::env::join_paths(&tree.bin) {
            if !tree.bin.is_empty() {
                env.push(("ORIOR_PATH_FIRST".to_string(), first.to_string_lossy().into_owned()));
            }
        }
        // A virtual environment's folder holds pyvenv.cfg, a folder above its Python.
        if let Some(folder) = tree.python.as_deref().and_then(Path::parent).and_then(Path::parent).filter(|folder| folder.join("pyvenv.cfg").is_file()) {
            env.push(("ORIOR_VIRTUAL_ENV".to_string(), folder.display().to_string()));
        }
    }
    (words, env)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// What bash started with orior's file says to `asked`, with HOME at `home` and `env` set.
    fn said(file: &Path, home: &Path, env: &[(&str, String)], asked: &str) -> String {
        let mut command = std::process::Command::new(crate::runner::bash().unwrap());
        command.args(["--init-file"]).arg(file).args(["-i", "-c", asked]).env("HOME", home).env("CHERE_INVOKING", "1").stdin(std::process::Stdio::null());
        for (key, value) in env {
            command.env(key, value);
        }
        let out = command.output().unwrap();
        String::from_utf8_lossy(&out.stdout).trim().to_string()
    }

    #[test]
    fn the_tree_s_environment_stands_first_after_a_profile_that_puts_another_python_ahead() {
        let dir = std::env::temp_dir().join(format!("orior_shell_init_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let (home, other, venv) = (dir.join("home"), dir.join("other"), dir.join("venv").join("bin"));
        for folder in [&home, &other, &venv] {
            std::fs::create_dir_all(folder).unwrap();
        }
        std::fs::write(dir.join("venv").join("pyvenv.cfg"), "home = x\n").unwrap();
        // A python of each place, and a profile that puts the other one first, as conda's base does.
        for folder in [&other, &venv] {
            let python = folder.join("python");
            std::fs::write(&python, "#!/bin/sh\necho python\n").unwrap();
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                std::fs::set_permissions(&python, std::fs::Permissions::from_mode(0o755)).unwrap();
            }
        }
        let unix = |path: &Path| {
            let text = path.display().to_string().replace('\\', "/");
            match text.split_once(":/") {
                Some((drive, rest)) if drive.len() == 1 => format!("/{}/{rest}", drive.to_lowercase()),
                _ => text,
            }
        };
        std::fs::write(home.join(".bash_profile"), format!("export PATH=\"{}:$PATH\"\n", unix(&other))).unwrap();
        let file = bash_file(&dir).unwrap();
        let login = ("ORIOR_LOGIN", "1".to_string());
        let alone = said(&file, &home, std::slice::from_ref(&login), "command -v python");
        assert!(alone.ends_with("other/python"), "the profile's python where no environment is chosen: {alone}");
        let first = ("ORIOR_PATH_FIRST", venv.display().to_string());
        let virtual_env = ("ORIOR_VIRTUAL_ENV", venv.parent().unwrap().display().to_string());
        let chosen = said(&file, &home, &[login, first, virtual_env], "command -v python; echo \"$VIRTUAL_ENV\"; echo \"${ORIOR_PATH_FIRST:-gone}\"");
        let lines: Vec<&str> = chosen.lines().collect();
        assert!(lines[0].ends_with("venv/bin/python"), "the environment's python, after the profile ran: {chosen}");
        assert!(lines[1].ends_with("venv"), "VIRTUAL_ENV set as activating sets it: {chosen}");
        assert_eq!(lines[2], "gone", "orior's own words taken out: {chosen}");
        let _ = std::fs::remove_dir_all(&dir);
    }
}
