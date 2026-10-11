// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Plugins others publish, found in a catalog and installed from File, Plugins. A catalog is a JSON
//! file at an address the reader names, on the web or on this machine:
//!
//!   { "orior": "plugin catalog", "plugins": [ { "id", "name", "version", "about", "package",
//!     "sha256", "reads", "writes", "reaches" }, ... ] }
//!
//! and each plugin's package a JSON file, at `package`, an address of its own or one from the
//! catalog's, holding the plugin's files by name:
//!
//!   { "orior": "plugin", "files": { "plugin.json": "...", "plugin.js": "..." } }
//!
//! A package whose catalog gives its `sha256` is installed only where its bytes hash to it, and one
//! whose plugin.json names another id or version than its catalog's entry is not installed. What a
//! plugin reads, writes and reaches on the network is what its own plugin.json says
//! (plugins.rs, `asks`); the window holds it to that. Nothing is read from a catalog or installed
//! but when the reader asks, and a plugin is updated only by installing it again.

use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};
use serde_json::Value;

/// What a catalog says it is, and what a package says it is.
const CATALOG: &str = "plugin catalog";
const PACKAGE: &str = "plugin";

/// One plugin a catalog lists.
#[derive(Serialize, Deserialize, Clone, Debug, Default, PartialEq)]
#[serde(default)]
pub struct Entry {
    pub id: String,
    pub name: String,
    pub version: String,
    pub about: String,
    pub package: String,
    pub sha256: String,
    pub reads: Vec<String>,
    pub writes: Vec<String>,
    pub reaches: Vec<String>,
}

/// The text at `address`, a web address or a file of this machine, asked for `purpose`.
fn fetch(address: &str, purpose: &str) -> Result<Vec<u8>, String> {
    if address.starts_with("http://") || address.starts_with("https://") {
        crate::net::reach(address, purpose)?;
        let answer = crate::requests::send(&crate::requests::Sent { method: "GET".into(), url: address.into(), redirect: true, ..Default::default() }, true);
        if let Some(failed) = answer.failed {
            return Err(format!("{address}: {failed}"));
        }
        if !(200..300).contains(&answer.status) {
            return Err(format!("{address}: {} {}", answer.status, answer.reason));
        }
        if answer.cut {
            return Err(format!("{address} is larger than orior reads"));
        }
        return Ok(if answer.binary { crate::serve::unbase64(&answer.body).ok_or_else(|| format!("{address}: its answer does not read"))? } else { answer.body.into_bytes() });
    }
    let path = address.strip_prefix("file://").unwrap_or(address);
    std::fs::read(path).map_err(|error| format!("{path}: {error}"))
}

/// `address` read from where `base` is: as it is where it is whole, and beside `base` where not.
pub fn beside(base: &str, address: &str) -> String {
    if address.contains("://") || Path::new(address).is_absolute() {
        return address.to_string();
    }
    if base.contains("://") {
        return match base.rfind('/') {
            Some(cut) => format!("{}/{address}", &base[..cut]),
            None => address.to_string(),
        };
    }
    Path::new(base).parent().unwrap_or(Path::new("")).join(address).display().to_string()
}

/// The plugins the catalog at `address` lists.
pub fn read(address: &str) -> Result<Vec<Entry>, String> {
    let bytes = fetch(address, "the plugin catalog")?;
    let catalog: Value = serde_json::from_slice(&bytes).map_err(|error| format!("{address}: {error}"))?;
    if catalog.get("orior").and_then(Value::as_str) != Some(CATALOG) {
        return Err(format!("{address} is no catalog of orior's plugins"));
    }
    let entries: Vec<Entry> = serde_json::from_value(catalog.get("plugins").cloned().unwrap_or_default()).map_err(|error| format!("{address}: {error}"))?;
    Ok(entries.into_iter().filter(|entry| !entry.id.is_empty() && crate::plugins::slug(&entry.id) == entry.id && !entry.package.is_empty()).collect())
}

/// Installs the plugin `entry` of the catalog at `address` into the reader's plugins folder, in place
/// of the one of its id that is there: its package fetched, its hash checked where the catalog gives
/// one, and its plugin.json held to the entry's id and version. Gives the folder.
pub fn install(address: &str, entry: &Entry) -> Result<PathBuf, String> {
    let folder = crate::home::plugins().ok_or("orior has no folder of its own")?;
    install_into(&folder, address, entry)
}

fn install_into(plugins: &Path, address: &str, entry: &Entry) -> Result<PathBuf, String> {
    let from = beside(address, &entry.package);
    let bytes = fetch(&from, &format!("the plugin {}", entry.id))?;
    if !entry.sha256.is_empty() && !crate::digest::sha256(&bytes).eq_ignore_ascii_case(&entry.sha256) {
        return Err(format!("{from} is not the package the catalog names: its hash differs"));
    }
    let package: Value = serde_json::from_slice(&bytes).map_err(|error| format!("{from}: {error}"))?;
    if package.get("orior").and_then(Value::as_str) != Some(PACKAGE) {
        return Err(format!("{from} is no package of an orior plugin"));
    }
    let files: BTreeMap<String, String> = serde_json::from_value(package.get("files").cloned().unwrap_or_default()).map_err(|error| format!("{from}: {error}"))?;
    let own: Value = serde_json::from_str(files.get("plugin.json").ok_or_else(|| format!("{from} holds no plugin.json"))?).map_err(|error| format!("{from}: plugin.json: {error}"))?;
    let (id, version) = (own.get("id").and_then(Value::as_str).unwrap_or(""), own.get("version").and_then(Value::as_str).unwrap_or(""));
    if id != entry.id || version != entry.version {
        return Err(format!("{from} holds {id} {version}, and the catalog names {} {}", entry.id, entry.version));
    }
    if files.keys().any(|name| name.is_empty() || name.split(['/', '\\']).any(|part| part.is_empty() || part == "." || part == "..")) {
        return Err(format!("{from} names a file outside its plugin's folder"));
    }
    let folder = plugins.join(&entry.id);
    let staged = plugins.join(format!(".{}.installing", entry.id));
    let _ = std::fs::remove_dir_all(&staged);
    for (name, text) in &files {
        let path = staged.join(name);
        std::fs::create_dir_all(path.parent().unwrap_or(&staged)).map_err(|error| format!("{}: {error}", staged.display()))?;
        std::fs::write(&path, text).map_err(|error| format!("{}: {error}", path.display()))?;
    }
    let _ = std::fs::remove_dir_all(&folder);
    std::fs::rename(&staged, &folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    Ok(folder)
}

#[cfg(test)]
mod installing {
    use super::{beside, install_into, read, Entry};
    use std::fs;

    #[test]
    fn a_plugin_is_found_in_a_catalog_and_installed_only_as_its_entry_names_it() {
        let root = std::env::temp_dir().join(format!("orior_ui_plugin_catalog_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("packages")).unwrap();
        fs::create_dir_all(root.join("plugins")).unwrap();
        let package = serde_json::json!({ "orior": "plugin", "files": { "plugin.json": "{\"id\": \"counts\", \"version\": \"1.1.0\", \"kind\": \"script\", \"api\": 3, \"reads\": [\"*.md\"]}", "plugin.js": "orior.on(\"open\", () => {});" } }).to_string();
        fs::write(root.join("packages/counts.json"), &package).unwrap();
        let hash = crate::digest::sha256(package.as_bytes());
        let catalog = serde_json::json!({ "orior": "plugin catalog", "plugins": [
            { "id": "counts", "name": "Counts", "version": "1.1.0", "package": "packages/counts.json", "sha256": hash, "reads": ["*.md"] },
            { "id": "../out", "name": "Out", "version": "1", "package": "x.json" }
        ] });
        fs::write(root.join("catalog.json"), catalog.to_string()).unwrap();
        let address = root.join("catalog.json").display().to_string();
        let entries = read(&address).unwrap();
        assert_eq!(entries.iter().map(|one| one.id.as_str()).collect::<Vec<_>>(), ["counts"], "an id that is no folder's name is left out");
        let folder = install_into(&root.join("plugins"), &address, &entries[0]).unwrap();
        assert!(folder.join("plugin.js").is_file() && folder.join("plugin.json").is_file());
        let wrong = Entry { sha256: "00".repeat(32), ..entries[0].clone() };
        assert!(install_into(&root.join("plugins"), &address, &wrong).unwrap_err().contains("hash differs"));
        let older = Entry { version: "1.0.0".into(), sha256: String::new(), ..entries[0].clone() };
        assert!(install_into(&root.join("plugins"), &address, &older).unwrap_err().contains("the catalog names counts 1.0.0"));
        fs::write(root.join("not.json"), "{\"plugins\": []}").unwrap();
        assert!(read(&root.join("not.json").display().to_string()).unwrap_err().contains("no catalog"));
        assert_eq!(beside("https://example.com/orior/catalog.json", "p/a.json"), "https://example.com/orior/p/a.json");
        assert_eq!(beside("https://example.com/catalog.json", "https://cdn.example.com/a.json"), "https://cdn.example.com/a.json");
        fs::remove_dir_all(&root).unwrap();
    }
}
