// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The run's buffer (`src/cu/types/file_defs/qry/buffer/*.c`) compiled into a static library and
//! linked into this crate, so that the crate is a member of a run: it hands into its own ring and
//! reads the buffer in place, with no process between it and the writer. The C compiler is `CC`
//! where that is set, else `clang` on a Windows build and `cc` elsewhere; the archiver is `AR`
//! where set, else `llvm-ar` on a Windows build and `ar` elsewhere.

use std::path::{Path, PathBuf};
use std::process::Command;

const BUFFER: &str = "src/cu/types/file_defs/qry/buffer";

const HEADERS: [&str; 4] = ["src/cu/types/file_defs/qry/qry_buffer.h", "src/cu/types/file_defs/qry/qry.h", "src/cu/types/file_defs/qry/qry_host.h", "src/includes/codecs/crc/crc_key.h"];

const C11: [&str; 5] = ["-std=c11", "-O2", "-Wall", "-Wextra", "-c"];

fn tree() -> PathBuf {
    let manifest = PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    manifest.join("..").join("..").join("..")
}

fn msvc() -> bool {
    std::env::var("CARGO_CFG_TARGET_ENV").is_ok_and(|env| env == "msvc")
}

fn compiler() -> String {
    std::env::var("CC").unwrap_or_else(|_| if std::env::var("CARGO_CFG_TARGET_OS").is_ok_and(|os| os == "windows") { "clang".to_string() } else { "cc".to_string() })
}

fn archiver() -> String {
    std::env::var("AR").unwrap_or_else(|_| if msvc() { "llvm-ar".to_string() } else { "ar".to_string() })
}

/// `source` compiled to `object`; the build stops with the compiler's words where it does not compile.
fn compiled(source: &Path, object: &Path) {
    let compiler = compiler();
    let mut command = Command::new(&compiler);
    command.args(C11);
    if msvc() && compiler.contains("clang") {
        command.arg("--target=x86_64-pc-windows-msvc");
    }
    command.arg("-o").arg(object).arg(source);
    let output = command.output().unwrap_or_else(|error| panic!("{compiler}: {error}"));
    if !output.status.success() {
        panic!("{} did not compile:\n{}", source.display(), String::from_utf8_lossy(&output.stderr));
    }
}

fn main() {
    let tree = tree();
    let out = PathBuf::from(std::env::var_os("OUT_DIR").expect("OUT_DIR"));
    let buffer = tree.join(BUFFER);
    println!("cargo:rerun-if-changed={}", buffer.display());
    for header in HEADERS {
        println!("cargo:rerun-if-changed={}", tree.join(header).display());
    }
    println!("cargo:rerun-if-env-changed=CC");
    println!("cargo:rerun-if-env-changed=AR");
    let library = out.join(if msvc() { "qry_buffer.lib" } else { "libqry_buffer.a" });
    let _ = std::fs::remove_file(&library);
    let mut archive = Command::new(archiver());
    archive.arg("rcs").arg(&library);
    let sources = std::fs::read_dir(&buffer).unwrap_or_else(|error| panic!("{}: {error}", buffer.display())).flatten().map(|entry| entry.path()).filter(|path| path.extension().is_some_and(|extension| extension == "c"));
    for source in sources {
        let object = out.join(source.file_name().expect("a source has a name")).with_extension(if msvc() { "obj" } else { "o" });
        compiled(&source, &object);
        archive.arg(&object);
    }
    let archived = archive.output().unwrap_or_else(|error| panic!("{}: {error}", archiver()));
    if !archived.status.success() {
        panic!("the buffer's library was not archived:\n{}", String::from_utf8_lossy(&archived.stderr));
    }
    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=qry_buffer");
    if std::env::var("CARGO_CFG_TARGET_OS").is_ok_and(|os| os == "linux") {
        println!("cargo:rustc-link-lib=pthread");
        println!("cargo:rustc-link-lib=rt");
    }
}
