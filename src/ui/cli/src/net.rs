// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Each address orior reaches over the network, asked for here first. orior reaches one only for
//! what the reader asked of it: a request sent, a database connected, a repository pulled or pushed,
//! a machine linked or deployed to, settings carried, and an error report the reader let file on its
//! own. The setting `network`, `off`, holds orior off the network entirely: each reach is refused,
//! saying what it was for. Each address reached is kept in `network.jsonl` in orior's own folder with
//! when and what for, the last KEPT of them, and Network lists them. This machine's own addresses are
//! no part of the network and are neither held nor kept.

use std::io::Write;
use std::sync::Mutex;
use std::sync::atomic::{AtomicU8, Ordering};

use serde::{Deserialize, Serialize};

/// The most addresses kept.
const KEPT: usize = 1000;

/// Whether the window holds orior off the network: 0 where the window has not said, and settings.json
/// is read; 1 where it lets orior on; 2 where it holds orior off.
static HELD: AtomicU8 = AtomicU8::new(0);

static WRITING: Mutex<()> = Mutex::new(());

/// One address reached: when, in milliseconds since 1970, the address, and what for.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Reached {
    pub at: u64,
    pub address: String,
    #[serde(rename = "for")]
    pub purpose: String,
}

/// Holds orior off the network, or lets it on, as the window's setting says.
pub fn hold(off: bool) {
    HELD.store(if off { 2 } else { 1 }, Ordering::SeqCst);
}

/// Whether orior is held off the network: as the window said, or as settings.json says where it has
/// not.
pub fn held() -> bool {
    match HELD.load(Ordering::SeqCst) {
        1 => false,
        2 => true,
        _ => crate::home::folder()
            .and_then(|folder| std::fs::read_to_string(folder.join("settings.json")).ok())
            .and_then(|text| serde_json::from_str::<serde_json::Value>(&crate::devcontainer::plain_json(&text)).ok())
            .is_some_and(|settings| settings.get("network").and_then(|value| value.as_str()) == Some("off")),
    }
}

/// The host of `address`: a URL's, an ssh address's after its user, or the address itself, without a
/// port.
pub fn host_of(address: &str) -> String {
    let rest = address.split_once("://").map_or(address, |(_, rest)| rest);
    let rest = rest.split(['/', '?', '#']).next().unwrap_or(rest);
    let rest = rest.rsplit_once('@').map_or(rest, |(_, host)| host);
    if let Some(inner) = rest.strip_prefix('[') {
        return inner.split(']').next().unwrap_or(inner).to_string();
    }
    match rest.matches(':').count() {
        1 => rest.split(':').next().unwrap_or(rest).to_string(),
        _ => rest.to_string(),
    }
}

/// Whether `host` is this machine's own.
pub fn local(host: &str) -> bool {
    let host = host.trim().to_ascii_lowercase();
    host.is_empty() || host == "localhost" || host.ends_with(".localhost") || host == "::1" || host.starts_with("127.") || host == "0.0.0.0"
}

/// Whether `address` is a folder or a file of this machine's, as a repository's remote may be.
fn path(address: &str) -> bool {
    let bytes = address.as_bytes();
    address.starts_with(['/', '\\', '.']) || address.starts_with("file://") || (bytes.len() > 2 && bytes[0].is_ascii_alphabetic() && bytes[1] == b':' && matches!(bytes[2], b'\\' | b'/'))
}

/// Asks to reach `address` for `purpose`: refused where orior is held off the network, and kept where
/// it is let on. This machine's own addresses and paths are let on and not kept.
pub fn reach(address: &str, purpose: &str) -> Result<(), String> {
    if path(address) || local(&host_of(address)) {
        return Ok(());
    }
    if held() {
        return Err(format!("orior is held off the network, and {purpose} did not reach {address}: Preferences or Help, Network, lets it on"));
    }
    keep(address, purpose);
    Ok(())
}

fn keep(address: &str, purpose: &str) {
    let Some(folder) = crate::home::folder() else { return };
    let at = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|since| since.as_millis() as u64).unwrap_or(0);
    let line = serde_json::to_string(&Reached { at, address: address.to_string(), purpose: purpose.to_string() }).unwrap_or_default();
    let _held = WRITING.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    let path = folder.join("network.jsonl");
    let _ = std::fs::create_dir_all(&folder);
    if let Ok(mut file) = std::fs::OpenOptions::new().create(true).append(true).open(&path) {
        let _ = writeln!(file, "{line}");
    }
    let text = std::fs::read_to_string(&path).unwrap_or_default();
    let lines: Vec<&str> = text.lines().collect();
    if lines.len() > KEPT * 2 {
        let _ = std::fs::write(&path, lines[lines.len() - KEPT..].join("\n") + "\n");
    }
}

/// The addresses reached, the last first, `most` of them at most.
pub fn reached(most: usize) -> Vec<Reached> {
    let text = crate::home::folder().and_then(|folder| std::fs::read_to_string(folder.join("network.jsonl")).ok()).unwrap_or_default();
    text.lines().rev().filter_map(|line| serde_json::from_str(line).ok()).take(most).collect()
}

#[cfg(test)]
mod reaching {
    use super::{host_of, local, path};

    #[test]
    fn an_address_s_host_is_read_from_a_url_an_ssh_address_and_a_host_with_its_port() {
        assert_eq!(host_of("https://api.example.com:8443/v1/items?x=1"), "api.example.com");
        assert_eq!(host_of("git@github.com:dstroy0/orior.git"), "github.com");
        assert_eq!(host_of("ssh://me@build.example.org:2222/srv"), "build.example.org");
        assert_eq!(host_of("db.example.net:5432"), "db.example.net");
        assert_eq!(host_of("[::1]:8080"), "::1");
        assert_eq!(host_of("http://127.0.0.1:3000/echo"), "127.0.0.1");
        assert!(local("127.0.0.1") && local("localhost") && local("::1") && local("tauri.localhost"));
        assert!(!local("github.com") && !local("10.0.0.5"));
        assert!(path("D:/repos/settings") && path("C:\\remote.git") && path("/srv/git/one.git") && path("../shared") && path("file:///srv/one.git"));
        assert!(!path("git@github.com:dstroy0/orior.git") && !path("https://github.com/dstroy0/orior"));
    }
}
