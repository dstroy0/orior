// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The windows open, as the command line finds them: each window keeps a file under `windows/` in
//! orior's home, readable by the reader alone, naming its process, the tree it has open, its comms
//! door and a held key of the door's that grants the channel `command` to speak on and `answer/*` to
//! listen on. A command line run on a tree with a window open says the command there, and the window
//! runs it as the menu runs it and answers on `answer/` and the command's id. A file whose window is
//! gone is taken away by the first command line that finds the door shut.

use std::path::{Path, PathBuf};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::comms::door::Client;
use crate::comms::Key;

/// How long a command line waits for the window's answer.
const ANSWER_WAIT: Duration = Duration::from_secs(10);

/// What a window keeps for the command line.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Kept {
    pub process: u32,
    pub root: Option<PathBuf>,
    pub address: String,
    pub key: String,
}

/// What the window answered: whether the command ran, and what it said.
#[derive(Debug, PartialEq)]
pub struct Answer {
    pub done: bool,
    pub said: String,
}

fn folder(home: &Path) -> PathBuf {
    home.join("windows")
}

/// Keeps `kept` for the command line in `home`, in a file the reader alone can read.
pub fn keep_in(home: &Path, kept: &Kept) -> Result<(), String> {
    let folder = folder(home);
    let said = |error: std::io::Error| format!("{}: {error}", folder.display());
    #[cfg(unix)]
    {
        use std::os::unix::fs::{DirBuilderExt, PermissionsExt};
        std::fs::DirBuilder::new().recursive(true).mode(0o700).create(&folder).map_err(said)?;
        std::fs::set_permissions(&folder, std::fs::Permissions::from_mode(0o700)).map_err(said)?;
    }
    #[cfg(not(unix))]
    std::fs::create_dir_all(&folder).map_err(said)?;
    let path = folder.join(format!("{}.json", kept.process));
    let beside = folder.join(format!(".{}.json", kept.process));
    let text = serde_json::to_string(kept).map_err(|error| error.to_string())?;
    #[cfg(unix)]
    {
        use std::io::Write;
        use std::os::unix::fs::OpenOptionsExt;
        let mut file = std::fs::OpenOptions::new().write(true).create(true).truncate(true).mode(0o600).open(&beside).map_err(said)?;
        file.write_all(text.as_bytes()).map_err(said)?;
    }
    #[cfg(not(unix))]
    std::fs::write(&beside, text).map_err(said)?;
    std::fs::rename(&beside, &path).map_err(said)
}

/// Keeps this window's door and key, and the tree it has open, in orior's home.
pub fn keep(root: Option<&Path>, address: &str, key: &Key) -> Result<(), String> {
    let home = crate::home::folder().ok_or("orior has no home folder")?;
    keep_in(&home, &Kept { process: std::process::id(), root: root.map(Path::to_path_buf), address: address.to_string(), key: key.text().to_string() })
}

/// Takes this window's file away, as it closes.
pub fn forget() {
    if let Some(home) = crate::home::folder() {
        let _ = std::fs::remove_file(folder(&home).join(format!("{}.json", std::process::id())));
    }
}

/// Every window kept in `home`.
pub fn kept_in(home: &Path) -> Vec<Kept> {
    let Ok(entries) = std::fs::read_dir(folder(home)) else { return Vec::new() };
    entries
        .flatten()
        .filter(|entry| entry.file_name().to_string_lossy().ends_with(".json") && !entry.file_name().to_string_lossy().starts_with('.'))
        .filter_map(|entry| serde_json::from_str(&std::fs::read_to_string(entry.path()).ok()?).ok())
        .collect()
}

/// The windows a command for `root` goes to: those with `root` open, or, with no tree named, the one
/// window open where there is only one.
pub fn windows_for(all: Vec<Kept>, root: Option<&Path>) -> Vec<Kept> {
    match root {
        Some(root) => all.into_iter().filter(|kept| kept.root.as_deref() == Some(root)).collect(),
        None if all.len() == 1 => all,
        None => Vec::new(),
    }
}

/// Says `command` of `menu`, with `args`, to a window open on `root`, and gives its answer; None
/// where no window is open there.
pub fn ask_in(home: &Path, root: Option<&Path>, menu: &str, command: &str, args: &[String]) -> Option<Result<Answer, String>> {
    for kept in windows_for(kept_in(home), root) {
        let Ok(client) = Client::join(&kept.address, &Key::from_text(&kept.key)) else {
            let _ = std::fs::remove_file(folder(home).join(format!("{}.json", kept.process)));
            continue;
        };
        return Some(ask_window(&client, menu, command, args));
    }
    None
}

/// Says the command to the window `client` has joined, and waits for its answer.
fn ask_window(client: &Client, menu: &str, command: &str, args: &[String]) -> Result<Answer, String> {
    let id: String = crate::comms::secret(8)?.iter().map(|byte| format!("{byte:02x}")).collect();
    let channel = format!("answer/{id}");
    client.listen(&channel)?;
    let heard = client.say("command", json!({"id": id, "menu": menu, "command": command, "args": args}))?;
    // the count says where the silence is: 0 where no part of the window listens on command any more, more where
    // the page heard and did not answer
    let answer = client.next(ANSWER_WAIT).ok_or_else(|| format!("the window gave no answer, heard by {heard}"))?;
    Ok(answer_of(&answer.body))
}

fn answer_of(body: &Value) -> Answer {
    Answer { done: body["done"].as_bool().unwrap_or(false), said: body["said"].as_str().unwrap_or("").to_string() }
}

/// As ask_in, in orior's home.
pub fn ask(root: Option<&Path>, menu: &str, command: &str, args: &[String]) -> Option<Result<Answer, String>> {
    ask_in(&crate::home::folder()?, root, menu, command, args)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::comms::{door, Comms};
    use std::sync::Arc;

    fn home(name: &str) -> PathBuf {
        let home = std::env::temp_dir().join(format!("orior_reach_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&home);
        std::fs::create_dir_all(&home).unwrap();
        home
    }

    #[test]
    fn a_command_line_finds_the_window_on_its_tree_and_the_window_answers() {
        let home = home("answers");
        let comms = Arc::new(Comms::default());
        let address = door::open(comms.clone()).unwrap();
        let window = comms.register("window", None, &["command"], &["answer/*"]).unwrap();
        let held = comms.register_held("command line", &["answer/*"], &["command"]).unwrap();
        let tree = home.join("tree");
        keep_in(&home, &Kept { process: 1, root: Some(tree.clone()), address: address.clone(), key: held.text().to_string() }).unwrap();
        let answering = comms.clone();
        let speaking = window.clone();
        comms
            .listen_with(&window, "command", move |message| {
                let body = &message.body;
                let said = format!("{} {} {}", body["menu"].as_str().unwrap(), body["command"].as_str().unwrap(), body["args"][0].as_str().unwrap());
                let _ = answering.say(&speaking, &format!("answer/{}", body["id"].as_str().unwrap()), json!({"done": true, "said": said}));
            })
            .unwrap();
        let answer = ask_in(&home, Some(&tree), "view", "word-wrap", &["on".to_string()]).unwrap().unwrap();
        assert_eq!(answer, Answer { done: true, said: "view word-wrap on".into() });
        assert!(ask_in(&home, Some(&home.join("other")), "view", "word-wrap", &[]).is_none(), "no window on another tree");
        assert!(ask_in(&home, None, "view", "word-wrap", &["off".to_string()]).is_some(), "the one window, with no tree named");
        std::fs::remove_dir_all(&home).unwrap();
    }

    #[test]
    fn a_window_gone_is_forgotten_by_the_first_command_line_to_find_its_door_shut() {
        let home = home("gone");
        let tree = home.join("tree");
        let shut = if cfg!(windows) { r"\\.\pipe\orior-comms-gone-0".to_string() } else { home.join("door").display().to_string() };
        keep_in(&home, &Kept { process: 2, root: Some(tree.clone()), address: shut, key: "00".into() }).unwrap();
        assert_eq!(kept_in(&home).len(), 1);
        assert!(ask_in(&home, Some(&tree), "file", "save", &[]).is_none());
        assert!(kept_in(&home).is_empty());
        std::fs::remove_dir_all(&home).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn the_kept_file_is_the_readers_alone() {
        use std::os::unix::fs::PermissionsExt;
        let home = home("alone");
        keep_in(&home, &Kept { process: 3, root: None, address: "a".into(), key: "k".into() }).unwrap();
        assert_eq!(std::fs::metadata(home.join("windows/3.json")).unwrap().permissions().mode() & 0o777, 0o600);
        assert_eq!(std::fs::metadata(home.join("windows")).unwrap().permissions().mode() & 0o777, 0o700);
        std::fs::remove_dir_all(&home).unwrap();
    }
}
