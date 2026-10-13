// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The query's face on the command line: the programs the query crate needs found through the
//! toolchains, every command of the crate a job of the Protocol menu, and the words of `orior
//! protocol` dispatched into the crate (orior_query::commands).
//!
//! Nothing here holds a list of languages or parts. Each is the files of the bridge's folder that
//! name it: a language its map to the bridge, .klm, and its ruleset, .krs; a part its machine file,
//! .khw, its record, .kqr, and its costs, .ksc. The crate reads them the same way.

use std::ffi::OsString;
use std::path::{Path, PathBuf};

use orior_query::commands::{self, Word};
use orior_query::programs::{OnPath, Programs, Role};

use crate::catalog::{Arg, Job, Param, Program, Step};
use crate::{executables, toolchains};

/// The programs as the toolchains find them: the gcc or the llvm toolchain's compilers, whichever
/// is chosen, the cuda toolchain's nvcc and the programs beside it, the msvc toolchain's cl, the
/// python toolchain's program; the PATH alone where no toolchain is chosen. A program not found is
/// said as the toolchains say it: the tool's name, and that File, Toolchains opens its install
/// page or takes its folder.
pub struct Toolchains;

/// `program` in the folder of the toolchain `tool`'s chosen program, where one is chosen.
fn chosen(tool: &str, program: &str) -> Option<PathBuf> {
    toolchains::chosen_program(tool).and_then(|found| found.parent().map(Path::to_path_buf)).and_then(|dir| toolchains::program_in(&dir, &[program.to_string()]))
}

impl Programs for Toolchains {
    fn program(&self, role: Role) -> Result<PathBuf, String> {
        let found = match role {
            Role::C => chosen("gcc", "gcc").or_else(|| chosen("llvm", "clang")),
            Role::Cxx => chosen("gcc", "g++").or_else(|| chosen("llvm", "clang++")),
            Role::Nvcc => chosen("cuda", "nvcc"),
            Role::Nvdisasm => chosen("cuda", "nvdisasm"),
            Role::Cuobjdump => chosen("cuda", "cuobjdump"),
            Role::Cl => chosen("msvc", "cl"),
            Role::Python => chosen("python", "python3").or_else(|| chosen("python", "python")),
            Role::NvidiaSmi => None,
        };
        if let Some(found) = found {
            return Ok(found);
        }
        OnPath.program(role).map_err(|_| {
            let tool = match role {
                Role::C | Role::Cxx => "gcc",
                Role::Nvcc | Role::Nvdisasm | Role::Cuobjdump | Role::NvidiaSmi => "cuda",
                Role::Cl => "msvc",
                Role::Python => "python",
            };
            match executables::program(tool, role.name()) {
                Err(said) => said,
                Ok(_) => format!("{} is not found: File, Toolchains opens its install page or takes its folder", role.name()),
            }
        })
    }

    fn run_path(&self) -> OsString {
        toolchains::run_path()
    }
}

/// Every command of the crate as a job of the protocol group, its one step orior itself with the
/// command's words, which the window runs and streams as it runs any job.
pub fn jobs(root: &Path, jobs: &mut Vec<Job>) {
    for command in commands::list(root) {
        let params = command.params.iter().map(|param| Param { key: param.key.into(), kind: param.kind, choices: param.choices.clone(), default: String::new(), required: param.required }).collect();
        let mut args = vec![Arg::Lit("protocol".into())];
        args.extend(command.words.iter().map(|word| match word {
            Word::Lit(text) => Arg::Lit(text.to_string()),
            Word::Value(key) => Arg::Value(key.to_string()),
            Word::Flag(flag, key) => Arg::Flag(flag, key.to_string()),
            Word::Each(flag, key) => Arg::Each(flag, key.to_string()),
            Word::Words(key) => Arg::Words(key.to_string(), false),
            Word::When(key, value, text) => Arg::When(key.to_string(), value.to_string(), vec![text.to_string()]),
        }));
        jobs.push(Job {
            id: format!("protocol/{}", command.id),
            group: "protocol",
            title: command.title.into(),
            file: commands::TABLE.into(),
            about: command.about.into(),
            params,
            opens: "",
            steps: vec![Step { program: Program::Orior, args }],
        });
    }
}

/// `orior protocol <command> <words>` run here: the lines to the terminal, the code the exit.
pub fn protocol(root: &Path, words: &[String]) -> i32 {
    toolchains::set_tree(root);
    match commands::run(root, &Toolchains, words, &mut |line| println!("{line}")) {
        Ok(code) => code,
        Err(said) => {
            eprintln!("{said}");
            crate::cli::WRONG
        }
    }
}

#[cfg(test)]
mod face {
    use super::*;
    use crate::runner;
    use std::collections::HashMap;
    use std::fs;

    /// A tree under the top's build/ holding the files named in the bridge's folder, each empty.
    fn tree(name: &str, files: &[&str]) -> PathBuf {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../build/cli_test").join(format!("query_{name}_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        for file in files {
            let path = root.join(commands::TABLE).join(file);
            fs::create_dir_all(path.parent().unwrap()).unwrap();
            fs::write(path, "").unwrap();
        }
        root
    }

    fn given(words: &[&str]) -> HashMap<String, Vec<String>> {
        words.iter().filter_map(|word| word.split_once('=')).map(|(key, value)| (key.to_string(), vec![value.to_string()])).collect()
    }

    const WHOLE: [&str; 9] = ["Lstar.klq", "cu.klm", "cu.krs", "sass.klm", "sass.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"];

    #[test]
    fn every_command_of_the_crate_is_a_job_of_the_protocol_group() {
        let root = tree("jobs", &WHOLE);
        let mut found = Vec::new();
        jobs(&root, &mut found);
        let ids: Vec<&str> = found.iter().map(|job| job.id.as_str()).collect();
        assert_eq!(ids, ["protocol/query", "protocol/learn", "protocol/cross", "protocol/write", "protocol/pairs", "protocol/stick", "protocol/kept", "protocol/sweep"]);
        assert!(found.iter().all(|job| job.group == "protocol" && matches!(job.steps[0].program, Program::Orior)));
        let query = &found[0];
        assert_eq!(query.params[0].choices, ["cu", "sass"], "c has a ruleset and no map, so it is no language yet");
        assert_eq!(query.params[2].choices, ["sm_86"]);
        assert!(!query.params[2].required);
        let shown = runner::shown(&root, query, &given(&["from=cu", "to=sass", "part=sm_86", "dry=dry"])).unwrap();
        assert_eq!(shown.len(), 1);
        assert!(shown[0].ends_with("protocol query --from cu --to sass --part sm_86 --dry"), "{}", shown[0]);
        let bridge_only = runner::shown(&root, query, &given(&["from=cu", "to=sass"])).unwrap();
        assert!(bridge_only[0].ends_with("protocol query --from cu --to sass"), "{}", bridge_only[0]);
        let _ = fs::remove_dir_all(&root);
    }

    #[test]
    fn a_learn_job_s_words_are_the_command_s() {
        let root = tree("learn", &WHOLE);
        let mut found = Vec::new();
        jobs(&root, &mut found);
        let learn = found.iter().find(|job| job.id == "protocol/learn").unwrap();
        let shown = runner::shown(&root, learn, &given(&["step=turns", "base=a.cu", "leave=none"])).unwrap();
        assert!(shown[0].ends_with("protocol learn turns --base a.cu --leave none"), "{}", shown[0]);
        let _ = fs::remove_dir_all(&root);
    }

    #[test]
    fn the_toolchains_give_the_c_compiler_where_one_is_found() {
        match Toolchains.program(Role::C) {
            Ok(path) => assert!(path.is_file(), "{}", path.display()),
            Err(said) => assert!(said.contains("Toolchains"), "{said}"),
        }
        assert!(!Toolchains.run_path().is_empty());
    }
}
