// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window's build: Tauri's, then orior's C side built by ui/cli/build.sh into a library for the target and
// linked in, with the system's libraries it calls, and the page's lattice built from C to wasm32 into the page.

use std::path::{Path, PathBuf};
use std::process::Command;

// the page's lattice, the window's own computing (ui/gui/src/lattice.c) with its GL driver (ui/gui/src-wasm), built
// to wasm32 into ui/gui/src-js/lattice/lattice.wasm, which the page loads, where clang and wasm-ld are on the PATH; a
// build with neither keeps the lattice.wasm the tree holds
fn lattice_built(top: &Path) {
    let folder = top.join("ui/gui/src-wasm");
    let computing = top.join("ui/gui/src/lattice.c");
    println!("cargo:rerun-if-changed={}", folder.display());
    println!("cargo:rerun-if-changed={}", computing.display());
    println!("cargo:rerun-if-changed={}", top.join("ui/gui/src/lattice.h").display());
    let found = |program: &str| Command::new(program).arg("--version").output().is_ok_and(|out| out.status.success());
    if !found("clang") || !found("wasm-ld") {
        println!("cargo:warning=clang and wasm-ld are not both on the PATH: the page keeps the lattice.wasm the tree holds");
        return;
    }
    let status = Command::new("clang")
        .args(["--target=wasm32", "-std=c11", "-O2", "-nostdlib", "-ffreestanding", "-Wall", "-Wextra", "-Werror"])
        .args(["-DLATTICE_WASM=1", "-Wl,--no-entry", "-o"])
        .arg(top.join("ui/gui/src-js/lattice/lattice.wasm"))
        .arg(computing)
        .arg(folder.join("lattice_gl.c"))
        .status()
        .expect("clang runs");
    if !status.success() {
        panic!("ui/gui/src-wasm did not build to wasm32");
    }
}

// the bash the PATH names first outside Windows' own folder, in a folder of the PATH or in the `bin` beside one, as Git
// puts its bash beside the `cmd` folder it adds to the PATH: the bash in Windows' own folder is WSL's, which reads no
// Windows path, and Windows looks there for a bare name before the PATH
fn bash() -> PathBuf {
    let name = if cfg!(windows) { "bash.exe" } else { "bash" };
    let system = std::env::var_os("SystemRoot").map(|root| root.to_string_lossy().to_lowercase());
    let outside = |folder: &PathBuf| system.as_ref().is_none_or(|system| !folder.to_string_lossy().to_lowercase().starts_with(system.as_str()));
    let beside = |folder: PathBuf| [folder.join(name), folder.join("..").join("bin").join(name)];
    std::env::var_os("PATH")
        .and_then(|path| std::env::split_paths(&path).filter(outside).flat_map(beside).find(|found| found.is_file()))
        .unwrap_or_else(|| PathBuf::from(name))
}

fn main() {
    tauri_build::build();
    let manifest = PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").expect("cargo sets CARGO_MANIFEST_DIR"));
    let top = manifest.join("..").join("..").join("..");
    let target = std::env::var("TARGET").expect("cargo sets TARGET");
    lattice_built(&top);
    for folder in ["ui/cli", "ui/query", "src/cu/includes/file_defs/qry", "src/cu/includes/formats/cfg_json", "src/cu/includes/parsers", "src/cu/includes/mmgr", "ui/plugins"] {
        println!("cargo:rerun-if-changed={}", top.join(folder).display());
    }
    // the C side is bitcode, which the linker the tree's .cargo/config.toml names for the target optimizes whole at the link
    let status = Command::new(bash())
        .arg(top.join("ui/cli/build.sh"))
        .arg("library")
        .arg(&target)
        .status()
        .expect("bash runs ui/cli/build.sh");
    if !status.success() {
        panic!("ui/cli/build.sh library {target} did not build");
    }
    // ORIOR_SANITIZE names a sanitizer the C side is built under, into a folder of its own, and the window links its
    // runtime
    println!("cargo:rerun-if-env-changed=ORIOR_SANITIZE");
    let sanitizer = std::env::var("ORIOR_SANITIZE").unwrap_or_default();
    let mut built = target.clone();
    if !sanitizer.is_empty() {
        built.push('-');
        built.push_str(&sanitizer);
    }
    println!("cargo:rustc-link-search=native={}", top.join("build/cli").join(&built).display());
    println!("cargo:rustc-link-lib=static=orior_cli");
    if !sanitizer.is_empty() {
        println!("cargo:rustc-link-arg=-fsanitize={sanitizer}");
    }
    let system: &[&str] = if target.contains("windows") {
        &["ws2_32", "secur32", "crypt32", "shell32", "advapi32", "user32", "ole32", "uuid"]
    } else if target.contains("apple") {
        &["pthread", "m"]
    } else {
        &["pthread", "dl", "m", "rt"]
    };
    for library in system {
        println!("cargo:rustc-link-lib={library}");
    }
}
