// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Builds the plugins that come with orior into the program: each folder under ../plugins that holds a
// plugin.json is one, by the folder's name, written to $OUT_DIR/bundled.rs as BUNDLED, in the order
// of their names.

use std::path::Path;

fn main() {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("plugins");
    println!("cargo:rerun-if-changed={}", dir.display());
    let mut found = Vec::new();
    if let Ok(entries) = std::fs::read_dir(&dir) {
        for entry in entries.flatten() {
            let file = entry.path().join("plugin.json");
            if file.is_file() {
                println!("cargo:rerun-if-changed={}", file.display());
                let id = entry.file_name().to_string_lossy().into_owned();
                let file = dunce_free(&file);
                found.push((id, file));
            }
        }
    }
    found.sort();
    let mut text = String::from("/// The plugins that come with orior: each one's id and the text of its plugin.json.\npub const BUNDLED: &[(&str, &str)] = &[\n");
    for (id, file) in &found {
        text.push_str(&format!("    ({id:?}, include_str!({file:?})),\n"));
    }
    text.push_str("];\n");
    let out = Path::new(&std::env::var("OUT_DIR").expect("cargo sets OUT_DIR")).join("bundled.rs");
    std::fs::write(out, text).expect("OUT_DIR takes a file");
}

// The file's whole path, as include_str! takes it.
fn dunce_free(path: &Path) -> String {
    let full = path.canonicalize().unwrap_or_else(|_| path.to_path_buf());
    let text = full.to_string_lossy().into_owned();
    text.strip_prefix(r"\\?\").map(str::to_string).unwrap_or(text)
}
