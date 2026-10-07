// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Formatting a text with the formatter toolchains.json gives its language: Black for Python,
//! clang-format for C, C++ and CUDA, rustfmt for Rust, and Prettier for JavaScript, CSS, HTML, JSON,
//! Markdown and YAML. The formatter is found as File, Toolchains finds it, runs in the file's folder
//! with the PATH orior's runs get, reads the text on its input and is told the file's path, and so
//! keeps to the project's own settings: a pyproject.toml, a .clang-format, a rustfmt.toml or a
//! .prettierrc. What it writes back is the text formatted; a formatter that fails gives what it said.

use std::io::{Read, Write};
use std::path::Path;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use crate::{plugins, toolchains};

/// How long a formatter may take before it is stopped.
const PATIENCE: Duration = Duration::from_secs(30);

/// The language a file opens in, by its extension, as the window chooses it: one of the reader's
/// plugins ahead of one that comes with orior, and of two that come with orior the first by id.
pub fn language_of(path: &Path) -> Option<String> {
    let ext = path.extension()?.to_string_lossy().to_lowercase();
    let mut chosen: Option<(String, &'static str)> = None;
    for plugin in plugins::all() {
        let Ok(read) = serde_json::from_str::<serde_json::Value>(&plugin.text) else {
            continue;
        };
        let names = read["extensions"].as_array().map(|list| list.iter().filter_map(|one| one.as_str()).any(|one| one.eq_ignore_ascii_case(&ext))).unwrap_or(false);
        if names && (chosen.is_none() || plugin.source == "user") {
            chosen = Some((plugin.id.clone(), plugin.source));
        }
    }
    chosen.map(|(id, _)| id)
}

/// Every language a formatter formats.
pub fn languages() -> Vec<String> {
    toolchains::manifest().into_iter().filter(|tool| tool.format.is_some()).flat_map(|tool| tool.formats).collect()
}

/// The tool that formats `language`, and how.
fn formatter_of(language: &str) -> Result<(toolchains::Tool, toolchains::Formatter), String> {
    let tools = toolchains::manifest();
    tools
        .iter()
        .find_map(|tool| tool.format.clone().filter(|_| tool.formats.iter().any(|one| one == language)).map(|format| (tool.clone(), format)))
        .ok_or_else(|| {
            let known: Vec<String> = tools.iter().filter(|tool| tool.format.is_some()).map(|tool| tool.name.clone()).collect();
            format!("orior has no formatter for {language}: it formats with {}", known.join(", "))
        })
}

/// `text`, the text of the file at `path` in `language`, formatted.
pub fn format(path: &Path, language: &str, text: &str) -> Result<String, String> {
    let (tool, format) = formatter_of(language)?;
    let found = toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen());
    let program = found
        .folder
        .as_deref()
        .and_then(|folder| toolchains::program_in(Path::new(folder), std::slice::from_ref(&format.program)))
        .ok_or_else(|| format!("{} is not found: File, Toolchains opens its install page or takes its folder", tool.name))?;
    let file = path.display().to_string();
    let args: Vec<String> = format.args.iter().map(|word| word.replace("{file}", &file)).collect();
    let mut command = Command::new(&program);
    command.args(&args).env("PATH", toolchains::run_path()).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped());
    if let Some(dir) = path.parent().filter(|dir| dir.is_dir()) {
        command.current_dir(dir);
    }
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| format!("{}: {error}", program.display()))?;
    let mut input = child.stdin.take().ok_or("the formatter took no input")?;
    let given = text.to_string();
    let writer = std::thread::spawn(move || {
        let _ = input.write_all(given.as_bytes());
    });
    let mut stdout = child.stdout.take().ok_or("the formatter gave no output")?;
    let mut stderr = child.stderr.take().ok_or("the formatter gave no output")?;
    let reader = std::thread::spawn(move || {
        let mut out = Vec::new();
        let _ = stdout.read_to_end(&mut out);
        out
    });
    let complaint = std::thread::spawn(move || {
        let mut out = Vec::new();
        let _ = stderr.read_to_end(&mut out);
        out
    });
    let began = Instant::now();
    let status = loop {
        match child.try_wait() {
            Ok(Some(status)) => break status,
            Ok(None) if began.elapsed() < PATIENCE => std::thread::sleep(Duration::from_millis(20)),
            _ => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(format!("{} took longer than {} s and was stopped", format.program, PATIENCE.as_secs()));
            }
        }
    };
    let _ = writer.join();
    let out = reader.join().unwrap_or_default();
    let said = String::from_utf8_lossy(&complaint.join().unwrap_or_default()).trim().to_string();
    if !status.success() {
        let said: Vec<&str> = said.lines().take(6).collect();
        return Err(format!("{} could not format {}: {}", format.program, path.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or(file), if said.is_empty() { "it said nothing".to_string() } else { said.join("\n") }));
    }
    String::from_utf8(out).map_err(|_| format!("{} wrote text that is not UTF-8", format.program))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn each_formatter_names_a_language_and_reads_the_file() {
        for tool in toolchains::manifest().into_iter().filter(|tool| tool.format.is_some()) {
            assert!(!tool.formats.is_empty(), "{} formats no language", tool.id);
        }
        assert_eq!(formatter_of("python").unwrap().1.program, "black");
        assert_eq!(formatter_of("c").unwrap().1.program, "clang-format");
        assert!(formatter_of("tex").is_err());
    }

    #[test]
    fn a_file_s_language_is_the_window_s() {
        assert_eq!(language_of(Path::new("a/b.py")).as_deref(), Some("python"));
        assert_eq!(language_of(Path::new("k.CU")).as_deref(), Some("c"));
        assert_eq!(language_of(Path::new("smooth.m")).as_deref(), Some("matlab"));
        assert_eq!(language_of(Path::new("none")), None);
    }
}
