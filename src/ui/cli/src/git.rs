// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What git says of the tree: the branch it is on, the files that differ from the last commit and
//! how, the commits that touched a file, every branch's commits laid out as a graph and the files each
//! changed, the commit that last changed each line of a file, and a file's text as one of those
//! commits left it or as the last did. In a tree git cannot read, each of these comes back empty.
//! What git is asked to do, for the Commit window and the Git menu: commit chosen files, push, pull
//! where nothing would merge, put a file back as the last commit left it, act on branches, put changes
//! aside in a stash and bring them back, and apply a commit of another branch, each giving git's own
//! words where it refuses. Here too is the clone of a repository into a folder of its own, for File,
//! Clone Repository and `orior file clone`.

use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use serde::Serialize;

use crate::root::inside;

/// The most commits a file's list holds.
const COMMITS_LIMIT: usize = 200;

/// A file that differs from the last commit. Its state is one letter: M changed, A added, D gone,
/// R renamed, U not yet tracked, C in conflict.
#[derive(Serialize)]
pub struct Changed {
    pub path: String,
    pub state: char,
}

/// One commit that touched a file: its id, when it was made in seconds since 1970, and its subject.
#[derive(Serialize)]
pub struct Commit {
    pub id: String,
    pub when: i64,
    pub subject: String,
}

fn git(root: &Path, args: &[&str]) -> Option<Vec<u8>> {
    let mut command = Command::new("git");
    // A question takes no lock it can do without: a status asked while the window watches the tree,
    // or stopped as the window closes, leaves the index's lock to no one.
    command.args(args).current_dir(root).env("GIT_OPTIONAL_LOCKS", "0");
    command.stdin(Stdio::null()).stderr(Stdio::null());
    crate::runner::quiet(&mut command);
    command.output().ok().filter(|out| out.status.success()).map(|out| out.stdout)
}

/// The state letter for a porcelain status's two columns.
fn state_of(index: u8, tree: u8) -> char {
    match (index, tree) {
        (b'?', b'?') => 'U',
        (b'U', _) | (_, b'U') | (b'A', b'A') | (b'D', b'D') => 'C',
        (b'D', _) | (_, b'D') => 'D',
        (b'R', _) => 'R',
        (b'A', _) => 'A',
        _ => 'M',
    }
}

/// The branch the tree is on, or the short id of the commit it stands at where it is on none.
pub fn branch(root: &Path) -> Option<String> {
    let named = git(root, &["rev-parse", "--abbrev-ref", "HEAD"]).map(|out| String::from_utf8_lossy(&out).trim().to_string())?;
    if named != "HEAD" {
        return Some(named);
    }
    git(root, &["rev-parse", "--short", "HEAD"]).map(|out| String::from_utf8_lossy(&out).trim().to_string())
}

/// Every file under the tree that differs from the last commit, by its path in the tree.
pub fn changed(root: &Path) -> Vec<Changed> {
    let prefix = git(root, &["rev-parse", "--show-prefix"]).map(|out| String::from_utf8_lossy(&out).trim().to_string()).unwrap_or_default();
    let Some(out) = git(root, &["status", "--porcelain=v1", "-z", "--untracked-files=all", "--", "."]) else {
        return Vec::new();
    };
    let text = String::from_utf8_lossy(&out);
    let mut records = text.split('\0').filter(|record| !record.is_empty());
    let mut found = Vec::new();
    while let Some(record) = records.next() {
        let bytes = record.as_bytes();
        if bytes.len() < 4 {
            continue;
        }
        let (index, tree) = (bytes[0], bytes[1]);
        // A rename or a copy is followed by the path it came from.
        if index == b'R' || index == b'C' {
            records.next();
        }
        if let Some(path) = record[3..].strip_prefix(prefix.as_str()) {
            found.push(Changed { path: path.to_string(), state: state_of(index, tree) });
        }
    }
    found
}

/// Runs git at `root` with nothing to answer a prompt with, and gives what it wrote, or what it said
/// went wrong.
fn run(root: &Path, args: &[&str]) -> Result<String, String> {
    let mut command = Command::new("git");
    command.args(args).current_dir(root).env("GIT_TERMINAL_PROMPT", "0").stdin(Stdio::null());
    crate::runner::quiet(&mut command);
    let out = command.output().map_err(|error| format!("git: {error}"))?;
    let said = format!("{}{}", String::from_utf8_lossy(&out.stdout), String::from_utf8_lossy(&out.stderr));
    if out.status.success() {
        Ok(said.trim().to_string())
    } else {
        Err(if said.trim().is_empty() { format!("git {} failed", args.first().unwrap_or(&"")) } else { said.trim().to_string() })
    }
}

/// Commits the files at `paths` with `message`, each as it stands, whether changed, new or gone, and
/// leaves every other file as it was. Gives git's line for the commit made.
pub fn commit(root: &Path, message: &str, paths: &[String]) -> Result<String, String> {
    if message.trim().is_empty() {
        return Err("a commit needs a message".into());
    }
    if paths.is_empty() {
        return Err("no file is chosen to commit".into());
    }
    for path in paths {
        inside(root, path)?;
    }
    let shown: Vec<String> = paths.iter().map(|path| format!("./{path}")).collect();
    let mut add = vec!["add", "-A", "--"];
    add.extend(shown.iter().map(String::as_str));
    run(root, &add)?;
    let mut commit = vec!["commit", "-m", message.trim(), "--"];
    commit.extend(shown.iter().map(String::as_str));
    let said = run(root, &commit)?;
    Ok(said.lines().find(|line| line.starts_with('[')).unwrap_or(said.lines().next().unwrap_or_default()).to_string())
}

/// A branch, local or remote: its short name, whether the tree is on it, the branch it follows, and
/// when its last commit was made and its subject.
#[derive(Serialize)]
pub struct Branch {
    pub name: String,
    pub remote: bool,
    pub current: bool,
    pub upstream: String,
    pub when: i64,
    pub subject: String,
}

/// Every local and remote branch, the one with the newest commit first, less each remote's HEAD.
pub fn branches(root: &Path) -> Vec<Branch> {
    let format = "--format=%(refname)%00%(refname:short)%00%(HEAD)%00%(upstream:short)%00%(committerdate:unix)%00%(subject)";
    let Some(out) = git(root, &["for-each-ref", "--sort=-committerdate", format, "refs/heads", "refs/remotes"]) else {
        return Vec::new();
    };
    String::from_utf8_lossy(&out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.split('\0');
            let full = parts.next()?;
            let name = parts.next()?.to_string();
            if full.starts_with("refs/remotes/") && full.ends_with("/HEAD") {
                return None;
            }
            Some(Branch {
                remote: full.starts_with("refs/remotes/"),
                current: parts.next()? == "*",
                upstream: parts.next()?.to_string(),
                when: parts.next()?.parse().unwrap_or(0),
                subject: parts.next().unwrap_or_default().to_string(),
                name,
            })
        })
        .collect()
}

/// Does to a branch what the branches list asks: `create` makes one from the branch open and goes on
/// it, `switch` goes on one, a remote one made a local branch that follows it, `rename` names it `to`,
/// `delete` takes it out where its commits are merged and `delete-unmerged` where they are not,
/// `merge` merges it into the branch open, and `rebase` sets the branch open's own commits on it.
/// Gives git's own words.
pub fn branch_act(root: &Path, act: &str, name: &str, to: &str) -> Result<String, String> {
    if name.is_empty() || name.starts_with('-') || to.starts_with('-') {
        return Err(format!("{name} is no branch's name"));
    }
    let named = |branch: &str| run(root, &["check-ref-format", "--branch", branch]).map(drop);
    match act {
        "create" => {
            named(name)?;
            run(root, &["switch", "-c", name])
        }
        "switch" => {
            let remote = run(root, &["show-ref", "--verify", "--quiet", &format!("refs/remotes/{name}")]).is_ok();
            if remote { run(root, &["switch", "--track", name]) } else { run(root, &["switch", name]) }
        }
        "rename" => {
            named(to)?;
            run(root, &["branch", "-m", name, to])
        }
        "delete" => run(root, &["branch", "-d", name]),
        "delete-unmerged" => run(root, &["branch", "-D", name]),
        "merge" => run(root, &["merge", "--no-edit", name]),
        "rebase" => run(root, &["rebase", name]),
        _ => Err(format!("{act} is nothing a branch is asked to do")),
    }
}

/// A stash, changes put aside: its name, `stash@{0}` the newest, when it was made in seconds since
/// 1970, and git's line for it, the branch it was made on and its message.
#[derive(Serialize)]
pub struct Stash {
    pub name: String,
    pub when: i64,
    pub subject: String,
}

/// Every stash, the newest first.
pub fn stashes(root: &Path) -> Vec<Stash> {
    let Some(out) = git(root, &["stash", "list", "--format=%gd%x1f%ct%x1f%gs"]) else {
        return Vec::new();
    };
    String::from_utf8_lossy(&out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(3, '\u{1f}');
            Some(Stash { name: parts.next()?.to_string(), when: parts.next()?.parse().unwrap_or(0), subject: parts.next().unwrap_or_default().to_string() })
        })
        .collect()
}

/// Does to the stashes what is asked: `push` puts every change aside, new files with them, under
/// `message` where one is given; `apply` brings stash `name` back and keeps it; `pop` brings it back
/// and drops it where it comes back whole; `drop` throws it away. Gives git's own words.
pub fn stash_act(root: &Path, act: &str, name: &str, message: &str) -> Result<String, String> {
    let named = name.strip_prefix("stash@{").and_then(|rest| rest.strip_suffix('}')).is_some_and(|n| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()));
    if act != "push" && !named {
        return Err(format!("{name} is no stash's name"));
    }
    match act {
        "push" if message.trim().is_empty() => run(root, &["stash", "push", "--include-untracked"]),
        "push" => run(root, &["stash", "push", "--include-untracked", "-m", message.trim()]),
        "apply" | "pop" | "drop" => run(root, &["stash", act, name]),
        _ => Err(format!("{act} is nothing a stash is asked to do")),
    }
}

/// The commits of other branches, local and remote, that the branch the tree is on does not hold, merges
/// aside, the newest first. A commit already applied to it under another id is not one: a cherry-pick
/// keeps the author, the time the author made it and the message, and a commit of the branch open
/// made since the oldest of them with all three the same is taken for it, which reads no changes.
pub fn elsewhere(root: &Path) -> Vec<Commit> {
    let limit = format!("-n{COMMITS_LIMIT}");
    let format = "--format=%H%x1f%ct%x1f%ae%x1f%at%x1f%B%x1e";
    let Some(out) = git(root, &["log", "--branches", "--remotes", "--not", "HEAD", "--no-merges", &limit, format]) else {
        return Vec::new();
    };
    let read = |out: &[u8]| -> Vec<(Commit, String)> {
        String::from_utf8_lossy(out)
            .split('\u{1e}')
            .filter_map(|record| {
                let mut parts = record.trim_start_matches('\n').splitn(5, '\u{1f}');
                let id = parts.next()?.to_string();
                let when = parts.next()?.parse().ok()?;
                let made = format!("{}\u{1f}{}\u{1f}{}", parts.next()?, parts.next()?, parts.next()?.trim_end());
                let subject = made.rsplit('\u{1f}').next().unwrap_or_default().lines().next().unwrap_or_default().to_string();
                Some((Commit { id, when, subject }, made))
            })
            .collect()
    };
    let found = read(&out);
    let Some(oldest) = found.iter().map(|(commit, _)| commit.when).min() else {
        return Vec::new();
    };
    let since = format!("--since=@{oldest}");
    let ours: std::collections::HashSet<String> = git(root, &["log", "HEAD", "--no-merges", &since, format]).map(|out| read(&out).into_iter().map(|(_, made)| made).collect()).unwrap_or_default();
    found.into_iter().filter(|(_, made)| !ours.contains(made)).map(|(commit, _)| commit).collect()
}

/// Applies commit `id` to the branch the tree is on as a commit of its own. Gives git's own words, and
/// where the change does not fit, leaves the files in conflict for the merge window, and a commit
/// after they are resolved finishes it.
pub fn cherry_pick(root: &Path, id: &str) -> Result<String, String> {
    if id.is_empty() || !id.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err(format!("{id} is not a commit"));
    }
    run(root, &["cherry-pick", id])
}

/// A file the next commit takes in part: its path and the text the commit gives it.
#[derive(serde::Deserialize)]
pub struct Part {
    pub path: String,
    pub text: String,
}

/// Runs git at `root` with an index of its own, `index`, and `input` on its standard input, giving what
/// it wrote or what it said went wrong.
fn run_on(root: &Path, index: &Path, args: &[&str], input: Option<&str>) -> Result<String, String> {
    use std::io::Write;
    let mut command = Command::new("git");
    command.args(args).current_dir(root).env("GIT_TERMINAL_PROMPT", "0").env("GIT_INDEX_FILE", index);
    command.stdin(if input.is_some() { Stdio::piped() } else { Stdio::null() }).stdout(Stdio::piped()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| format!("git: {error}"))?;
    if let (Some(input), Some(mut stdin)) = (input, child.stdin.take()) {
        stdin.write_all(input.as_bytes()).map_err(|error| format!("git: {error}"))?;
    }
    let out = child.wait_with_output().map_err(|error| format!("git: {error}"))?;
    let said = format!("{}{}", String::from_utf8_lossy(&out.stdout), String::from_utf8_lossy(&out.stderr));
    if out.status.success() { Ok(said.trim().to_string()) } else { Err(said.trim().to_string()) }
}

/// Commits the files of `whole` as each stands and the files of `parts` with the text each is given,
/// leaving everything else as the last commit had it, in what is staged as well. The commit is made
/// from an index of its own, read from the last commit; after it, the tree's index takes the commit's
/// text for each file committed, and a file committed in part keeps the rest of its changes unstaged.
pub fn commit_parts(root: &Path, message: &str, whole: &[String], parts: &[Part]) -> Result<String, String> {
    if message.trim().is_empty() {
        return Err("a commit needs a message".into());
    }
    if whole.is_empty() && parts.is_empty() {
        return Err("no file is chosen to commit".into());
    }
    for path in whole.iter().chain(parts.iter().map(|part| &part.path)) {
        inside(root, path)?;
    }
    let index = PathBuf::from(run(root, &["rev-parse", "--git-path", "orior-commit-index"])?);
    let index = if index.is_absolute() { index } else { root.join(index) };
    let made = (|| {
        if run_on(root, &index, &["read-tree", "HEAD"], None).is_err() {
            run_on(root, &index, &["read-tree", "--empty"], None)?;
        }
        if !whole.is_empty() {
            let mut add = vec!["add", "-A", "--"];
            let shown: Vec<String> = whole.iter().map(|path| format!("./{path}")).collect();
            add.extend(shown.iter().map(String::as_str));
            run_on(root, &index, &add, None)?;
        }
        for part in parts {
            let blob = run_on(root, &index, &["hash-object", "-w", "--stdin"], Some(&part.text))?;
            let listed = run_on(root, &index, &["ls-files", "-s", "--", &part.path], None)?;
            let mode = listed.split_whitespace().next().filter(|mode| !mode.is_empty()).unwrap_or("100644").to_string();
            run_on(root, &index, &["update-index", "--add", "--cacheinfo", &format!("{mode},{blob},{}", part.path)], None)?;
        }
        run_on(root, &index, &["commit", "-m", message.trim()], None)
    })();
    let _ = std::fs::remove_file(&index);
    let said = made?;
    let mut reset = vec!["reset", "-q", "--"];
    let shown: Vec<String> = whole.iter().chain(parts.iter().map(|part| &part.path)).map(|path| format!("./{path}")).collect();
    reset.extend(shown.iter().map(String::as_str));
    run(root, &reset)?;
    Ok(said.lines().find(|line| line.starts_with('[')).unwrap_or(said.lines().next().unwrap_or_default()).to_string())
}

/// Marks a file a merge left in conflict resolved, as it now stands, by staging it.
pub fn resolve(root: &Path, path: &str) -> Result<(), String> {
    inside(root, path)?;
    run(root, &["add", "--", &format!("./{path}")]).map(drop)
}

/// Pushes the branch to the remote it follows, or to origin under its own name where it follows none.
pub fn push(root: &Path) -> Result<String, String> {
    if run(root, &["rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"]).is_ok() {
        run(root, &["push"])
    } else {
        run(root, &["push", "-u", "origin", "HEAD"])
    }
}

/// Brings in the commits of the remote branch the tree's follows, only where its own commits are all
/// among them, so that nothing is merged or rewritten.
pub fn pull(root: &Path) -> Result<String, String> {
    run(root, &["pull", "--ff-only"])
}

/// How many commits the branch has that its remote does not, and the other way, or None where it
/// follows no remote.
pub fn ahead_behind(root: &Path) -> Option<(u32, u32)> {
    let said = git(root, &["rev-list", "--left-right", "--count", "HEAD...@{u}"])?;
    let text = String::from_utf8_lossy(&said);
    let mut counts = text.split_whitespace().filter_map(|count| count.parse().ok());
    Some((counts.next()?, counts.next()?))
}

/// Puts the file at `path` back as the last commit left it, taking back its changes in the tree and
/// in what is staged. A file the last commit does not hold is refused, as taking it back would delete
/// it.
pub fn rollback(root: &Path, path: &str) -> Result<(), String> {
    inside(root, path)?;
    let shown = format!("./{path}");
    if git(root, &["cat-file", "-e", &format!("HEAD:{shown}")]).is_none() {
        return Err(format!("{path} is not in the last commit: delete it from the tree to take it back"));
    }
    run(root, &["restore", "--source=HEAD", "--staged", "--worktree", "--", &shown]).map(|_| ())
}

/// The commits that touched `file`, the newest first, following it through renames.
pub fn commits(root: &Path, file: &str) -> Result<Vec<Commit>, String> {
    inside(root, file)?;
    let limit = format!("-n{COMMITS_LIMIT}");
    let shown = format!("./{file}");
    Ok(git(root, &["log", "--follow", &limit, "--format=%H%x1f%ct%x1f%s", "--", &shown]).map(|out| commits_of(&out)).unwrap_or_default())
}

fn commits_of(out: &[u8]) -> Vec<Commit> {
    String::from_utf8_lossy(out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(3, '\u{1f}');
            let id = parts.next()?.to_string();
            let when = parts.next()?.parse().ok()?;
            let subject = parts.next().unwrap_or_default().to_string();
            Some(Commit { id, when, subject })
        })
        .collect()
}

/// The most commits the graph draws, and the most a search over them finds.
const GRAPH_LIMIT: usize = 2000;

/// One commit of the graph: its id, its parents, its author, when its author made it in seconds
/// since 1970, the branches and tags that stand at it, and its subject. `lane` is the column its dot
/// stands in and `color` the number of the line it is on. `lines` are the strokes of its row, each
/// `[from, to, half, color]`: from column `from` at the row's top to column `to` at its middle where
/// `half` is 0, from `from` at the middle to `to` at the bottom where it is 1.
#[derive(Serialize, Debug)]
pub struct Drawn {
    pub id: String,
    pub parents: Vec<String>,
    pub author: String,
    pub when: i64,
    pub refs: Vec<String>,
    pub subject: String,
    pub lane: usize,
    pub color: usize,
    pub lines: Vec<[usize; 4]>,
}

/// The commits of every branch, the newest first and none before its children, laid out as a graph.
/// With a `query`, the commits whose subject, author, id or branches hold each of its words, in any
/// case, from the whole history, each on a line of its own.
pub fn graph(root: &Path, query: &str) -> Vec<Drawn> {
    let limit = format!("-n{GRAPH_LIMIT}");
    let mut args = vec!["log", "--all", "--date-order", "--format=%H%x1f%P%x1f%an%x1f%at%x1f%D%x1f%s"];
    let words: Vec<String> = query.split_whitespace().map(str::to_lowercase).collect();
    if words.is_empty() {
        args.insert(1, &limit);
    }
    let Some(out) = git(root, &args) else {
        return Vec::new();
    };
    let mut drawn = drawn_of(&out);
    if words.is_empty() {
        lay_out(&mut drawn);
        return drawn;
    }
    drawn.retain(|commit| {
        let held = format!("{} {} {} {}", commit.subject, commit.author, commit.id, commit.refs.join(" ")).to_lowercase();
        words.iter().all(|word| held.contains(word.as_str()))
    });
    drawn.truncate(GRAPH_LIMIT);
    drawn
}

fn drawn_of(out: &[u8]) -> Vec<Drawn> {
    String::from_utf8_lossy(out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(6, '\u{1f}');
            let id = parts.next()?.to_string();
            let parents = parts.next()?.split_whitespace().map(str::to_string).collect();
            let author = parts.next()?.to_string();
            let when = parts.next()?.parse().ok()?;
            let refs = parts.next()?.split(", ").filter(|one| !one.is_empty()).map(str::to_string).collect();
            let subject = parts.next().unwrap_or_default().to_string();
            Some(Drawn { id, parents, author, when, refs, subject, lane: 0, color: 0, lines: Vec::new() })
        })
        .collect()
}

/// Gives each commit its column and the strokes of its row. Each column holds the commit it waits
/// for, the next on its line, and the line's color. A commit takes the first column waiting for it,
/// or the first free one where none waits, a branch's newest commit; every other column waiting for
/// it ends at it. Its first parent goes on in its column, or, where another column already waits for
/// that parent, joins that column; each further parent, a merge's, joins the column that waits for it
/// or opens one of its own.
fn lay_out(drawn: &mut [Drawn]) {
    let mut lanes: Vec<Option<(String, usize)>> = Vec::new();
    let mut colors = 0;
    let fresh = |colors: &mut usize| {
        *colors += 1;
        *colors - 1
    };
    let free = |lanes: &mut Vec<Option<(String, usize)>>| {
        lanes.iter().position(Option::is_none).unwrap_or_else(|| {
            lanes.push(None);
            lanes.len() - 1
        })
    };
    for commit in drawn.iter_mut() {
        let waiting: Vec<usize> = lanes.iter().enumerate().filter(|(_, lane)| lane.as_ref().is_some_and(|(id, _)| *id == commit.id)).map(|(at, _)| at).collect();
        let (lane, color) = match waiting.first() {
            Some(&at) => (at, lanes[at].as_ref().map_or(0, |(_, color)| *color)),
            None => (free(&mut lanes), fresh(&mut colors)),
        };
        for (at, held) in lanes.iter().enumerate() {
            if let Some((id, line)) = held {
                commit.lines.push([at, if *id == commit.id { lane } else { at }, 0, *line]);
            }
        }
        for &at in &waiting {
            lanes[at] = None;
        }
        let mut opened = Vec::new();
        for (nth, parent) in commit.parents.iter().enumerate() {
            let joins = lanes.iter().position(|held| held.as_ref().is_some_and(|(id, _)| id == parent));
            match (nth, joins) {
                (_, Some(at)) => commit.lines.push([lane, at, 1, lanes[at].as_ref().map_or(0, |(_, line)| *line)]),
                (0, None) => {
                    lanes[lane] = Some((parent.clone(), color));
                    opened.push(lane);
                    commit.lines.push([lane, lane, 1, color]);
                }
                (_, None) => {
                    let at = free(&mut lanes);
                    let line = fresh(&mut colors);
                    lanes[at] = Some((parent.clone(), line));
                    opened.push(at);
                    commit.lines.push([lane, at, 1, line]);
                }
            }
        }
        for (at, held) in lanes.iter().enumerate() {
            if let Some((_, line)) = held.as_ref().filter(|_| !opened.contains(&at)) {
                commit.lines.push([at, at, 1, *line]);
            }
        }
        while lanes.last().is_some_and(Option::is_none) {
            lanes.pop();
        }
        commit.lane = lane;
        commit.color = color;
    }
}

/// A file a commit changed from its first parent, or from nothing where it has none: its path under
/// the tree, its state letter (M changed, A added, D gone, R renamed), and the path it had before
/// where it was renamed.
#[derive(Serialize)]
pub struct Touched {
    pub path: String,
    pub state: char,
    pub was: Option<String>,
}

/// The files under the tree that commit `id` changed.
pub fn touched(root: &Path, id: &str) -> Result<Vec<Touched>, String> {
    if id.is_empty() || !id.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err(format!("{id} is not a commit"));
    }
    let parent = format!("{id}^1");
    let from = git(root, &["rev-parse", "-q", "--verify", &parent]).is_some();
    let mut args = vec!["diff-tree", "-r", "-M", "--name-status", "-z", "--relative", "--no-commit-id"];
    if from {
        args.push(&parent);
    } else {
        args.push("--root");
    }
    args.push(id);
    let out = git(root, &args).ok_or_else(|| format!("{id} is not a commit"))?;
    let text = String::from_utf8_lossy(&out);
    let mut fields = text.split('\0').filter(|field| !field.is_empty());
    let mut found = Vec::new();
    while let Some(status) = fields.next() {
        let state = status.chars().next().unwrap_or('M');
        let Some(first) = fields.next() else {
            break;
        };
        if state == 'R' || state == 'C' {
            let Some(path) = fields.next() else {
                break;
            };
            found.push(Touched { path: path.to_string(), state: if state == 'R' { 'R' } else { 'A' }, was: Some(first.to_string()).filter(|_| state == 'R') });
        } else {
            found.push(Touched { path: first.to_string(), state: if matches!(state, 'A' | 'D') { state } else { 'M' }, was: None });
        }
    }
    Ok(found)
}

/// A commit that last changed some line of a file: its id, author, when its author made it in seconds
/// since 1970, and subject; the file's path under the tree as the commit left it, and the commit
/// before it with the path the file had there, where there is one.
#[derive(Serialize, Default, Clone)]
pub struct LineCommit {
    pub id: String,
    pub author: String,
    pub when: i64,
    pub subject: String,
    pub path: Option<String>,
    pub parent: Option<String>,
    pub was: Option<String>,
}

/// The commit that last changed each line of a file: `lines` holds, for each line, its commit's
/// place in `commits`, or -1 for a line no commit holds yet.
#[derive(Serialize, Default)]
pub struct LineHistory {
    pub commits: Vec<LineCommit>,
    pub lines: Vec<i64>,
}

/// Runs git at `root` with `input` on its standard input, written as git reads it, and gives what it
/// wrote where it succeeds.
fn git_fed(root: &Path, args: &[&str], input: String) -> Option<Vec<u8>> {
    use std::io::Write;
    let mut command = Command::new("git");
    command.args(args).current_dir(root).env("GIT_OPTIONAL_LOCKS", "0");
    command.stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().ok()?;
    let mut stdin = child.stdin.take()?;
    let writer = std::thread::spawn(move || stdin.write_all(input.as_bytes()));
    let out = child.wait_with_output().ok()?;
    writer.join().ok()?.ok()?;
    out.status.success().then_some(out.stdout)
}

/// The commit that last changed each line of `file` as `text` holds it, the file as the editor has it,
/// edits and all, a line the last commit does not hold being no commit's.
pub fn line_history(root: &Path, file: &str, text: String) -> Result<LineHistory, String> {
    inside(root, file)?;
    let prefix = git(root, &["rev-parse", "--show-prefix"]).map(|out| String::from_utf8_lossy(&out).trim().to_string()).unwrap_or_default();
    let shown = format!("./{file}");
    let out = git_fed(root, &["blame", "--porcelain", "--contents", "-", "--", &shown], text).ok_or_else(|| format!("git has no history of {file}"))?;
    Ok(history_of(&String::from_utf8_lossy(&out), &prefix))
}

/// Reads `git blame --porcelain`: a line of a commit's id and the line's numbers before each line of
/// the file, the commit's own lines the first time it comes, and the line itself after a tab.
fn history_of(out: &str, prefix: &str) -> LineHistory {
    let mut history = LineHistory::default();
    let mut places: std::collections::HashMap<String, usize> = std::collections::HashMap::new();
    let mut at = 0;
    let under = |path: &str| path.strip_prefix(prefix).map(str::to_string);
    for line in out.lines() {
        if line.starts_with('\t') {
            let held = history.commits.get(at).is_some_and(|commit| !commit.id.bytes().all(|b| b == b'0'));
            history.lines.push(if held { at as i64 } else { -1 });
            continue;
        }
        let (key, value) = line.split_once(' ').unwrap_or((line, ""));
        if key.len() == 40 && key.bytes().all(|b| b.is_ascii_hexdigit()) {
            at = *places.entry(key.to_string()).or_insert_with(|| {
                history.commits.push(LineCommit { id: key.to_string(), ..LineCommit::default() });
                history.commits.len() - 1
            });
            continue;
        }
        let Some(commit) = history.commits.get_mut(at) else {
            continue;
        };
        match key {
            "author" => commit.author = value.to_string(),
            "author-time" => commit.when = value.parse().unwrap_or(0),
            "summary" => commit.subject = value.to_string(),
            "filename" => commit.path = under(value),
            "previous" => {
                let (parent, path) = value.split_once(' ').unwrap_or((value, ""));
                commit.parent = Some(parent.to_string());
                commit.was = under(path);
            }
            _ => {}
        }
    }
    history
}

/// The text of `file` as commit `id` left it.
pub fn text_at(root: &Path, file: &str, id: &str) -> Result<String, String> {
    inside(root, file)?;
    if id.is_empty() || !id.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err(format!("{id} is not a commit"));
    }
    git(root, &["show", &format!("{id}:./{file}")])
        .map(|out| String::from_utf8_lossy(&out).into_owned())
        .ok_or_else(|| format!("{file} is not in {id}"))
}

/// A file's text as the last commit left it, or nothing where the tree is not git's or the last
/// commit does not hold the file.
pub fn head_text(root: &Path, file: &str) -> Option<String> {
    inside(root, file).ok()?;
    git(root, &["show", &format!("HEAD:./{file}")]).map(|out| String::from_utf8_lossy(&out).into_owned())
}

/// The repository File, Clone Repository offers first.
pub const ORIOR: &str = "https://github.com/dstroy0/orior.git";

/// The folder a clone goes in where none is named: the reader's home.
pub fn clone_parent() -> PathBuf {
    let home = if cfg!(windows) { "USERPROFILE" } else { "HOME" };
    std::env::var_os(home).map(PathBuf::from).unwrap_or_else(|| std::env::current_dir().unwrap_or_default())
}

/// The folder File, Open, Repository clones into: one of orior's own, apart from the reader's.
pub fn opened_parent() -> Option<PathBuf> {
    crate::home::folder().map(|folder| folder.join("repositories"))
}

/// Makes `folder` a repository, as git init does, and answers what git said. A folder that is a
/// repository already is left as it is.
pub fn init(folder: &Path) -> Result<String, String> {
    if folder.join(".git").exists() {
        return Err(format!("{} is a repository already", folder.display()));
    }
    run(folder, &["init"])
}

/// The folder a clone of `url` makes under `parent`: the last part of the address, without `.git`,
/// as git itself names it.
pub fn clone_folder(url: &str, parent: &Path) -> Result<PathBuf, String> {
    let address = url.trim().trim_end_matches(['/', '\\']);
    let name = address.strip_suffix(".git").unwrap_or(address).rsplit(['/', '\\', ':']).next().unwrap_or("");
    if name.is_empty() || name == "." || name == ".." {
        return Err(format!("{url} names no repository"));
    }
    Ok(parent.join(name))
}

/// Clones `url` into `target`, a folder that is not there yet or is empty, with the git orior's runs
/// get. Each line of git's progress goes to `said` as it comes, with whether the line is finished: a
/// line git ends with a carriage return is one it writes over with the next. Answers the folder, or
/// the first line git marked fatal.
pub fn clone(url: &str, target: &Path, mut said: impl FnMut(&str, bool)) -> Result<PathBuf, String> {
    if target.read_dir().is_ok_and(|mut held| held.next().is_some()) {
        return Err(format!("{} is there already and holds files", target.display()));
    }
    let program = crate::toolchains::chosen_program("git").unwrap_or_else(|| PathBuf::from("git"));
    let mut command = Command::new(program);
    // git asks for no password at a terminal no one can type into; a credential helper still may.
    command.args(["clone", "--progress", "--", url]).arg(target).env("PATH", crate::toolchains::run_path()).env("GIT_TERMINAL_PROMPT", "0");
    command.stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| match error.kind() {
        std::io::ErrorKind::NotFound => "git is not installed, or not on the PATH: File, Toolchains, or orior file toolchains install git, opens its install page".to_string(),
        _ => format!("git did not start: {error}"),
    })?;
    let mut stderr = child.stderr.take().expect("piped");
    let mut line = Vec::new();
    let mut fatal = None;
    let mut chunk = [0u8; 4096];
    let mut tell = |line: &mut Vec<u8>, ended: bool, fatal: &mut Option<String>| {
        let text = String::from_utf8_lossy(line).trim().to_string();
        line.clear();
        if !text.is_empty() {
            said(&text, ended);
            if fatal.is_none() {
                *fatal = text.strip_prefix("fatal: ").map(String::from);
            }
        }
    };
    loop {
        let read = match stderr.read(&mut chunk) {
            Ok(0) | Err(_) => break,
            Ok(read) => read,
        };
        for &byte in &chunk[..read] {
            match byte {
                b'\n' => tell(&mut line, true, &mut fatal),
                b'\r' => tell(&mut line, false, &mut fatal),
                _ => line.push(byte),
            }
        }
    }
    tell(&mut line, true, &mut fatal);
    let status = child.wait().map_err(|error| error.to_string())?;
    if status.success() {
        Ok(target.to_path_buf())
    } else {
        Err(fatal.unwrap_or_else(|| format!("git clone {url} failed")))
    }
}

#[cfg(test)]
mod clones {
    use std::path::Path;

    use super::clone_folder;

    #[test]
    fn a_clone_is_named_as_git_names_it() {
        let parent = Path::new("work");
        assert_eq!(clone_folder("https://github.com/dstroy0/orior.git", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("https://github.com/dstroy0/orior/", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("git@github.com:dstroy0/orior.git", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("git@host:orior", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder(r"D:\repos\orior.git", parent).unwrap(), parent.join("orior"));
        assert!(clone_folder("https://", parent).is_err());
        assert!(clone_folder("  ", parent).is_err());
    }
}

#[cfg(test)]
mod parting {
    use super::{Part, commit_parts};
    use std::process::Command;

    #[test]
    fn a_file_taken_in_part_commits_that_part_alone() {
        let root = std::env::temp_dir().join(format!("orior_ui_parts_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| {
            let out = Command::new("git").args(args).current_dir(&root).output().unwrap();
            assert!(out.status.success(), "{args:?}");
            String::from_utf8_lossy(&out.stdout).into_owned()
        };
        git(&["init", "-q", "-b", "main"]);
        git(&["config", "user.name", "t"]);
        git(&["config", "user.email", "t@t"]);
        std::fs::write(root.join("a.txt"), "one\ntwo\nthree\nfour\n").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "first"]);
        std::fs::write(root.join("a.txt"), "one\nTWO\nthree\nFOUR\n").unwrap();
        std::fs::write(root.join("other.txt"), "staged apart\n").unwrap();
        git(&["add", "other.txt"]);
        let part = Part { path: "a.txt".into(), text: "one\nTWO\nthree\nfour\n".into() };
        commit_parts(&root, "part of a", &[], &[part]).unwrap();
        assert_eq!(git(&["show", "HEAD:a.txt"]), "one\nTWO\nthree\nfour\n");
        assert_eq!(std::fs::read_to_string(root.join("a.txt")).unwrap(), "one\nTWO\nthree\nFOUR\n");
        assert_eq!(git(&["show", "--name-only", "--format=", "HEAD"]).trim(), "a.txt");
        let mut status: Vec<String> = git(&["status", "--porcelain"]).lines().map(str::to_string).collect();
        status.sort();
        assert_eq!(status, [" M a.txt", "A  other.txt"]);
        std::fs::remove_dir_all(&root).unwrap();
    }
}

#[cfg(test)]
mod branching {
    use super::{branch_act, branches};
    use std::process::Command;

    #[test]
    fn a_branch_is_made_switched_to_renamed_merged_and_deleted() {
        let root = std::env::temp_dir().join(format!("orior_ui_branches_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| assert!(Command::new("git").args(args).current_dir(&root).output().unwrap().status.success(), "{args:?}");
        git(&["init", "-q", "-b", "main"]);
        git(&["config", "user.name", "t"]);
        git(&["config", "user.email", "t@t"]);
        std::fs::write(root.join("a.txt"), "one").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "first"]);
        branch_act(&root, "create", "topic", "").unwrap();
        std::fs::write(root.join("a.txt"), "two").unwrap();
        git(&["commit", "-q", "-am", "second"]);
        let listed = branches(&root);
        assert_eq!(listed.iter().find(|branch| branch.current).map(|branch| branch.name.as_str()), Some("topic"));
        assert_eq!(listed.iter().find(|branch| branch.name == "topic").map(|branch| branch.subject.as_str()), Some("second"));
        branch_act(&root, "rename", "topic", "feature").unwrap();
        branch_act(&root, "switch", "main", "").unwrap();
        assert!(branch_act(&root, "delete", "feature", "").is_err());
        branch_act(&root, "merge", "feature", "").unwrap();
        assert_eq!(std::fs::read_to_string(root.join("a.txt")).unwrap(), "two");
        branch_act(&root, "delete", "feature", "").unwrap();
        assert_eq!(branches(&root).iter().map(|branch| branch.name.as_str()).collect::<Vec<_>>(), ["main"]);
        assert!(branch_act(&root, "create", "-x", "").is_err());
        assert!(branch_act(&root, "create", "bad..name", "").is_err());
        std::fs::remove_dir_all(&root).unwrap();
    }
}

#[cfg(test)]
mod graphing {
    use super::{Drawn, graph, lay_out, line_history, touched};
    use std::process::Command;

    fn commit(id: &str, parents: &[&str]) -> Drawn {
        let parents = parents.iter().map(|one| one.to_string()).collect();
        Drawn { id: id.into(), parents, author: String::new(), when: 0, refs: Vec::new(), subject: String::new(), lane: 0, color: 0, lines: Vec::new() }
    }

    #[test]
    fn a_merged_branch_takes_a_column_of_its_own_and_joins_where_it_began() {
        let mut drawn = vec![commit("m", &["a3", "b2"]), commit("a3", &["a2"]), commit("b2", &["b1"]), commit("b1", &["a2"]), commit("a2", &["a1"]), commit("a1", &[])];
        lay_out(&mut drawn);
        assert_eq!(drawn.iter().map(|one| one.lane).collect::<Vec<_>>(), [0, 0, 1, 1, 0, 0]);
        assert_eq!(drawn[0].lines, [[0, 0, 1, 0], [0, 1, 1, 1]]);
        assert_eq!(drawn[1].lines, [[0, 0, 0, 0], [1, 1, 0, 1], [0, 0, 1, 0], [1, 1, 1, 1]]);
        assert!(drawn[3].lines.contains(&[1, 0, 1, 0]));
        assert_eq!(drawn[4].lines, [[0, 0, 0, 0], [0, 0, 1, 0]]);
        assert_eq!(drawn[5].lines, [[0, 0, 0, 0]]);
        assert_eq!(drawn[2].color, 1);
    }

    #[test]
    fn a_commit_lists_the_files_it_changed_and_a_search_finds_it() {
        let root = std::env::temp_dir().join(format!("orior_ui_graph_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| {
            let out = Command::new("git").args(args).current_dir(&root).output().unwrap();
            assert!(out.status.success(), "{args:?}");
            String::from_utf8_lossy(&out.stdout).trim().to_string()
        };
        git(&["init", "-q", "-b", "main"]);
        git(&["config", "user.name", "Ada"]);
        git(&["config", "user.email", "t@t"]);
        std::fs::write(root.join("a.txt"), "one\ntwo\nthree\nfour\n").unwrap();
        std::fs::write(root.join("b.txt"), "b\n").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "first"]);
        std::fs::rename(root.join("a.txt"), root.join("c.txt")).unwrap();
        std::fs::remove_file(root.join("b.txt")).unwrap();
        std::fs::write(root.join("d.txt"), "d\n").unwrap();
        git(&["add", "-A"]);
        git(&["commit", "-q", "-m", "Second, renamed"]);
        let first = git(&["rev-parse", "HEAD~1"]);
        let second = git(&["rev-parse", "HEAD"]);
        let mut seen: Vec<(String, char, Option<String>)> = touched(&root, &second).unwrap().into_iter().map(|one| (one.path, one.state, one.was)).collect();
        seen.sort();
        assert_eq!(seen, [("b.txt".into(), 'D', None), ("c.txt".into(), 'R', Some("a.txt".into())), ("d.txt".into(), 'A', None)]);
        assert_eq!(touched(&root, &first).unwrap().len(), 2);
        assert!(touched(&root, "not-hex").is_err());
        let all = graph(&root, "");
        assert_eq!(all.iter().map(|one| one.subject.as_str()).collect::<Vec<_>>(), ["Second, renamed", "first"]);
        assert!(all[0].refs.iter().any(|one| one.contains("main")));
        assert_eq!(all[0].author, "Ada");
        assert_eq!(graph(&root, "RENAMED ada").iter().map(|one| one.id.as_str()).collect::<Vec<_>>(), [second.as_str()]);
        assert!(graph(&root, "nowhere").is_empty());
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn each_line_has_the_commit_that_last_changed_it() {
        let base = std::env::temp_dir().join(format!("orior_ui_lines_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&base);
        let root = base.join("sub");
        std::fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| {
            let out = Command::new("git").args(args).current_dir(&base).output().unwrap();
            assert!(out.status.success(), "{args:?}");
            String::from_utf8_lossy(&out.stdout).trim().to_string()
        };
        git(&["init", "-q", "-b", "main"]);
        git(&["config", "user.name", "Ada"]);
        git(&["config", "user.email", "t@t"]);
        std::fs::write(root.join("a.txt"), "one\ntwo\n").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "first"]);
        std::fs::write(root.join("a.txt"), "one\nsecond\n").unwrap();
        git(&["commit", "-q", "-am", "second"]);
        let first = git(&["rev-parse", "HEAD~1"]);
        let history = line_history(&root, "a.txt", "one\nsecond\nnew\n".into()).unwrap();
        let ids: Vec<Option<&str>> = history.lines.iter().map(|&at| usize::try_from(at).ok().map(|at| history.commits[at].subject.as_str())).collect();
        assert_eq!(ids, [Some("first"), Some("second"), None]);
        let second = &history.commits[usize::try_from(history.lines[1]).unwrap()];
        assert_eq!((second.author.as_str(), second.path.as_deref(), second.parent.as_deref(), second.was.as_deref()), ("Ada", Some("a.txt"), Some(first.as_str()), Some("a.txt")));
        assert!(line_history(&root, "missing.txt", String::new()).is_err());
        std::fs::remove_dir_all(&base).unwrap();
    }
}

#[cfg(test)]
mod stashing {
    use super::{cherry_pick, elsewhere, stash_act, stashes};
    use std::process::Command;

    #[test]
    fn changes_are_put_aside_and_brought_back_and_a_commit_is_picked_from_another_branch() {
        let root = std::env::temp_dir().join(format!("orior_ui_stash_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let git = |args: &[&str]| {
            let out = Command::new("git").args(args).current_dir(&root).output().unwrap();
            assert!(out.status.success(), "{args:?}");
            String::from_utf8_lossy(&out.stdout).trim().to_string()
        };
        git(&["init", "-q", "-b", "main"]);
        git(&["config", "user.name", "t"]);
        git(&["config", "user.email", "t@t"]);
        std::fs::write(root.join("a.txt"), "one\n").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "first"]);
        std::fs::write(root.join("a.txt"), "changed\n").unwrap();
        std::fs::write(root.join("b.txt"), "new\n").unwrap();
        stash_act(&root, "push", "", "half done").unwrap();
        assert_eq!(git(&["status", "--porcelain"]), "");
        let listed = stashes(&root);
        assert_eq!(listed.len(), 1);
        assert_eq!(listed[0].name, "stash@{0}");
        assert!(listed[0].subject.ends_with("half done"), "{}", listed[0].subject);
        stash_act(&root, "apply", "stash@{0}", "").unwrap();
        assert_eq!(std::fs::read_to_string(root.join("a.txt")).unwrap(), "changed\n");
        assert!(root.join("b.txt").exists());
        assert_eq!(stashes(&root).len(), 1);
        git(&["checkout", "--", "a.txt"]);
        std::fs::remove_file(root.join("b.txt")).unwrap();
        stash_act(&root, "pop", "stash@{0}", "").unwrap();
        assert!(stashes(&root).is_empty());
        stash_act(&root, "push", "", "").unwrap();
        stash_act(&root, "drop", "stash@{0}", "").unwrap();
        assert!(stashes(&root).is_empty());
        assert!(stash_act(&root, "apply", "HEAD", "").is_err());
        assert!(stash_act(&root, "clear", "stash@{0}", "").is_err());

        git(&["switch", "-q", "-c", "topic"]);
        std::fs::write(root.join("c.txt"), "picked\n").unwrap();
        git(&["add", "."]);
        git(&["commit", "-q", "-m", "on topic"]);
        let picked = git(&["rev-parse", "HEAD"]);
        git(&["switch", "-q", "main"]);
        assert_eq!(elsewhere(&root).iter().map(|one| one.id.as_str()).collect::<Vec<_>>(), [picked.as_str()]);
        cherry_pick(&root, &picked).unwrap();
        assert_eq!(std::fs::read_to_string(root.join("c.txt")).unwrap(), "picked\n");
        assert_eq!(git(&["log", "-1", "--format=%s"]), "on topic");
        assert!(elsewhere(&root).is_empty());
        assert!(cherry_pick(&root, "--abort").is_err());
        std::fs::remove_dir_all(&root).unwrap();
    }
}

#[cfg(test)]
mod states {
    use super::state_of;

    #[test]
    fn each_status_has_one_letter() {
        assert_eq!(state_of(b'?', b'?'), 'U');
        assert_eq!(state_of(b' ', b'M'), 'M');
        assert_eq!(state_of(b'M', b' '), 'M');
        assert_eq!(state_of(b'A', b' '), 'A');
        assert_eq!(state_of(b' ', b'D'), 'D');
        assert_eq!(state_of(b'R', b' '), 'R');
        assert_eq!(state_of(b'U', b'U'), 'C');
        assert_eq!(state_of(b'A', b'A'), 'C');
    }
}

#[cfg(test)]
mod committing {
    use std::path::Path;

    use super::{ahead_behind, changed, commit, pull, push, rollback, run};

    fn write(dir: &Path, name: &str, text: &str) {
        std::fs::write(dir.join(name), text).unwrap();
    }

    #[test]
    #[ignore = "commits through git as it is set up here, signing and all"]
    fn chosen_files_commit_push_and_come_back_with_a_pull() {
        let base = std::env::temp_dir().join(format!("orior-commit-{}", std::process::id()));
        let (remote, here, there) = (base.join("remote.git"), base.join("here"), base.join("there"));
        std::fs::create_dir_all(&here).unwrap();
        run(&base, &["init", "--bare", "-b", "main", "remote.git"]).unwrap();
        run(&here, &["init", "-b", "main"]).unwrap();
        run(&here, &["remote", "add", "origin", &remote.display().to_string()]).unwrap();
        write(&here, "a.txt", "one\n");
        write(&here, "b.txt", "two\n");
        assert!(commit(&here, "Begin.", &["a.txt".into(), "b.txt".into()]).unwrap().contains("Begin."));
        push(&here).unwrap();
        assert_eq!(ahead_behind(&here), Some((0, 0)));

        write(&here, "a.txt", "one changed\n");
        write(&here, "b.txt", "two changed\n");
        write(&here, "c.txt", "three\n");
        commit(&here, "Change a and add c.", &["a.txt".into(), "c.txt".into()]).unwrap();
        let left: Vec<String> = changed(&here).into_iter().map(|one| one.path).collect();
        assert_eq!(left, vec!["b.txt".to_string()], "only the file not chosen is left changed");
        rollback(&here, "b.txt").unwrap();
        assert!(changed(&here).is_empty());
        write(&here, "d.txt", "new\n");
        assert!(rollback(&here, "d.txt").unwrap_err().contains("not in the last commit"));
        assert_eq!(ahead_behind(&here), Some((1, 0)));
        push(&here).unwrap();

        run(&base, &["clone", &remote.display().to_string(), "there"]).unwrap();
        write(&here, "a.txt", "one again\n");
        commit(&here, "Change a again.", &["a.txt".into()]).unwrap();
        push(&here).unwrap();
        pull(&there).unwrap();
        assert_eq!(std::fs::read_to_string(there.join("a.txt")).unwrap().trim(), "one again");
        let _ = std::fs::remove_dir_all(&base);
    }
}
