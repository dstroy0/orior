// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Running one file with the toolchain its language has: Python with python, R with Rscript, MATLAB
//! with matlab -batch, Octave with octave-cli, Lean with lean --run, TeX with latexmk, a netlist with
//! ngspice or LTspice in batch, VHDL with ghdl, and C, C++, CUDA and Rust compiled to build/run/ and
//! then run. Each toolchain's `runs` in toolchains.json gives the line, by language, or by language
//! and extension as c.cu is, and of the toolchains whose `runs` name it the first found runs it.
//!
//! The line is for a POSIX shell, the one orior's terminal runs, and runs at the tree's top folder.
//! In it {file} is the file's path quoted, {path} the same unquoted, {dir} its folder quoted, {stem}
//! its name without its extension, {out} build/run/<stem> quoted, {program} the toolchain's program
//! found, and any other {name} the program of that name in the same folder.

use std::path::{Path, PathBuf};

use crate::toolchains;

#[derive(serde::Serialize, Debug)]
pub struct RunLine {
    pub line: String,
    pub tool: String,
}

/// A path as a POSIX shell takes it: with forward slashes, in single quotes.
fn quoted(path: &Path) -> String {
    let text = path.display().to_string().replace('\\', "/");
    format!("'{}'", text.replace('\'', "'\\''"))
}

/// The line that runs the file at `path`, under the tree at `root`, in `language`.
pub fn line_for(root: &Path, path: &Path, language: &str) -> Result<RunLine, String> {
    let ext = path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).unwrap_or_default();
    let keys = [format!("{language}.{ext}"), language.to_string()];
    let tools = toolchains::manifest();
    let named: Vec<(&toolchains::Tool, &String)> = keys.iter().flat_map(|key| tools.iter().filter_map(move |tool| tool.runs.get(key).map(|line| (tool, line)))).collect();
    if named.is_empty() {
        return Err(format!("orior has no way to run a {language} file"));
    }
    let path_folders = toolchains::path_folders();
    let kept = toolchains::chosen();
    let found = named.iter().find_map(|(tool, line)| {
        let found = toolchains::find(tool, &path_folders, &kept);
        found.program.clone().map(|program| (*tool, *line, PathBuf::from(program)))
    });
    let Some((tool, template, program)) = found else {
        let names: Vec<&str> = named.iter().map(|(tool, _)| tool.name.as_str()).collect();
        return Err(format!("{} runs it, and is not found: File, Toolchains opens its install page or takes its folder", names.join(" or ")));
    };
    let folder = program.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
    let mut out_path = root.join("build").join("run").join(&stem);
    if cfg!(windows) {
        out_path.set_extension("exe");
    }
    let mut line = String::new();
    let mut rest = template.as_str();
    while let Some(open) = rest.find('{') {
        line.push_str(&rest[..open]);
        let close = rest[open..].find('}').map(|at| open + at).ok_or_else(|| format!("{}'s line for {language} has an open {{", tool.name))?;
        let name = &rest[open + 1..close];
        let filled = match name {
            "file" => quoted(path),
            "path" => path.display().to_string().replace('\\', "/"),
            "dir" => quoted(path.parent().unwrap_or(root)),
            "stem" => stem.clone(),
            "out" => {
                std::fs::create_dir_all(root.join("build").join("run")).map_err(|error| format!("build/run: {error}"))?;
                quoted(&out_path)
            }
            "program" => quoted(&program),
            other => quoted(&toolchains::program_in(&folder, &[other.to_string()]).ok_or_else(|| format!("{} has no {other} in {}", tool.name, folder.display()))?),
        };
        line.push_str(&filled);
        rest = &rest[close + 1..];
    }
    line.push_str(rest);
    Ok(RunLine { line: format!("( cd {} && {line} )", quoted(root)), tool: tool.name.clone() })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_path_is_quoted_for_the_shell() {
        assert_eq!(quoted(Path::new("a b/it's.py")), "'a b/it'\\''s.py'");
    }

    #[test]
    fn every_run_line_closes_its_names_and_a_language_without_one_is_refused() {
        for tool in toolchains::manifest() {
            for (key, line) in &tool.runs {
                assert_eq!(line.matches('{').count(), line.matches('}').count(), "{} {key}", tool.id);
            }
        }
        assert!(line_for(Path::new("."), Path::new("a.json"), "json").unwrap_err().contains("no way to run"));
    }
}
