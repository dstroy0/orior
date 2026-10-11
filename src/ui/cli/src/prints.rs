// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The print statements a change added: each line the tree's change against its last commit puts in,
//! a file new to git whole, that prints in its file's language as a print for a task does, such as
//! Python's print(, JavaScript's console.log(, Rust's dbg! and println!, C's printf, Go's fmt.Println,
//! Java's System.out.println and Ruby's puts. Each is named by its file, its line in the file as it
//! stands, and its text, for the reader to see before a commit and take out in one step.

use std::path::Path;
use std::process::{Command, Stdio};

use serde::Serialize;

use crate::git;

/// A print a change added: its file, its line from 0, and its text.
#[derive(Serialize, Debug, Clone, PartialEq)]
pub struct Print {
    pub path: String,
    pub line: u32,
    pub text: String,
}

/// The starts of a line that prints, by the extension of its file.
fn starts(extension: &str) -> &'static [&'static str] {
    match extension {
        "py" | "pyi" => &["print(", "pprint(", "pprint.pprint("],
        "js" | "mjs" | "cjs" | "jsx" | "ts" | "tsx" | "mts" | "cts" | "vue" | "svelte" => &["console.log(", "console.debug(", "console.info(", "console.warn(", "console.error(", "console.trace(", "console.dir(", "console.table("],
        "rs" => &["dbg!(", "println!(", "eprintln!(", "print!(", "eprint!("],
        "c" | "h" | "cc" | "cpp" | "cxx" | "hpp" | "hh" | "hxx" | "cu" | "cuh" => &["printf(", "fprintf(stderr", "fprintf(stdout", "puts(", "std::cout <<", "std::cerr <<", "cout <<", "cerr <<"],
        "go" => &["fmt.Println(", "fmt.Printf(", "fmt.Print(", "log.Println(", "log.Printf(", "println("],
        "java" | "kt" | "kts" | "scala" | "groovy" => &["System.out.println(", "System.out.print(", "System.err.println(", "println(", "print("],
        "rb" => &["puts ", "puts(", "p ", "pp ", "print "],
        "php" => &["var_dump(", "print_r(", "echo ", "error_log("],
        "lua" => &["print("],
        "r" => &["print(", "cat("],
        "m" | "jl" => &["disp(", "fprintf(", "println(", "@show "],
        "swift" | "dart" => &["print(", "debugPrint("],
        "cs" => &["Console.WriteLine(", "Console.Write(", "Debug.WriteLine("],
        _ => &[],
    }
}

/// Whether `text`, a line of the file at `path`, prints as a print for a task does.
pub fn prints(path: &str, text: &str) -> bool {
    let extension = path.rsplit_once('.').map(|(_, ext)| ext.to_ascii_lowercase()).unwrap_or_default();
    let trimmed = text.trim_start();
    starts(&extension).iter().any(|start| {
        trimmed.strip_prefix(start).is_some_and(|rest| {
            let joined = start.ends_with(['(', '<', ' ']) || start.ends_with("stderr") || start.ends_with("stdout");
            joined || rest.starts_with([' ', '('])
        })
    })
}

/// The lines a unified diff with no lines of context puts in, each by its path, its line in the file
/// as it stands, from 0, and its text.
pub fn put_in(diff: &str) -> Vec<(String, u32, String)> {
    let mut out = Vec::new();
    let mut path = String::new();
    let mut line = 0u32;
    for row in diff.lines() {
        if let Some(name) = row.strip_prefix("+++ ") {
            path = name.strip_prefix("b/").unwrap_or(name).to_string();
        } else if let Some(head) = row.strip_prefix("@@ ") {
            let plus = head.split_whitespace().find(|part| part.starts_with('+')).unwrap_or("+0");
            let start = plus[1..].split(',').next().and_then(|at| at.parse::<u32>().ok()).unwrap_or(1);
            line = start.saturating_sub(1);
        } else if let Some(text) = row.strip_prefix('+') {
            if path != "/dev/null" {
                out.push((path.clone(), line, text.to_string()));
            }
            line += 1;
        }
    }
    out
}

/// The print statements the change of the tree `root` against its last commit added.
pub fn added(root: &Path) -> Vec<Print> {
    let mut found = Vec::new();
    for repo in git::repositories(root) {
        let dir = root.join(&repo.path);
        let at = if repo.path.is_empty() { String::new() } else { format!("{}/", repo.path) };
        let mut command = Command::new("git");
        command.args(["diff", "HEAD", "--unified=0", "--no-color", "--no-ext-diff", "--relative"]).current_dir(&dir).stdin(Stdio::null()).stderr(Stdio::null());
        crate::runner::quiet(&mut command);
        if let Ok(out) = command.output() {
            for (path, line, text) in put_in(&String::from_utf8_lossy(&out.stdout)) {
                if prints(&path, &text) {
                    found.push(Print { path: format!("{at}{path}"), line, text });
                }
            }
        }
    }
    for changed in git::changed(root, None).into_iter().filter(|one| one.state == 'U' && !one.path.ends_with('/')) {
        let Ok(text) = std::fs::read_to_string(root.join(&changed.path)) else {
            continue;
        };
        for (line, row) in text.lines().enumerate() {
            if prints(&changed.path, row) {
                found.push(Print { path: changed.path.clone(), line: line as u32, text: row.to_string() });
            }
        }
    }
    found.sort_by(|a, b| a.path.cmp(&b.path).then(a.line.cmp(&b.line)));
    found.dedup();
    found
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_line_prints_by_its_file_s_language() {
        assert!(prints("a.py", "    print(total)"));
        assert!(!prints("a.py", "    printer(total)"), "a longer name is no print");
        assert!(!prints("a.py", "    # print(total)"), "a comment is no print");
        assert!(prints("a.js", "  console.log('x', y);"));
        assert!(prints("a.rs", "    dbg!(&value);"));
        assert!(prints("a.c", "  printf(\"%d\\n\", n);"));
        assert!(prints("a.c", "  fprintf(stderr, \"x\");"));
        assert!(prints("a.go", "\tfmt.Println(x)"));
        assert!(prints("a.rb", "puts value"));
        assert!(!prints("a.rb", "putsch value"));
        assert!(!prints("a.md", "print(x)"), "a file of no language read here has none");
    }

    #[test]
    fn the_lines_a_diff_puts_in_are_read_with_their_lines_in_the_file_as_it_stands() {
        let diff = "diff --git a/a.py b/a.py\n--- a/a.py\n+++ b/a.py\n@@ -3,0 +4,2 @@ def f():\n+    print(x)\n+    y = 1\n@@ -10 +12 @@\n-    old\n+    print(\"new\")\n";
        assert_eq!(put_in(diff), vec![("a.py".into(), 3, "    print(x)".into()), ("a.py".into(), 4, "    y = 1".into()), ("a.py".into(), 11, "    print(\"new\")".into())]);
    }

    #[test]
    fn the_prints_a_change_added_are_found_in_its_files_and_in_files_new_to_git() {
        let root = std::env::temp_dir().join(format!("orior_ui_prints_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let root = dunce::canonicalize(&root).unwrap();
        let run = |args: &[&str]| assert!(Command::new("git").args(args).current_dir(&root).output().unwrap().status.success(), "{args:?}");
        run(&["init", "-q"]);
        run(&["config", "user.name", "Tester"]);
        run(&["config", "user.email", "tester@example.com"]);
        std::fs::write(root.join("a.py"), "def f(x):\n    print(\"kept\")\n    return x\n").unwrap();
        run(&["add", "."]);
        run(&["commit", "-q", "-m", "first"]);
        std::fs::write(root.join("a.py"), "def f(x):\n    print(\"kept\")\n    print(x)\n    return x\n").unwrap();
        std::fs::write(root.join("b.js"), "const a = 1;\nconsole.log(a);\n").unwrap();
        let found = added(&root);
        assert_eq!(found, vec![Print { path: "a.py".into(), line: 2, text: "    print(x)".into() }, Print { path: "b.js".into(), line: 1, text: "console.log(a);".into() }], "the print already committed is not one the change added");
        let _ = std::fs::remove_dir_all(&root);
    }
}
