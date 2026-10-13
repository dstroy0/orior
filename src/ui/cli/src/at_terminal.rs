// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The commands of the window's menus that run at the terminal where no window is open on the tree:
//! Git's pushes, pulls, branches, stashes and cherry-picks, the tree's tests run, and the settings
//! carried to and from another machine. Each does what its menu does in the window, with the words
//! the command line gives in place of what the window would ask, and says what it did. A command the
//! window alone can do is not among them, and a window opens to run it.

use std::path::Path;
use std::sync::mpsc;
use std::sync::Arc;

use crate::{carry, git, test_runs, testing};

/// Whether `command` runs at the terminal where no window is open.
pub fn runs(command: &str) -> bool {
    matches!(
        command,
        "push" | "pull" | "branches" | "new-branch" | "branch-switch" | "branch-merge" | "branch-rebase" | "branch-rename" | "branch-delete"
            | "stash" | "stashes" | "stash-apply" | "stash-pop" | "stash-drop" | "cherry-pick"
            | "run-tests" | "cover-tests" | "test-run" | "test-cover"
            | "carry-send" | "carry-take" | "carry-undo"
    )
}

/// Runs `command` with `args` on the tree at `root`, as its menu runs it in the window, and gives
/// what it says.
pub fn run(root: &Path, command: &str, args: &[String]) -> Result<String, String> {
    let first = args.first().map(String::as_str).unwrap_or("");
    let words = args.join(" ");
    let named = |what: &str| if first.is_empty() { Err(format!("{command} takes {what}")) } else { Ok(first) };
    match command {
        "push" => git::push(root),
        "pull" => git::pull(root),
        "branches" => Ok(git::branches(root).iter().map(|branch| format!("{} {}{}", if branch.current { "*" } else { " " }, branch.name, if branch.subject.is_empty() { String::new() } else { format!("  {}", branch.subject) })).collect::<Vec<_>>().join("\n")),
        "new-branch" => git::branch_act(root, "create", named("the new branch's name")?, ""),
        "branch-switch" => git::branch_act(root, "switch", named("a branch")?, ""),
        "branch-merge" => git::branch_act(root, "merge", named("a branch")?, ""),
        "branch-rebase" => git::branch_act(root, "rebase", named("a branch")?, ""),
        "branch-rename" => match args.get(1) {
            Some(to) => git::branch_act(root, "rename", named("a branch")?, to),
            None => Err("branch-rename takes a branch and its new name".to_string()),
        },
        // A branch whose commits no other branch holds is deleted with --force, as the window asks
        // first before it deletes one.
        "branch-delete" => {
            let force = args.iter().any(|word| word == "--force");
            let name = args.iter().find(|word| *word != "--force").map(String::as_str).unwrap_or("");
            match git::branch_act(root, if force { "delete-unmerged" } else { "delete" }, name, "") {
                Err(said) if said.contains("not fully merged") => Err(format!("{name} has commits no other branch holds: branch-delete {name} --force deletes it and them")),
                done => done,
            }
        }
        "stash" => git::stash_act(root, "push", "", &words),
        "stashes" => Ok(git::stashes(root).iter().map(|stash| format!("{}  {}", stash.name, stash.subject)).collect::<Vec<_>>().join("\n")),
        "stash-apply" => git::stash_act(root, "apply", named("a stash")?, ""),
        "stash-pop" => git::stash_act(root, "pop", named("a stash")?, ""),
        "stash-drop" => git::stash_act(root, "drop", named("a stash")?, ""),
        // With no commit named, the commits of the other branches the branch open does not hold, to
        // name one of.
        "cherry-pick" if first.is_empty() => Ok(git::elsewhere(root).iter().map(|commit| format!("{}  {}", &commit.id[..commit.id.len().min(7)], commit.subject)).collect::<Vec<_>>().join("\n")),
        "cherry-pick" => git::cherry_pick(root, first),
        "run-tests" | "cover-tests" => {
            let mut files: Vec<String> = testing::found(root).into_iter().map(|test| test.file).collect();
            files.dedup();
            tests(root, files, command == "cover-tests")
        }
        "test-run" | "test-cover" if args.is_empty() => Err(format!("{command} takes the tests or files of tests to run")),
        "test-run" | "test-cover" => tests(root, args.to_vec(), command == "test-cover"),
        "carry-send" => carry::send(&home()?, Path::new(named("the file or folder the settings go to")?)),
        "carry-take" => take(&home()?, named("the file or folder the settings come from")?, &args[1..]),
        "carry-undo" => carry::undo(&home()?, named("a taking")?).map(|taking| format!("The taking from {} is undone.", taking.from)),
        _ => Err(format!("{command} runs in a window")),
    }
}

fn home() -> Result<std::path::PathBuf, String> {
    crate::home::folder().ok_or_else(|| "orior has no folder of its own".to_string())
}

/// What `from` carries that differs from this machine's taken in: each file and each setting of
/// settings.json named in `chosen`, or every one where none is named.
fn take(home: &Path, from: &str, chosen: &[String]) -> Result<String, String> {
    let there = carry::read(Path::new(from))?;
    let wanted = |name: &str| chosen.is_empty() || chosen.iter().any(|one| one == name);
    let differences = carry::differences(home, &there);
    let mut places: Vec<String> = differences.iter().filter(|one| one.how != "here" && one.place != "settings.json" && wanted(&one.place)).map(|one| one.place.clone()).collect();
    let keys: Vec<String> = differences.iter().flat_map(|one| one.settings.iter().map(|setting| setting.key.clone())).filter(|key| wanted(key)).collect();
    if !keys.is_empty() {
        places.push("settings.json".to_string());
    }
    if places.is_empty() {
        return Ok(format!("{from} holds the settings this machine has."));
    }
    let taking = carry::take(home, from, &there, &places, &keys)?;
    Ok(format!("Took {} file{} from {from}; carry-undo {} writes them back.", taking.files.len(), if taking.files.len() == 1 { "" } else { "s" }, taking.id))
}

/// Runs `given` and waits for its end: each test that failed with what it said, then the counts.
fn tests(root: &Path, given: Vec<String>, cover: bool) -> Result<String, String> {
    let runs = test_runs::Runs::default();
    let (heard, hearing) = mpsc::channel();
    let tell: test_runs::Tell = Arc::new(move |one| {
        let _ = heard.send(one);
    });
    runs.start(root, given, true, cover, tell)?;
    let mut failed = Vec::new();
    let mut covered = Vec::new();
    for one in hearing {
        match one {
            test_runs::Heard::Result { outcome, .. } if outcome.outcome == "failed" => failed.push(format!("FAILED {}: {}", outcome.id, outcome.message.lines().next().unwrap_or_default())),
            test_runs::Heard::Coverage { files, .. } => {
                covered = files.iter().map(|file| format!("{}: {} of {} lines ran", file.file, file.ran.len(), file.ran.len() + file.missed.len())).collect();
            }
            test_runs::Heard::Done { passed, failed: count, skipped, seconds, said, .. } => {
                let mut lines = failed;
                lines.extend(covered);
                lines.push(format!("{passed} passed, {count} failed, {skipped} skipped in {seconds:.1} s{}", if said.is_empty() { String::new() } else { format!(": {said}") }));
                // A run that ran no test did not do what it was asked.
                return if count == 0 && passed + skipped > 0 { Ok(lines.join("\n")) } else { Err(lines.join("\n")) };
            }
            _ => {}
        }
    }
    Err("The run of tests ended without its counts".to_string())
}

#[cfg(test)]
mod terminal {
    use super::{run, runs};
    use std::process::Command;

    fn repo(name: &str) -> std::path::PathBuf {
        let root = std::env::temp_dir().join(format!("orior_at_terminal_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        // The repository's own settings, whatever the machine's ask: a commit here is not signed.
        for args in [&["init", "-q", "-b", "main"][..], &["config", "user.name", "t"], &["config", "user.email", "t@t"], &["config", "commit.gpgsign", "false"]] {
            assert!(Command::new("git").args(args).current_dir(&root).status().unwrap().success());
        }
        std::fs::write(root.join("a.txt"), "one\n").unwrap();
        for args in [&["add", "."][..], &["commit", "-q", "-m", "first"]] {
            assert!(Command::new("git").args(args).current_dir(&root).status().unwrap().success());
        }
        root
    }

    fn words(text: &[&str]) -> Vec<String> {
        text.iter().map(|one| one.to_string()).collect()
    }

    #[test]
    fn a_window_command_runs_here_only_where_the_terminal_can_do_it() {
        assert!(runs("push") && runs("branch-switch") && runs("stash-drop") && runs("test-run") && runs("carry-undo"));
        assert!(!runs("word-wrap") && !runs("preferences") && !runs("debug"));
        let root = repo("refused");
        assert_eq!(run(&root, "word-wrap", &[]).unwrap_err(), "word-wrap runs in a window");
        assert_eq!(run(&root, "branch-switch", &[]).unwrap_err(), "branch-switch takes a branch");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn branches_are_made_listed_switched_renamed_and_deleted_and_changes_stashed_and_brought_back() {
        let root = repo("git");
        run(&root, "new-branch", &words(&["made"])).unwrap();
        assert!(run(&root, "branches", &[]).unwrap().lines().any(|line| line.starts_with("* made")));
        run(&root, "branch-switch", &words(&["main"])).unwrap();
        run(&root, "branch-rename", &words(&["made", "named"])).unwrap();
        assert!(run(&root, "branches", &[]).unwrap().contains("named"));
        run(&root, "branch-delete", &words(&["named"])).unwrap();
        assert!(!run(&root, "branches", &[]).unwrap().contains("named"));
        std::fs::write(root.join("a.txt"), "two\n").unwrap();
        run(&root, "stash", &words(&["put", "aside"])).unwrap();
        assert_eq!(std::fs::read_to_string(root.join("a.txt")).unwrap(), "one\n");
        assert!(run(&root, "stashes", &[]).unwrap().starts_with("stash@{0}"));
        run(&root, "stash-pop", &words(&["stash@{0}"])).unwrap();
        assert_eq!(std::fs::read_to_string(root.join("a.txt")).unwrap(), "two\n");
        assert_eq!(run(&root, "stashes", &[]).unwrap(), "");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn tests_run_here_and_say_each_failure_and_the_counts() {
        let root = repo("tests");
        // Cases of unittest's, which pytest and Python's own runner each take, whichever the machine has.
        std::fs::write(root.join("test_here.py"), "import unittest\n\n\nclass Here(unittest.TestCase):\n    def test_passes(self):\n        self.assertTrue(True)\n\n    def test_fails(self):\n        self.assertEqual(1, 2)\n").unwrap();
        std::fs::write(root.join("test_good.py"), "import unittest\n\n\nclass Good(unittest.TestCase):\n    def test_passes(self):\n        self.assertTrue(True)\n").unwrap();
        let said = run(&root, "test-run", &words(&["test_here.py"])).unwrap_err();
        assert!(said.lines().any(|line| line.starts_with("FAILED ") && line.contains("test_fails")), "{said}");
        assert!(said.contains("1 passed, 1 failed"), "{said}");
        let said = run(&root, "test-run", &words(&["test_good.py"])).unwrap();
        assert!(said.starts_with("1 passed, 0 failed"), "{said}");
        std::fs::write(root.join("test_none.py"), "x = 1\n").unwrap();
        assert!(run(&root, "test-run", &words(&["test_none.py"])).unwrap_err().starts_with("0 passed, 0 failed"), "a run that ran no test fails");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn settings_sent_from_one_machine_are_taken_in_on_another_and_the_taking_undone() {
        let base = std::env::temp_dir().join(format!("orior_at_terminal_carry_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&base);
        let (here, there, between) = (base.join("here"), base.join("there"), base.join("between"));
        for folder in [&here, &there, &between] {
            std::fs::create_dir_all(folder).unwrap();
        }
        std::fs::write(here.join("user.css"), "body { color: red; }\n").unwrap();
        crate::carry::send(&here, &between).unwrap();
        let said = super::take(&there, &between.display().to_string(), &[]).unwrap();
        assert!(said.starts_with("Took 1 file"), "{said}");
        assert_eq!(std::fs::read_to_string(there.join("user.css")).unwrap(), "body { color: red; }\n");
        assert_eq!(super::take(&there, &between.display().to_string(), &[]).unwrap(), format!("{} holds the settings this machine has.", between.display()));
        let id = said.split("carry-undo ").nth(1).unwrap().split_whitespace().next().unwrap();
        crate::carry::undo(&there, id).unwrap();
        assert!(!there.join("user.css").exists(), "the taking undone");
        let _ = std::fs::remove_dir_all(&base);
    }

    #[test]
    fn a_window_command_given_with_no_window_open_runs_here_where_it_can_and_opens_a_window_where_not() {
        let root = repo("command_line");
        let given = |text: &[&str]| crate::cli::run([&["--root", root.to_str().unwrap()][..], text].concat().iter().map(|word| word.to_string()).collect());
        assert!(matches!(given(&["git", "new-branch", "from-the-terminal"]), crate::cli::Outcome::Exit(0)));
        assert!(run(&root, "branches", &[]).unwrap().contains("* from-the-terminal"));
        assert!(matches!(given(&["view", "word-wrap", "on"]), crate::cli::Outcome::Window(_)));
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_branch_with_commits_of_its_own_is_deleted_only_when_forced_and_its_commit_picked_first() {
        let root = repo("unmerged");
        run(&root, "new-branch", &words(&["side"])).unwrap();
        std::fs::write(root.join("b.txt"), "side\n").unwrap();
        for args in [&["add", "."][..], &["commit", "-q", "-m", "on the side"]] {
            assert!(Command::new("git").args(args).current_dir(&root).status().unwrap().success());
        }
        run(&root, "branch-switch", &words(&["main"])).unwrap();
        let listed = run(&root, "cherry-pick", &[]).unwrap();
        assert!(listed.ends_with("on the side"), "{listed}");
        let refused = run(&root, "branch-delete", &words(&["side"])).unwrap_err();
        assert!(refused.contains("--force"), "{refused}");
        run(&root, "cherry-pick", &words(&[listed.split_whitespace().next().unwrap()])).unwrap();
        assert!(root.join("b.txt").exists(), "the commit picked onto main");
        run(&root, "branch-delete", &words(&["side", "--force"])).unwrap();
        assert!(!run(&root, "branches", &[]).unwrap().lines().any(|line| line[2..].split("  ").next() == Some("side")), "side deleted");
        let _ = std::fs::remove_dir_all(&root);
    }
}
