// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window's build: Tauri's, then orior's C side built by ui/cli/build.sh into a library for the target and
// linked in, with the system's libraries it calls.

use std::path::PathBuf;

// the bash the PATH names first: for a bare name Windows looks in its own folders before the PATH, and the bash that
// stands there is WSL's, which reads no Windows path
fn bash() -> PathBuf {
    let name = if cfg!(windows) { "bash.exe" } else { "bash" };
    std::env::var_os("PATH")
        .and_then(|path| std::env::split_paths(&path).map(|folder| folder.join(name)).find(|found| found.is_file()))
        .unwrap_or_else(|| PathBuf::from(name))
}

fn main() {
    tauri_build::build();
    let manifest = PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").expect("cargo sets CARGO_MANIFEST_DIR"));
    let top = manifest.join("..").join("..").join("..");
    let target = std::env::var("TARGET").expect("cargo sets TARGET");
    for folder in ["ui/cli", "ui/query", "src/cu/includes/file_defs/qry", "src/cu/includes/formats/cfg_json", "src/cu/includes/parsers", "src/cu/engine/runtime/scriptura", "ui/plugins"] {
        println!("cargo:rerun-if-changed={}", top.join(folder).display());
    }
    let status = std::process::Command::new(bash())
        .arg(top.join("ui/cli/build.sh"))
        .arg("library")
        .arg(&target)
        .status()
        .expect("bash runs ui/cli/build.sh");
    if !status.success() {
        panic!("ui/cli/build.sh library {target} did not build");
    }
    println!("cargo:rustc-link-search=native={}", top.join("build/cli").join(&target).display());
    println!("cargo:rustc-link-lib=static=orior");
    let system: &[&str] = if target.contains("windows") {
        &["ws2_32", "secur32", "crypt32", "shell32", "advapi32", "user32"]
    } else if target.contains("apple") {
        &["pthread", "m"]
    } else {
        &["pthread", "dl", "m", "rt"]
    };
    for library in system {
        println!("cargo:rustc-link-lib={library}");
    }
}
