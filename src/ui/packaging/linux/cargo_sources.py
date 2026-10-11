# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Writes cargo-sources.json beside the Flatpak's manifest: every crate of crates.io that the
window's Cargo.lock and the command line's name, each fetched by flatpak-builder from crates.io's
own address and checked against the hash its lock gives, unpacked where cargo finds it offline, and
the cargo config that sends cargo there. A Flatpak builds with no network: each crate it takes is
named here first.

Usage: python -I cargo_sources.py
"""

import json
import pathlib
import tomllib

HERE = pathlib.Path(__file__).resolve().parent
UI = HERE.parent.parent
LOCKS = [UI / "src-tauri" / "Cargo.lock", UI / "cli" / "Cargo.lock"]
REGISTRY = "registry+https://github.com/rust-lang/crates.io-index"
VENDOR = "cargo/vendor"


def crates():
    """Each crate of crates.io the locks name, by name and version, with the hash each gives."""
    found = {}
    for lock in LOCKS:
        if not lock.is_file():
            continue
        for package in tomllib.loads(lock.read_text(encoding="utf-8")).get("package", []):
            if package.get("source") == REGISTRY and package.get("checksum"):
                found[(package["name"], package["version"])] = package["checksum"]
    return sorted(found.items())


def sources():
    out = []
    for (name, version), checksum in crates():
        folder = f"{VENDOR}/{name}-{version}"
        out.append(
            {
                "type": "archive",
                "archive-type": "tar-gzip",
                "url": f"https://static.crates.io/crates/{name}/{name}-{version}.crate",
                "sha256": checksum,
                "dest": folder,
            }
        )
        out.append(
            {
                "type": "inline",
                "contents": json.dumps({"package": checksum, "files": {}}),
                "dest": folder,
                "dest-filename": ".cargo-checksum.json",
            }
        )
    out.append(
        {
            "type": "inline",
            "contents": f'[source.crates-io]\nreplace-with = "vendored"\n\n[source.vendored]\ndirectory = "{VENDOR}"\n',
            "dest": "cargo",
            "dest-filename": "config.toml",
        }
    )
    return out


def main():
    made = sources()
    (HERE / "cargo-sources.json").write_text(json.dumps(made, indent=2) + "\n", encoding="utf-8")
    print(f"{(len(made) - 1) // 2} crates written to {HERE / 'cargo-sources.json'}")


if __name__ == "__main__":
    main()
