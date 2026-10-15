// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The programs the query needs, named by role and never by path. The command line implements
//! `Programs` over its toolchains; `OnPath` is the crate's own implementation, which reads the PATH
//! alone, what the command line falls back to where no toolchain is chosen, and what the tests use.

use std::ffi::OsString;
use std::path::{Path, PathBuf};

/// A program by what it is for.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Role {
    /// The C compiler the tree's C programs are built with: cc or gcc, or clang.
    C,
    /// The C++ compiler the code generator's .cu files are compiled with as C++.
    Cxx,
    /// nvcc, the vendor's compiler.
    Nvcc,
    /// nvdisasm, beside nvcc.
    Nvdisasm,
    /// cuobjdump, beside nvcc.
    Cuobjdump,
    /// cl, the host compiler nvcc calls on Windows.
    Cl,
    /// The tree's Python.
    Python,
    /// nvidia-smi, which says whether the part is free.
    NvidiaSmi,
    /// docker, which runs the harness's images.
    Docker,
    /// curl, which fetches the vendor's archives.
    Curl,
    /// tar, which unpacks them.
    Tar,
}

impl Role {
    pub const EVERY: [Role; 11] = [Role::C, Role::Cxx, Role::Nvcc, Role::Nvdisasm, Role::Cuobjdump, Role::Cl, Role::Python, Role::NvidiaSmi, Role::Docker, Role::Curl, Role::Tar];

    /// The role as a person reads it in a message.
    pub fn name(self) -> &'static str {
        match self {
            Role::C => "the C compiler",
            Role::Cxx => "the C++ compiler",
            Role::Nvcc => "nvcc",
            Role::Nvdisasm => "nvdisasm",
            Role::Cuobjdump => "cuobjdump",
            Role::Cl => "cl",
            Role::Python => "python",
            Role::NvidiaSmi => "nvidia-smi",
            Role::Docker => "docker",
            Role::Curl => "curl",
            Role::Tar => "tar",
        }
    }
}

/// Where the query's programs are found.
pub trait Programs: Send + Sync {
    /// The program of `role`, or why it is not found, in words that say what to do.
    fn program(&self, role: Role) -> Result<PathBuf, String>;
    /// The PATH every program the query starts runs with.
    fn run_path(&self) -> OsString;
}

/// The programs as the PATH alone gives them.
pub struct OnPath;

/// The first of `names` found as a program in a folder of the PATH, with `.exe` on Windows.
pub fn on_path(names: &[&str]) -> Option<PathBuf> {
    let path = std::env::var_os("PATH")?;
    for dir in std::env::split_paths(&path) {
        for name in names {
            if let Some(found) = in_folder(&dir, name) {
                return Some(found);
            }
        }
    }
    None
}

/// `name` as a program in `dir`, with `.exe` on Windows, where it is there.
pub fn in_folder(dir: &Path, name: &str) -> Option<PathBuf> {
    let plain = dir.join(name);
    if plain.is_file() {
        return Some(plain);
    }
    if cfg!(windows) {
        let exe = dir.join(format!("{name}.exe"));
        if exe.is_file() {
            return Some(exe);
        }
    }
    None
}

/// MSVC's host compiler as nvcc wants it on Windows: the newest `cl` under Visual Studio's MSVC
/// tools, the Build Tools' first.
fn cl_installed() -> Option<PathBuf> {
    let mut found = Vec::new();
    let bases = ["C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC", "C:\\Program Files\\Microsoft Visual Studio"];
    for base in bases {
        for versioned in std::fs::read_dir(base).into_iter().flatten().flatten().map(|entry| entry.path()) {
            let direct = versioned.join("bin").join("Hostx64").join("x64").join("cl.exe");
            if direct.is_file() {
                found.push(direct);
            }
            // Program Files\Microsoft Visual Studio\<year>\<edition>\VC\Tools\MSVC\<version>
            for edition in std::fs::read_dir(&versioned).into_iter().flatten().flatten().map(|entry| entry.path()) {
                let msvc = edition.join("VC").join("Tools").join("MSVC");
                for version in std::fs::read_dir(&msvc).into_iter().flatten().flatten().map(|entry| entry.path()) {
                    let cl = version.join("bin").join("Hostx64").join("x64").join("cl.exe");
                    if cl.is_file() {
                        found.push(cl);
                    }
                }
            }
        }
    }
    found.sort();
    found.pop()
}

impl Programs for OnPath {
    fn program(&self, role: Role) -> Result<PathBuf, String> {
        let found = match role {
            Role::C => on_path(&["cc", "gcc", "clang"]),
            Role::Cxx => on_path(&["c++", "g++", "clang++"]),
            Role::Nvcc => on_path(&["nvcc"]),
            Role::Nvdisasm | Role::Cuobjdump => {
                let beside = if role == Role::Nvdisasm { "nvdisasm" } else { "cuobjdump" };
                self.program(Role::Nvcc)?.parent().and_then(|dir| in_folder(dir, beside))
            }
            Role::Cl => on_path(&["cl"]).or_else(cl_installed),
            Role::Python => on_path(if cfg!(windows) { &["python", "python3"] } else { &["python3", "python"] }),
            Role::NvidiaSmi => on_path(&["nvidia-smi"]),
            Role::Docker => on_path(&["docker"]),
            Role::Curl => on_path(&["curl"]),
            Role::Tar => on_path(&["tar"]),
        };
        found.ok_or_else(|| format!("{} is not found on the PATH", role.name()))
    }

    fn run_path(&self) -> OsString {
        std::env::var_os("PATH").unwrap_or_default()
    }
}

#[cfg(test)]
mod roles {
    use super::*;

    #[test]
    fn every_role_has_a_name_a_person_reads() {
        for role in Role::EVERY {
            assert!(!role.name().is_empty());
            assert!(!role.name().contains("Role"));
        }
    }

    #[test]
    fn the_c_compiler_is_found_on_the_path_here() {
        // the tree's C programs are built by cc or gcc; where neither is on the PATH the test says so
        match OnPath.program(Role::C) {
            Ok(path) => assert!(path.is_file(), "{}", path.display()),
            Err(said) => assert!(said.contains("the C compiler"), "{said}"),
        }
    }

    #[test]
    fn a_program_beside_nvcc_is_looked_for_beside_it() {
        if let Ok(nvcc) = OnPath.program(Role::Nvcc) {
            let nvdisasm = OnPath.program(Role::Nvdisasm).unwrap();
            assert_eq!(nvdisasm.parent(), nvcc.parent());
        }
    }

    #[test]
    fn what_is_not_found_is_said_by_its_name() {
        struct Nothing;
        impl Programs for Nothing {
            fn program(&self, role: Role) -> Result<PathBuf, String> {
                Err(format!("{} is not found", role.name()))
            }
            fn run_path(&self) -> OsString {
                OsString::new()
            }
        }
        assert_eq!(Nothing.program(Role::Python).unwrap_err(), "python is not found");
    }
}
