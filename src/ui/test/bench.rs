// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The files the window's tests measure it on: files people report other editors slow to open and to
//! scroll, each pinned by where it is fetched from, its count of bytes and its SHA-256, fetched once
//! into test/target/bench and checked whole at every run, and put in each run's tree under bench/.
//!
//! - checker.ts, TypeScript's type checker, the file VS Code's own text buffer is measured on.
//! - sqlite3.c, SQLite's amalgamation, the C file VS Code measures on and tree-sitter's readers report
//!   slow to scroll.
//! - large-file.json, json-iterator's test file, the one Helix's readers measure a slow scroll on, with
//!   lines of tens of thousands of characters.

use std::path::{Path, PathBuf};
use std::process::Command;

/// Each file: its name, where it is fetched from, its count of bytes, and its SHA-256.
pub const FILES: &[(&str, &str, u64, &str)] = &[
    (
        "checker.ts",
        "https://raw.githubusercontent.com/microsoft/TypeScript/v5.9.3/src/compiler/checker.ts",
        3_121_747,
        "aba154f42352498790b55fa6995bdbb5dee607460eef5eebc1908b0cf0087d3c",
    ),
    (
        "sqlite3.c",
        "https://raw.githubusercontent.com/azadkuh/sqlite-amalgamation/3.38.2/sqlite3.c",
        8_462_111,
        "254d5579c10ae6a7425ddd055abbdecc5d7b7e6b473ddf28729cd831f25bbd8d",
    ),
    (
        "large-file.json",
        "https://raw.githubusercontent.com/json-iterator/test-data/0bce379832b475a6c21726ce37f971f8d849513b/large-file.json",
        26_141_343,
        "4fc1e52c4e609febd05d75a24c84bc6957fa4d2cfb0d5fbebbac650bdc7ed8c0",
    ),
];

/// The SHA-256 of `bytes`, as lowercase hex.
pub fn sha256(bytes: &[u8]) -> String {
    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];
    let mut state: [u32; 8] = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
    let mut padded = bytes.to_vec();
    padded.push(0x80);
    while padded.len() % 64 != 56 {
        padded.push(0);
    }
    padded.extend_from_slice(&((bytes.len() as u64) * 8).to_be_bytes());
    for block in padded.chunks_exact(64) {
        let mut words = [0u32; 64];
        for (at, word) in block.chunks_exact(4).enumerate() {
            words[at] = u32::from_be_bytes(word.try_into().expect("four bytes"));
        }
        for at in 16..64 {
            let (a, b) = (words[at - 15], words[at - 2]);
            let s0 = a.rotate_right(7) ^ a.rotate_right(18) ^ (a >> 3);
            let s1 = b.rotate_right(17) ^ b.rotate_right(19) ^ (b >> 10);
            words[at] = words[at - 16].wrapping_add(s0).wrapping_add(words[at - 7]).wrapping_add(s1);
        }
        let mut v = state;
        for at in 0..64 {
            let s1 = v[4].rotate_right(6) ^ v[4].rotate_right(11) ^ v[4].rotate_right(25);
            let choose = (v[4] & v[5]) ^ (!v[4] & v[6]);
            let first = v[7].wrapping_add(s1).wrapping_add(choose).wrapping_add(K[at]).wrapping_add(words[at]);
            let s0 = v[0].rotate_right(2) ^ v[0].rotate_right(13) ^ v[0].rotate_right(22);
            let most = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
            let second = s0.wrapping_add(most);
            v = [first.wrapping_add(second), v[0], v[1], v[2], v[3].wrapping_add(first), v[4], v[5], v[6]];
        }
        for (word, added) in state.iter_mut().zip(v) {
            *word = word.wrapping_add(added);
        }
    }
    state.iter().map(|word| format!("{word:08x}")).collect()
}

/// Whether `path` holds the file pinned at `size` bytes and `sum`.
fn whole(path: &Path, size: u64, sum: &str) -> bool {
    path.metadata().is_ok_and(|meta| meta.len() == size) && std::fs::read(path).is_ok_and(|bytes| sha256(&bytes) == sum)
}

/// The folder of the files, each fetched where it is not there whole, or what kept one from it.
pub fn fetched(here: &Path) -> Result<PathBuf, String> {
    let folder = here.join("target").join("bench");
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    for &(name, url, size, sum) in FILES {
        let path = folder.join(name);
        if whole(&path, size, sum) {
            continue;
        }
        let part = folder.join(format!("{name}.part"));
        let got = Command::new("curl").args(["-sfL", "--retry", "3", "-o"]).arg(&part).arg(url).status().map_err(|error| format!("curl: {error}"))?;
        if !got.success() || !whole(&part, size, sum) {
            let _ = std::fs::remove_file(&part);
            return Err(format!("{name} was not fetched whole from {url}"));
        }
        std::fs::rename(&part, &path).map_err(|error| format!("{}: {error}", path.display()))?;
    }
    Ok(folder)
}

/// The files put in the run's tree under bench/, each linked to the one fetched where the system
/// links it, and copied where it does not.
pub fn placed(folder: &Path, tree: &Path) -> Result<(), String> {
    let into = tree.join("bench");
    std::fs::create_dir_all(&into).map_err(|error| format!("{}: {error}", into.display()))?;
    for &(name, ..) in FILES {
        let (from, to) = (folder.join(name), into.join(name));
        if std::fs::hard_link(&from, &to).is_err() {
            std::fs::copy(&from, &to).map_err(|error| format!("{}: {error}", to.display()))?;
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_sha_256_is_the_standard_s() {
        assert_eq!(sha256(b""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        assert_eq!(sha256(b"abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert_eq!(sha256(&[b'a'; 1000]), "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
        let every: Vec<u8> = (0..300).flat_map(|_| 0..=255u8).collect();
        assert_eq!(sha256(&every), "f8b0585eb91f58c007a5634362c9f90d8543822c113f702523bc7b73408a9392");
    }
}
