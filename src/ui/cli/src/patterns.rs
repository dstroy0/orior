// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The patterns that keep the explorer, the search and the watching of files each from files of the
//! tree, a list for each, one pattern to a line.
//!
//! A pattern names files and folders as a `.gitignore` line does: `*` stands for any letters of a
//! name, `**` for any folders, `?` for one letter, and a trailing `/` names folders only. A pattern
//! with no `/` but a trailing one names a file or folder by its name wherever it is; one with a `/`
//! names a path from the tree's top. A folder named takes everything in it. A line starting with `#`
//! and an empty line name nothing.
//!
//! A pattern starting with `!` keeps the files it names and hides every other: `!src/**` alone keeps
//! the files under src and no others. Several such patterns keep the files any of them names, and a
//! pattern with no `!` hides files even among those. On Windows a pattern matches whatever the case.

use std::sync::{Arc, Mutex};

/// One pattern: as a path from the tree's top, `**/` before it where it names a name anywhere, and
/// whether it names folders only.
#[derive(Debug, Clone, PartialEq)]
struct Pattern {
    glob: Vec<char>,
    folders: bool,
}

impl Pattern {
    fn read(line: &str) -> Option<Pattern> {
        let line = line.trim();
        let line = line.strip_prefix("./").unwrap_or(line);
        let folders = line.ends_with('/');
        let line = line.trim_end_matches('/');
        if line.is_empty() {
            return None;
        }
        let glob = if line.contains('/') { line.trim_start_matches('/').to_string() } else { format!("**/{line}") };
        Some(Pattern { glob: glob.chars().map(fold).collect(), folders })
    }

    /// Whether the pattern names the path, or a folder it is in.
    fn hits(&self, path: &str, dir: bool) -> bool {
        let path: Vec<char> = path.chars().map(fold).collect();
        let folders = path.iter().enumerate().filter(|(_, letter)| **letter == '/').map(|(at, _)| at);
        for end in folders {
            if glob(&self.glob, &path[..end]) {
                return true;
            }
        }
        (dir || !self.folders) && glob(&self.glob, &path)
    }

    /// The pattern as git's pathspecs: the path itself and everything in it.
    fn pathspecs(&self, magic: &str) -> Vec<String> {
        let glob: String = self.glob.iter().collect();
        let case = if cfg!(windows) { ",icase" } else { "" };
        let inside = format!(":({magic}glob{case}){glob}/**");
        if self.folders { vec![inside] } else { vec![format!(":({magic}glob{case}){glob}"), inside] }
    }
}

/// A letter as a pattern compares it: whatever its case on Windows, as it is elsewhere.
fn fold(letter: char) -> char {
    if cfg!(windows) { letter.to_lowercase().next().unwrap_or(letter) } else { letter }
}

/// Whether the glob names the path from its first letter to its last.
fn glob(pattern: &[char], path: &[char]) -> bool {
    match pattern {
        [] => path.is_empty(),
        ['*', '*', '/', rest @ ..] => glob(rest, path) || path.iter().enumerate().any(|(at, letter)| *letter == '/' && glob(rest, &path[at + 1..])),
        ['*', '*', rest @ ..] => (0..=path.len()).any(|at| glob(rest, &path[at..])),
        ['*', rest @ ..] => {
            let name = path.iter().position(|letter| *letter == '/').unwrap_or(path.len());
            (0..=name).any(|at| glob(rest, &path[at..]))
        }
        ['?', rest @ ..] => path.first().is_some_and(|letter| *letter != '/') && glob(rest, &path[1..]),
        [letter, rest @ ..] => path.first() == Some(letter) && glob(rest, &path[1..]),
    }
}

/// The patterns of one part: those that hide what they name, and those that keep only what they name.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct Patterns {
    hide: Vec<Pattern>,
    keep: Vec<Pattern>,
}

impl Patterns {
    pub fn read(lines: &[String]) -> Patterns {
        let mut patterns = Patterns::default();
        for line in lines.iter().map(|line| line.trim()).filter(|line| !line.starts_with('#')) {
            match line.strip_prefix('!') {
                Some(kept) => patterns.keep.extend(Pattern::read(kept)),
                None => patterns.hide.extend(Pattern::read(line)),
            }
        }
        patterns
    }

    pub fn is_empty(&self) -> bool {
        self.hide.is_empty() && self.keep.is_empty()
    }

    /// Whether any pattern keeps only what it names.
    pub fn keeps_only(&self) -> bool {
        !self.keep.is_empty()
    }

    /// Whether the patterns hide a file, or a folder where `dir`. A folder is hidden here only where a
    /// pattern names it; whether it holds a file kept is the caller's to say.
    pub fn hides(&self, path: &str, dir: bool) -> bool {
        if self.hide.iter().any(|pattern| pattern.hits(path, dir)) {
            return true;
        }
        !dir && !self.keep.is_empty() && !self.keep.iter().any(|pattern| pattern.hits(path, false))
    }

    /// Whether a pattern that keeps names the folder, or a folder it is in.
    pub fn keeps_folder(&self, path: &str) -> bool {
        self.keep.iter().any(|pattern| pattern.hits(path, true))
    }

    /// The patterns as git's pathspecs: what the patterns that keep name, or the whole tree where none
    /// do, less what the others name.
    pub fn pathspecs(&self) -> Vec<String> {
        let mut specs: Vec<String> = self.keep.iter().flat_map(|pattern| pattern.pathspecs("")).collect();
        if specs.is_empty() {
            specs.push(".".into());
        }
        specs.extend(self.hide.iter().flat_map(|pattern| pattern.pathspecs("exclude,")));
        specs
    }
}

/// The parts of the window that patterns keep from files.
#[derive(Clone, Copy, Debug, serde::Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Part {
    Explorer,
    Search,
    Watching,
}

static SET: Mutex<[Option<Arc<Patterns>>; 3]> = Mutex::new([None, None, None]);

/// Sets the patterns of a part, and says whether they changed.
pub fn set(part: Part, lines: &[String]) -> bool {
    let read = Arc::new(Patterns::read(lines));
    let Ok(mut set) = SET.lock() else { return false };
    let changed = set[part as usize].as_deref() != Some(&read);
    set[part as usize] = Some(read);
    changed
}

/// The patterns of a part, none where they were never set.
pub fn of(part: Part) -> Arc<Patterns> {
    SET.lock().ok().and_then(|set| set[part as usize].clone()).unwrap_or_default()
}

#[cfg(test)]
mod matching {
    use super::Patterns;

    fn read(lines: &[&str]) -> Patterns {
        Patterns::read(&lines.iter().map(|line| line.to_string()).collect::<Vec<_>>())
    }

    #[test]
    fn a_pattern_hides_what_it_names_and_what_is_in_it() {
        let patterns = read(&["target", "*.log", "docs/gen/", "# a note", "", "src/**/*.min.js", "?.tmp"]);
        assert!(patterns.hides("target", true));
        assert!(patterns.hides("src/ui/target/debug/orior.exe", false));
        assert!(patterns.hides("run.log", false));
        assert!(patterns.hides("deep/in/it/run.log", false));
        assert!(!patterns.hides("run.log.txt", false));
        assert!(patterns.hides("docs/gen", true));
        assert!(patterns.hides("docs/gen/page.html", false));
        assert!(!patterns.hides("docs/gen", false));
        assert!(!patterns.hides("other/docs/gen/page.html", false));
        assert!(patterns.hides("src/a.min.js", false));
        assert!(patterns.hides("src/ui/vendor/a.min.js", false));
        assert!(!patterns.hides("lib/a.min.js", false));
        assert!(patterns.hides("x.tmp", false));
        assert!(!patterns.hides("xy.tmp", false));
        assert!(!patterns.hides("note", false));
    }

    #[test]
    fn a_pattern_that_keeps_hides_every_other_file() {
        let patterns = read(&["!src/**", "!*.md", "src/**/*.o"]);
        assert!(patterns.keeps_only());
        assert!(!patterns.hides("src/ui/main.rs", false));
        assert!(!patterns.hides("README.md", false));
        assert!(!patterns.hides("docs/guide.md", false));
        assert!(patterns.hides("Cargo.toml", false));
        assert!(patterns.hides("src/ui/main.o", false));
        assert!(!patterns.hides("docs", true));
        assert!(patterns.keeps_folder("src/ui"));
        assert!(!patterns.keeps_folder("docs"));
        assert!(!read(&["!src"]).hides("src/a.rs", false));
    }

    #[test]
    fn git_picks_the_files_the_patterns_leave() {
        let root = std::env::temp_dir().join(format!("orior_ui_patterns_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        let files = ["src/a.rs", "src/gen/b.rs", "docs/gen/x.md", "docs/guide.md", "target/debug/o.exe", "run.log", "deep/run.log", "README.md", "lib/vendor/a.min.js"];
        for file in files {
            let path = root.join(file);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, "x").unwrap();
        }
        let git = |args: &[String]| {
            let out = std::process::Command::new("git").args(args).current_dir(&root).output().unwrap();
            let mut listed: Vec<String> = String::from_utf8_lossy(&out.stdout).lines().map(str::to_string).collect();
            listed.sort();
            listed
        };
        git(&["init".into(), "-q".into()]);
        for lines in [&["target", "*.log", "docs/gen/"][..], &["!src/**", "src/gen"], &["!*.md"], &["**/*.min.js", "!lib/**"]] {
            let patterns = read(lines);
            let mut args: Vec<String> = ["ls-files", "--cached", "--others", "--"].map(str::to_string).to_vec();
            args.extend(patterns.pathspecs());
            let mut left: Vec<String> = files.iter().filter(|file| !patterns.hides(file, false)).map(|file| file.to_string()).collect();
            left.sort();
            assert_eq!(git(&args), left, "{lines:?}");
        }
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_pathspecs_name_what_the_patterns_name() {
        let case = if cfg!(windows) { ",icase" } else { "" };
        assert_eq!(read(&[]).pathspecs(), ["."]);
        assert_eq!(
            read(&["target", "!src/**", "gen/"]).pathspecs(),
            [
                format!(":(glob{case})src/**"),
                format!(":(glob{case})src/**/**"),
                format!(":(exclude,glob{case})**/target"),
                format!(":(exclude,glob{case})**/target/**"),
                format!(":(exclude,glob{case})**/gen/**"),
            ]
        );
    }
}
